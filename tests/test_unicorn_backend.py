# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

import os
from datetime import datetime, timezone
import json
from pathlib import Path
import tempfile
import unittest

from tools.unicorn_backend import (ENTRY, IMAGE_SIZE, ConventionalMemory, VectorState,
                                   X32Selectors, UnsupportedGuestOperation, run,
                                   serve_startup_int21, startup_memory)
from tools.unicorn_files import DosFileError, ReadOnlyDosFiles
from tools.x32_allocation import Allocation


class UnicornStartupServiceTests(unittest.TestCase):
    def setUp(self):
        self.regs = dict(ax=0x3505, bx=4, cx=0x260, dx=0, si=0, di=0,
                         bp=0, sp=0x3fff000, ip=0x95, flags=0x202)

    def test_portable_psp_and_environment_match_native_boot(self):
        writes = dict(startup_memory(bytes(ENTRY + 2)))
        self.assertEqual(writes[0x02000080], b'\x04 gui\r')
        self.assertIn(b'DID=.\0quick_flag=1\0\0', writes[0x02001000])

    def test_first_x32_selector_limit_request(self):
        result = serve_startup_int21(0x93, self.regs)
        self.assertEqual(result['cx'], 0x260)
        self.assertEqual(result['flags'] & 0x41, 0x40)
        self.assertEqual(self.regs['flags'], 0x202)

    def test_dos_version_request(self):
        regs = dict(self.regs, ax=0x3005, bx=0x1234, cx=0x12345678)
        result = serve_startup_int21(0xa0, regs)
        self.assertEqual((result['ax'], result['bx'], result['cx']),
                         (5, 0x1200, 0x12340000))

    def test_unknown_interface_stops(self):
        with self.assertRaises(UnsupportedGuestOperation):
            serve_startup_int21(0x19a, dict(self.regs, ax=0x3509))

    def test_stack_and_heap_services(self):
        allocation = Allocation()
        request = dict(self.regs, ax=0x3509, bx=0x3ffef00, cx=0x4000)
        response = serve_startup_int21(0x19a, request, allocation)
        self.assertEqual(response['dx'], allocation.stack_end - allocation.free_bytes - 1)
        self.assertEqual(response['flags'] & 1, 0)
        grown = serve_startup_int21(0x245, dict(request, ax=0x3508, cx=0x4e0000,
                                                  flags=response['flags']), allocation)
        self.assertEqual(grown['cx'], 0x4e0000)
        self.assertEqual(allocation.heap_end, 0x4e0000)
        self.assertEqual(grown['flags'] & 1, 0)

    def test_python_allocator_matches_original_dosbox_x_reference(self):
        source = Path(__file__).resolve().parent.parent / 'docs/x32-reference.json'
        reference = json.loads(source.read_text())['record']
        state = reference['allocation_before']
        allocation = Allocation(heap_end=state['heap_end'], stack_end=state['stack_end'],
                                free_bytes=state['free'], minimum_stack=state['minimum_stack'],
                                heap_ceiling=0x80000000)
        request = reference['3509']['input']
        limit, flags = allocation.reserve_stack(request['ebx'], request['ecx'],
                                                 request['eflags'])
        self.assertEqual((limit, flags), (reference['3509']['output']['edx'],
                                           reference['3509']['output']['eflags']))
        self.assertEqual(allocation.free_bytes, reference['allocation_after']['free'])
        heap = reference['3508']
        self.assertEqual(allocation.grow_heap(0x4e0000, heap['input_flags']),
                         (heap['eax'], heap['ecx'], heap['eflags']))
        self.assertEqual(allocation.free_bytes, heap['free'])

    def test_allocation_failure_does_not_mutate_state(self):
        allocation = Allocation()
        before = vars(allocation).copy()
        with self.assertRaises(UnsupportedGuestOperation):
            serve_startup_int21(0x19a, dict(self.regs, ax=0x3509, bx=0x100,
                                              cx=0x1000), allocation)
        self.assertEqual(vars(allocation), before)

    def test_utc_clock_and_date(self):
        now = datetime(2026, 11, 1, 12, 34, 56, 789000, timezone.utc)
        clock = serve_startup_int21(0x111, dict(self.regs, ax=0x2c00), now=now)
        self.assertEqual((clock['ax'], clock['cx'], clock['dx']),
                         (0x2c00, 0x0c22, 0x384e))
        date = serve_startup_int21(0x111, dict(self.regs, ax=0x2a00), now=now)
        self.assertEqual((date['ax'] & 255, date['cx'], date['dx']),
                         (0, 2026, 0x0b01))

    def test_vector_install_and_query(self):
        vectors = VectorState()
        regs = dict(self.regs, ax=0x2504, cx=9, dx=0x12345, ds=0x2c, es=4)
        vectors.service(regs)
        query = vectors.service(dict(regs, ax=0x2502))
        self.assertEqual((query['bx'], query['es']), (0x12345, 0x2c))
        self.assertEqual(vectors.protected[8][0], IMAGE_SIZE + 0x100)

    def test_conventional_memory_bounds(self):
        ram = ConventionalMemory()
        allocated = ram.allocate(dict(self.regs, bx=0x100))
        self.assertEqual((allocated['ax'], allocated['bx']), (0x1000, 0x1010000))
        self.assertEqual(ram.end, 0x11000)
        failed = ram.allocate(dict(self.regs, bx=0xffff))
        self.assertEqual((failed['ax'], failed['flags'] & 1), (8, 1))
        self.assertEqual(ram.end, 0x11000)

    def test_image_validation(self):
        with self.assertRaises(ValueError):
            startup_memory(b'')
        with self.assertRaises(ValueError):
            startup_memory(b'\0' * (IMAGE_SIZE + 1))

    def test_dynamic_selector_descriptor_update(self):
        class Memory:
            def __init__(self):
                self.changes = []

            def mem_write(self, address, data):
                self.changes.append((address, data))

        memory = Memory()
        selectors = X32Selectors(memory, 0x4000100)
        regs = dict(self.regs, ax=0x3501, bx=0)
        allocated = selectors.service(regs)
        self.assertEqual(allocated['bx'], 0x37)
        selectors.service(dict(regs, ax=0x3503, bx=0x37, cx=0x1234000))
        query = selectors.service(dict(regs, ax=0x3504, bx=0x37))
        self.assertEqual(query['cx'], 0x1234000)
        self.assertEqual(memory.changes[-1][0], 0x4000130)
        selectors.service(dict(regs, ax=0x3502, bx=0x37))
        self.assertEqual(memory.changes[-1][1], b'\0' * 8)


