# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

import os
from datetime import datetime, timezone
import json
from pathlib import Path
import struct
import tempfile
from types import SimpleNamespace
import unittest

from tools.unicorn_backend import (ENTRY, IMAGE_SIZE, ConventionalMemory, GamePort, GuestMouseTrace, KeyboardController, KeyScript, MouseScript, ProtectedTimer, RealModeBridge, VgaDac, VgaStatus, VectorState,
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

    def test_keyboard_ack_handshake_matches_linux_harness(self):
        keyboard = KeyboardController()
        self.assertEqual((keyboard.read(0x64), keyboard.read(0x60)), (0, 0))
        keyboard.write(0x60, 0xed)
        self.assertEqual(keyboard.read(0x64), 1)
        self.assertEqual(keyboard.read(0x60), 0xfa)
        self.assertEqual((keyboard.read(0x64), keyboard.read(0x60)), (0, 0))
        with self.assertRaises(UnsupportedGuestOperation):
            keyboard.write(0x64, 0x20)

    def test_keyboard_reads_host_scancode_mailbox_and_advances_head(self):
        class Memory:
            def __init__(self):
                self.ram = bytearray(0x100000)
                struct.pack_into('<IHH', self.ram, 0xf1400, 0x31504e49, 255, 1)
                self.ram[0xf1507] = 0xe0
                self.ram[0xf1408] = 0x4b

            def mem_read(self, address, size):
                return bytes(self.ram[address - 0x01000000:address - 0x01000000 + size])

            def mem_write(self, address, data):
                offset = address - 0x01000000
                self.ram[offset:offset + len(data)] = data

        guest = Memory()
        keyboard = KeyboardController()
        self.assertTrue(keyboard.pending(guest))
        self.assertEqual(keyboard.read(0x64, guest), 1)
        self.assertEqual([keyboard.read(0x60, guest) for _ in range(3)], [0xe0, 0x4b, 0])
        self.assertEqual(struct.unpack_from('<H', guest.ram, 0xf1404)[0], 1)
        self.assertFalse(keyboard.pending(guest))
        keyboard.write(0x60, 0xed)
        self.assertEqual(keyboard.read(0x60, guest), 0xfa)
        struct.pack_into('<I', guest.ram, 0xf1400, 0)
        self.assertFalse(keyboard.pending(guest))

    def test_gameport_discharge_and_six_bit_vga_dac(self):
        class Memory:
            def __init__(self):
                self.ram = bytearray(0x100000)
                struct.pack_into('<I', self.ram, 0xf1400, 0x31504e49)
                struct.pack_into('<BB4H', self.ram, 0xf1600, 3, 1, 2, 1, 0, 0)

            def mem_read(self, address, size):
                start = address - 0x01000000
                return bytes(self.ram[start:start + size])

            def mem_write(self, address, data):
                start = address - 0x01000000
                self.ram[start:start + len(data)] = data

        guest = Memory()
        joystick = GamePort()
        joystick.write(guest)
        self.assertEqual([joystick.read(guest) for _ in range(3)], [0xeF, 0xeD, 0xec])
        struct.pack_into('<I', guest.ram, 0xf1400, 0)
        self.assertEqual(joystick.read(guest), 0xff)
        palette = VgaDac()
        palette.write(guest, 0x3c8, 2)
        palette.write(guest, 0x3c9, 127)
        palette.write(guest, 0x3c9, 10)
        palette.write(guest, 0x3c9, 20)
        self.assertEqual(bytes(guest.ram[0xf1006:0xf1009]), bytes((63, 10, 20)))

    def test_vga_status_switches_at_native_retrace_boundary(self):
        samples = iter((0, 14999999, 15000000, 16666666, 16666667))
        status = VgaStatus(clock=lambda: next(samples))
        self.assertEqual([status.read() for _ in range(5)], [0, 0, 8, 8, 0])

    def test_pit_ticks_and_protected_irq0_frame(self):
        import struct

        class Registers:
            def __init__(self):
                self.memory = {0x0100046c: b'\0' * 4}
                self.writes = {}

            def mem_read(self, address, size):
                return self.memory[address][:size]

            def mem_write(self, address, data):
                self.memory[address] = bytes(data)

            def reg_write(self, reg, value):
                self.writes[reg] = value

        timer = ProtectedTimer(interval=2)
        guest = Registers()
        vectors = VectorState()
        vectors.protected[8] = (0x12345, 0x2f)
        regs = dict(sp=0x4000, cs=0x2f, ss=7, flags=0x3202,
                    sp_register='SP', ip_register='IP', flags_register='FLAGS')
        timer.port_out(0x43, 0x36)
        timer.port_out(0x40, 0)
        timer.port_out(0x40, 0)
        self.assertEqual(timer.divisor, 65536)
        timer.instructions = 2
        self.assertTrue(timer.instruction(guest, 0x2345, vectors, regs))
        self.assertEqual(guest.writes, dict(SP=0x3ff4, IP=0x12345, FLAGS=0x3002))
        self.assertEqual(struct.unpack('<3I', guest.memory[0x3ff4]), (0x2345, 0x2f, 0x3202))
        self.assertEqual(struct.unpack('<I', guest.memory[0x0100046c])[0], 1)
        self.assertEqual(timer.interrupts, 1)
        timer.port_out(0x20, 0x20)
        timer.instructions = 3
        self.assertFalse(timer.instruction(guest, 0x2345, vectors, regs))
        self.assertFalse(timer.in_service)
        timer.instructions = 4
        self.assertFalse(timer.instruction(guest, 0x2345, vectors, dict(regs, flags=0x3002)))
        self.assertEqual(timer.interrupts, 1)

    def test_registered_irq1_handler_receives_queued_keyboard_input(self):
        class Cpu:
            def __init__(self):
                self.data = {0x0100046c: b'\0' * 4}
                self.registers = {}

            def mem_read(self, address, count):
                return self.data[address][:count]

            def mem_write(self, address, data):
                self.data[address] = bytes(data)

            def reg_write(self, reg, value):
                self.registers[reg] = value

        cpu = Cpu()
        timer = ProtectedTimer(interval=1)
        vectors = VectorState()
        vectors.protected[8] = (0x1000, 0x2f)
        vectors.protected[9] = (0x2000, 0x2f)
        regs = dict(sp=0x4000, cs=0x2f, ss=7, flags=0x3202,
                    sp_register='SP', ip_register='IP', flags_register='FLAGS')
        timer.instructions = 1
        self.assertTrue(timer.instruction(cpu, 0x3456, vectors, regs, keyboard_pending=True))
        self.assertEqual(cpu.registers['IP'], 0x2000)
        self.assertEqual(timer.keyboard_interrupts, 1)
        self.assertEqual(timer.interrupts, 1)

    def test_headless_mouse_script_applies_press_and_release_at_ticks(self):
        class Mouse:
            mouse_x, mouse_y, mouse_buttons = 320, 100, 0
            mouse_motion_x = mouse_motion_y = 0

            def move_mouse(self, dx, dy):
                self.mouse_motion_x += dx
                self.mouse_motion_y += dy
                self.mouse_x += dx
                self.mouse_y += dy

        mouse = Mouse()
        events = MouseScript(['20:200:0:1', '25:200:0:0', '0:320:100:0'])
        events.apply(0, mouse)
        self.assertEqual((mouse.mouse_x, mouse.mouse_y, mouse.mouse_buttons), (320, 100, 0))
        events.apply(19, mouse)
        self.assertEqual(mouse.mouse_buttons, 0)
        self.assertEqual((mouse.mouse_motion_x, mouse.mouse_motion_y), (0, 0))
        events.apply(22, mouse)
        self.assertEqual((mouse.mouse_x, mouse.mouse_y, mouse.mouse_buttons), (200, 0, 1))
        self.assertEqual((mouse.mouse_motion_x, mouse.mouse_motion_y), (-120, -100))
        events.apply(25, mouse)
        self.assertEqual(mouse.mouse_buttons, 0)
        self.assertEqual([event['tick'] for event in events.applied], [0, 20, 25])
        for bad in ('1:2:3', '-1:2:3:1', '1:640:0:0', '1:0:0:8', 'abc:0:0:0'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                MouseScript([bad])

    def test_guest_mouse_trace_groups_calls_by_wrapper_and_button_state(self):
        trace = GuestMouseTrace()
        trace.record(0xce064, 0x1234, 0, 320, 200, 100)
        trace.record(0xce064, 0x1234, 0, 320, 200, 105)
        trace.record(0xce064, 0x1234, 1, 180, 85, 110)
        trace.record(0xce02c, 0x5678, 1, 180, 85, 111)
        sites = trace.report()
        self.assertEqual([(s['kind'], s['caller'], s['buttons'], s['count']) for s in sites],
                         [('motion', 0x5678, 1, 1),
                          ('buttons', 0x1234, 0, 2),
                          ('buttons', 0x1234, 1, 1)])
        self.assertEqual((sites[1]['first_tick'], sites[1]['last_tick']), (100, 105))
        trace.record_gui_frame(180, 85, False, 120)
        trace.record_gui_frame(180, 85, True, 121)
        trace.record_gui_hit(0x123456, True, 121)
        trace.record_gui_event(3, 0x123456, True, 121)
        trace.record_gui_dispatch(3, 0x123456, 0x19, 121)
        trace.record_gui_callback(0x123456, 0x20, 0x20, 0xabcd, True, 121)
        trace.record_gui_subcontrol(0xf18a4, 0x123456, 0x20, 0x40, 0x20, 0xabcd,
                                    0x20, (1, 0, 0, 0, 0), (2, 0, 0, 0, 0),
                                    0xabcdef, 0x10, 1, 121)
        trace.record_gui_activation(0xa4b08, 0x123456, 2, 4, 0, 0xabcd, (1, 1), 121)
        trace.record_gui_window_command(0xa4b80, 7, 0x123456, 7, 4, 121)
        trace.record_gui_key(27, 121)
        self.assertEqual([row['left_down'] for row in trace.gui_report()['frames']],
                         [False, True])
        self.assertEqual(trace.gui_report()['hits'][0]['control'], 0x123456)
        self.assertEqual(trace.gui_report()['events'][0]['control_id'], 3)
        self.assertEqual(trace.gui_report()['dispatches'][0]['action'], 0x19)
        self.assertEqual(trace.gui_report()['callbacks'][0]['callback'], 0xabcd)
        self.assertEqual(trace.gui_report()['subcontrols'][0]['stage'], 0xf18a4)
        self.assertEqual(trace.gui_report()['subcontrols'][0]['open_windows'][0], 1)
        self.assertEqual(trace.gui_report()['subcontrols'][0]['child'], 0xabcdef)
        self.assertEqual(trace.gui_report()['activations'][0]['flags'], 4)
        self.assertEqual(trace.gui_report()['activations'][0]['stack_slots'], [1, 1])
        self.assertEqual(trace.gui_report()['window_commands'][0]['index'], 7)
        self.assertEqual(trace.gui_report()['keys'][0]['key'], 27)

    def test_headless_key_script_uses_sdl_scans_and_bios_queue(self):
        from tools.input_devices import HostInput
        engine = HostInput()
        script = KeyScript(['8:41:27:1', '12:41:27:0'])
        script.apply(7, engine)
        self.assertEqual(list(engine.scan_queue), [])
        script.apply(8, engine)
        self.assertEqual(engine.keys, [0x011b])  # Esc: set-1 scan 1 and ASCII 27
        self.assertEqual(list(engine.scan_queue), [1])
        script.apply(12, engine)
        self.assertEqual(list(engine.scan_queue), [1, 0x81])
        self.assertEqual([entry['tick'] for entry in script.applied], [8, 12])
        for bad in ('1:2:3', '-1:41:27:1', '1:256:27:1', '1:41:256:1', '1:41:27:2'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                KeyScript([bad])

    def test_invalid_x32_residency_subfunction_sets_carry(self):
        result = serve_startup_int21(0x2f0, dict(self.regs, ax=0x252b, bx=0x61))
        self.assertEqual(result['ax'], 0xa5a5a5a5)
        self.assertEqual(result['flags'] & 1, 1)
        self.assertEqual(result['bx'], 0x61)

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
    def test_virtual_sound_config_preserves_original_asset(self):
        from tools.unicorn_files import DosFiles
        with tempfile.TemporaryDirectory() as tmp:
            config = Path(tmp) / 'EF2000.CFG'
            config.write_bytes(b'MUSIC=1,127\r\nSOUNDFX=1,127\r\n')
            files = DosFiles(tmp, config_data=b'MUSIC=0,127\r\nSOUNDFX=0,127\r\n')
            handle = files.open('EF2000.CFG')
            self.assertEqual(files.read(handle, 40), b'MUSIC=0,127\r\nSOUNDFX=0,127\r\n')
            self.assertEqual(config.read_bytes(), b'MUSIC=1,127\r\nSOUNDFX=1,127\r\n')

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
            dotted = files.open('.\\data\\.\\TEST.DAT')
            self.assertEqual(files.read(dotted, 3), b'012')
            files.close(dotted)
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
    def test_original_style_vga_retrace_wait_exits_when_status_toggles(self):
        image = bytearray(0x195fb0)
        image[ENTRY:ENTRY + 17] = bytes.fromhex(
            'ba da 03 00 00'    # mov edx,3DAh
            'ec a8 08 74 fb'    # IN AL,DX; TEST AL,8; JZ back to IN
            'b8 00 4c 00 00'    # mov eax,4C00h
            'cd 21'             # DOS exit
        )
        samples = iter((0, 0, 15000000))
        result = run(bytes(image), instruction_limit=100,
                     vga_clock=lambda: next(samples))
        self.assertEqual(result['status'], 'guest_exit', result.get('detail'))
        self.assertEqual(result['exit_code'], 0)

    def test_headless_vga_frame_can_be_saved_as_png(self):
        image = bytearray(0x195fb0)
        image[ENTRY:ENTRY + 9] = bytes.fromhex('b8 13 00 00 00 cd 10 eb fe')
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'frame.png'
            result = run(bytes(image), instruction_limit=300, framebuffer=path)
            self.assertEqual(result['status'], 'instruction_limit', result.get('detail'))
            self.assertEqual(result['video_mode'], 0x13)
            self.assertEqual(result['video_modes'][-1]['mode'], 0x13)
            self.assertEqual(result['framebuffer'], str(path))
            self.assertEqual(path.read_bytes()[:8], b'\x89PNG\r\n\x1a\n')
            self.assertEqual(struct.unpack('>II', path.read_bytes()[16:24]), (320, 200))

    def test_headless_keys_are_written_into_guest_mailbox(self):
        # The guest probes BIOS keyboard once, then loops while a scripted
        # make/break pair reaches the same mailbox used by live SDL input.
        image = bytearray(0x195fb0)
        image[ENTRY:ENTRY + 9] = bytes.fromhex('b8 01 01 00 00 cd 16 eb fe')
        result = run(bytes(image), instruction_limit=300, timer_instructions=20,
                     key_events=['1:41:27:1', '2:41:27:0'])
        self.assertEqual(result['status'], 'instruction_limit', result.get('detail'))
        self.assertEqual([e['down'] for e in result['key_events']], [True, False])

    def test_windowed_display_pumps_events_and_closes_safely(self):
        displays = []

        class FakeDisplay:
            def __init__(self, **settings):
                self.settings = settings
                self.closed = False
                self.menu = SimpleNamespace(open=False)
                self.updates = 0
                self.closed_called = False
                displays.append(self)

            def poll(self, engine):
                self.engine = engine
                engine.key_event(4, ord('a'), True)

            def update(self, engine, memory):
                self.updates += 1
                self.snapshot = memory
                if self.updates == 2:
                    self.closed = True

            def close(self):
                self.closed_called = True

        image = bytearray(0x195fb0)
        image[ENTRY:ENTRY + 2] = bytes.fromhex('eb fe')
        result = run(bytes(image), instruction_limit=0, timer_instructions=100,
                     windowed=True, display_factory=FakeDisplay, display_instructions=10,
                     geometry=(1280, 720), aspect_ratio='native', smooth_scaling=True)
        self.assertEqual(result['status'], 'window_closed', result.get('detail'))
        self.assertEqual(result['instruction_limit'], 0)
        self.assertEqual(displays[0].updates, 2)
        self.assertEqual(len(displays[0].snapshot), 0x100000)
        self.assertEqual(struct.unpack_from('<IHH', displays[0].snapshot, 0xf1400),
                         (0x31504e49, 0, 1))
        self.assertEqual(displays[0].snapshot[0xf1408], 0x1e)
        self.assertTrue(displays[0].closed_called)
        self.assertEqual(displays[0].settings['geometry'], (1280, 720))
        self.assertEqual(displays[0].settings['aspect_ratio'], 'native')
        self.assertTrue(displays[0].settings['smooth_scaling'])

    def test_synthetic_guest_receives_irq0_and_returns_through_iret(self):
        image = bytearray(0x195fb0)
        code = bytes.fromhex(
            '0e 1f'                # push cs; pop ds
            'b8 04 25 00 00'      # mov eax,2504h: protected vector set
            'b9 08 00 00 00'      # mov ecx,8: IRQ0
            'ba 00 02 00 00'      # mov edx,200h: handler
            'cd 21 fb eb fe'      # INT 21h; STI; jmp $
        )
        image[ENTRY:ENTRY + len(code)] = code
        image[0x200:0x206] = bytes.fromhex('50 b0 20 e6 20 58')
        image[0x206] = 0xcf  # IRET
        result = run(bytes(image), instruction_limit=5000, timer_instructions=100)
        self.assertEqual(result['status'], 'instruction_limit', result.get('detail'))
        self.assertGreater(result['timer']['irq0'], 5)
        self.assertEqual(result['timer']['ticks'], 50)

    def test_portable_real_mode_bridge_updates_registers_and_guest_memory(self):
        class GuestMemory:
            def __init__(self):
                self.ram = bytearray(0x100000)
                self.frame = bytearray(struct.pack('<5H2I', 0x10, 0, 0, 0, 0, 0x4f00, 0))

            def mem_read(self, address, size):
                if address == 0x2000:
                    return bytes(self.frame[:size])
                if address == 0x01000000:
                    return bytes(self.ram[:size])
                raise AssertionError((address, size))

            def mem_write(self, address, data):
                if address == 0x2000:
                    self.frame[:] = data
                elif address == 0x01000000:
                    self.ram[:] = data
                else:
                    raise AssertionError(address)

        import struct
        guest = GuestMemory()
        regs = dict(ax=0x2511, bx=0, cx=0, dx=0x2000, si=0, di=0,
                    ds=7, es=7, flags=0x202)
        bridge = RealModeBridge()
        response = bridge.execute(guest, regs, [0] * 256)
        self.assertEqual(response['ax'], 0x4f)
        self.assertEqual(bridge.requests[0]['vector'], 0x10)
        self.assertEqual(guest.ram[0:4], b'VESA')
        guest.frame[:] = struct.pack('<5H2I', 0x66, 0, 0, 0, 0, 0, 0)
        guest.ram[0x10000:0x10004] = bytes.fromhex('b8 34 12 cf')
        response = bridge.execute(guest, regs, [0x10000000 if i == 0x66 else 0
                                                for i in range(256)])
        self.assertEqual(response['ax'], 0x1234)
        self.assertEqual(struct.unpack_from('<I', guest.frame, 10)[0], 0)
        video = bridge.bios(guest, dict(regs, ax=0x13), [0] * 256, 0x10)
        self.assertEqual(video['ax'], 0x13)
        self.assertEqual(bridge.engine.video_mode, 0x13)

    def test_synthetic_original_instruction_execution(self):
        image = bytearray(0x195fb0)
        # Real x86 instructions, not a mocked Unicorn API.
        image[ENTRY:ENTRY + 8] = bytes.fromhex('66 b8 34 12 cd 21 eb fe')
        result = run(bytes(image), instruction_limit=50)
        self.assertEqual(result['status'], 'unsupported_interface')
        self.assertEqual(result['events'][0]['ip'], ENTRY + 4)
        self.assertEqual(result['events'][0]['ax'], 0x1234)
        self.assertEqual(result['events'][0]['ds'], 7)

    def test_dos_file_results_and_instruction_sampling_are_reported(self):
        image = bytearray(0x195fb0)
        image[ENTRY:ENTRY + 14] = bytes.fromhex('b8 00 3d 00 00 ba 00 03 00 00 cd 21 eb fe')
        image[0x300:0x30c] = b'MISSING.DAT\0'
        with tempfile.TemporaryDirectory() as directory:
            result = run(bytes(image), game_dir=directory, instruction_limit=250,
                         sample_instructions=50)
        self.assertEqual(result['status'], 'instruction_limit', result.get('detail'))
        self.assertEqual(result['events'][-1]['filename'], 'MISSING.DAT')
        self.assertTrue(result['events'][-1]['carry'])
        self.assertEqual(result['events'][-1]['result_ax'], 2)
        self.assertEqual(len(result['ip_samples']), 5)
        self.assertTrue(all(entry['ip'] == ENTRY + 12 for entry in result['ip_samples'][1:]))
        with self.assertRaises(ValueError):
            run(bytes(image), sample_instructions=-1)

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
        self.assertEqual(result['video_mode'], 0x13)
        self.assertEqual([entry['mode'] for entry in result['video_modes']], [3, 0x13])
        self.assertTrue(any(entry['vector'] == 0x10 for entry in result['real_mode_requests']))
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
            self.assertEqual(result['video_mode'], 0x13)
            self.assertFalse(any('VIEWS2.DAT' in line.upper() for line in result['console']))
            self.assertFalse((Path(os.environ['EF2000_DATA_DIR']) / 'TACTCOM.$$$').exists())