class DosFileBridgeTests(unittest.TestCase):
    def test_case_insensitive_read_seek_close(self):
        with tempfile.TemporaryDirectory() as tmp:
            subdir = Path(tmp) / 'Data'
            subdir.mkdir()
            (subdir / 'Test.Dat').write_bytes(b'0123456789')
            files = ReadOnlyDosFiles(tmp)
            handle = files.open('C:\\EF2000\\DATA\\TEST.DAT')
            self.assertEqual(handle, 5)
            self.assertEqual(files.read(handle, 3), b'012')
            self.assertEqual(files.seek(handle, -2, 2), 8)
            self.assertEqual(files.read(handle, 4), b'89')
            files.close(handle)
            with self.assertRaises(DosFileError) as failure:
                files.read(handle, 1)
            self.assertEqual(failure.exception.code, 6)

    def test_prevents_host_writes_and_path_traversal(self):
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp) / 'GAME.DAT').write_bytes(b'game')
            files = ReadOnlyDosFiles(tmp)
            for filename in ('../GAME.DAT', 'D:/GAME.DAT', 'C:/../GAME.DAT'):
                with self.subTest(filename=filename), self.assertRaises(DosFileError):
                    files.open(filename)
            with self.assertRaises(DosFileError) as failure:
                files.open('GAME.DAT', access=1)
            self.assertEqual(failure.exception.code, 5)
            self.assertEqual((Path(tmp) / 'GAME.DAT').read_bytes(), b'game')

    def test_cd_root_is_distinct_from_game_data(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'game').mkdir()
            (root / 'cd').mkdir()
            (root / 'game' / 'LOCAL.DAT').write_bytes(b'game')
            (root / 'cd' / 'CHECK.CD').write_bytes(b'cd')
            files = ReadOnlyDosFiles(root / 'game', root / 'cd')
            self.assertEqual(files.read(files.open('local.dat'), 4), b'game')
            self.assertEqual(files.read(files.open('\\CHECK.CD'), 2), b'cd')
            with self.assertRaises(DosFileError):
                files.open('check.cd')
            without_cd = ReadOnlyDosFiles(root / 'game')
            with self.assertRaises(DosFileError):
                without_cd.open('\\CHECK.CD')

    def test_writable_overlay_copies_original_on_first_write(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            game, overlay = root / 'game', root / 'state'
            game.mkdir()
            original = game / 'CONFIG.DAT'
            original.write_bytes(b'abcdefgh')
            files = ReadOnlyDosFiles(game, overlay_root=overlay)
            handle = files.open('config.dat', access=2)
            self.assertEqual(files.read(handle, 3), b'abc')
            self.assertEqual(files.write(handle, b'XY'), 2)
            files.close(handle)
            self.assertEqual(original.read_bytes(), b'abcdefgh')
            self.assertEqual((overlay / 'EF2000' / 'CONFIG.DAT').read_bytes(), b'abcXYfgh')
            handle = files.open('config.dat')
            self.assertEqual(files.read(handle, 8), b'abcXYfgh')
            files.close(handle)

    def test_create_seek_and_absolute_drive_overlay(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            game, overlay = root / 'game', root / 'state'
            game.mkdir()
            files = ReadOnlyDosFiles(game, overlay_root=overlay)
            handle = files.create('C:\\TEST.$$$')
            self.assertEqual(files.write(handle, b'hi'), 2)
            self.assertEqual(files.seek(handle, 4, 0), 4)
            self.assertEqual(files.write(handle, b'!'), 1)
            files.flush(handle)
            self.assertEqual((overlay / 'TEST.$$$').read_bytes(), b'hi\0\0!')
            files.close(handle)
            reopened = files.open('C:\\TEST.$$$')
            self.assertEqual(files.read(reopened, 5), b'hi\0\0!')

    def test_readonly_files_refuse_mutation_without_overlay(self):
        with tempfile.TemporaryDirectory() as tmp:
            original = Path(tmp) / 'DATA.DAT'
            original.write_bytes(b'original')
            files = ReadOnlyDosFiles(tmp)
            with self.assertRaises(DosFileError):
                files.create('new.dat')
            with self.assertRaises(DosFileError):
                files.open('data.dat', access=1)
            handle = files.open('data.dat')
            with self.assertRaises(DosFileError):
                files.write(handle, b'changed')
            self.assertEqual(original.read_bytes(), b'original')


try:
    import unicorn  # noqa: F401
except ImportError:
    HAS_UNICORN = False
else:
    HAS_UNICORN = True


@unittest.skipUnless(HAS_UNICORN, 'requires Unicorn Python bindings')
class UnicornGuestTests(unittest.TestCase):
    def test_synthetic_original_instruction_execution(self):
        image = bytearray(0x195fb0)
        # Real x86 instructions, not a mocked Unicorn API.
        image[ENTRY:ENTRY + 8] = bytes.fromhex('66 b8 34 12 cd 21 eb fe')
        result = run(bytes(image), instruction_limit=50)
        self.assertEqual(result['status'], 'unsupported_interface')
        self.assertEqual(result['events'][0]['ip'], ENTRY + 4)
        self.assertEqual(result['events'][0]['ax'], 0x1234)
        self.assertEqual(result['events'][0]['ds'], 7)

    @unittest.skipUnless(os.environ.get('EF2000_EXE'), 'requires original EF2000_EXE')
    def test_original_extracted_code_reaches_file_open(self):
        from tools.extract_image import extract
        image = extract(Path(os.environ['EF2000_EXE']).read_bytes())[1]['payload.bin']
        result = run(image)
        self.assertEqual(result['status'], 'unsupported_interface')
        self.assertEqual([(e['ip'], e['ax'] & 0xffff) for e in result['events'][:4]],
                         [(0x93, 0x3505), (0xa0, 0x3005),
                          (0x19a, 0x3509), (0x245, 0x3508)])
        self.assertEqual(result['events'][-1]['ax'] & 0xffff, 0x3d00)
        self.assertEqual(result['events'][-1]['ip'], 0xfa4db)
        self.assertEqual(result['allocation']['heap_end'], 0x4e0000)
        self.assertTrue(result['startup_checkpoint']['bss_zero'])
        self.assertEqual(result['startup_checkpoint']['cx'], 0x4000)
        self.assertEqual(result['startup_checkpoint']['si'], 0x4df000)
        self.assertEqual(result['startup_checkpoint']['di'], 0x4de2f0)

    @unittest.skipUnless(os.environ.get('EF2000_DATA_DIR') and os.environ.get('EF2000_EXE'),
                         'requires original game assets')
    def test_original_game_reads_archives_without_separate_cd_files(self):
        from tools.extract_image import extract
        image = extract(Path(os.environ['EF2000_EXE']).read_bytes())[1]['payload.bin']
        result = run(image, instruction_limit=30000000, game_dir=os.environ['EF2000_DATA_DIR'])
        self.assertEqual(result['status'], 'instruction_limit')
        self.assertTrue(any('did1st.dat' == e.get('filename') for e in result['events']))
        self.assertTrue(any('did.dat' in e.get('filename', '') for e in result['events']))
        self.assertTrue(any('TACTCOM' in line for line in result['console']))
        self.assertFalse(any('Datenset nicht gefunden' in line for line in result['console']))

    @unittest.skipUnless(os.environ.get('EF2000_CD_DIR') and
                         os.environ.get('EF2000_DATA_DIR') and os.environ.get('EF2000_EXE'),
                         'requires original game assets and a diagnostic CD marker')
    def test_cd_marker_does_not_override_archive_startup(self):
        from tools.extract_image import extract
        executable = Path(os.environ['EF2000_EXE'])
        image = extract(executable.read_bytes())[1]['payload.bin']
        with tempfile.TemporaryDirectory() as overlay:
            result = run(image, game_dir=os.environ['EF2000_DATA_DIR'],
                         cd_dir=os.environ['EF2000_CD_DIR'], overlay_dir=overlay)
            self.assertEqual(result['status'], 'instruction_limit')
            self.assertFalse(any('VIEWS2.DAT' in line.upper() for line in result['console']))
            self.assertFalse((Path(os.environ['EF2000_DATA_DIR']) / 'TACTCOM.$$$').exists())
