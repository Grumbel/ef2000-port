# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Synthetic ring-3 capture restoration, including expand-down stack segments."""

from dataclasses import replace
import copy
import ctypes
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest

from tools import asm_capture, asm_c_differential, call_capture


def fixture():
    ram = bytearray(0x20000)
    # Reload SS after restoration: this requires actual CPL=3, not just CS RPL=3.
    ram[0x1000:0x1007] = bytes.fromhex('66b807008ed0c3')
    ram[0x18000:0x18004] = struct.pack('<I', 0x1100)
    code = 0x00cffb000000ffff
    data = 0x0040f70000000100  # Expand-down, 32-bit, lower limit 0x100.
    ldt = struct.pack('<QQ', data, code)
    ldt_descriptor = 15 | (0x30100 << 16) | (0x82 << 40)
    gdt = struct.pack('<QQ', 0, ldt_descriptor)
    tables = {'gdtr': dict(selector=0, base=0x30000, limit=15, flags=0, bytes=gdt),
              'ldtr': dict(selector=8, base=0x30100, limit=15, flags=0, bytes=ldt),
              'descriptors': dict(cs=code, ss=data, ds=data, es=data, fs=0, gs=0)}
    regs = dict(eax=11, ebx=22, ecx=33, edx=44, esi=55, edi=66, ebp=77,
                esp=0x18000, eip=0x1000, eflags=0x246, cs=15, ss=7, ds=7, es=7, fs=0, gs=0)
    x87 = dict(x87_control=0x37f, x87_status=5 << 11, x87_tag=0xffff,
               x87_ip=0x1234, x87_cs=15, x87_dp=0x2345, x87_ds=7, x87_opcode=0x321,
               x87=[[i, 0x80000000, 0x3fff+i] for i in range(8)])
    return call_capture.CapturedCall(bytes(32), 0x1000, 0, regs, bytes(ram),
                                     2, x87, tables, 0x02010400)


class AsmCaptureTests(unittest.TestCase):
    @unittest.skipUnless(all(shutil.which(tool) for tool in ('cc', 'as', 'objcopy', 'objdump')),
                         'requires C compiler and GNU binutils')
    def test_descriptor_scalar_paths_compare_full_captured_state(self):
        from tools import asm_to_c, asm_x87_oracle
        from tests.test_asm_to_c import assemble
        sources = [
            'mov eax, dword ptr ds:[0x2000]\nadd dword ptr ss:[esp+4], eax\nret',
            'mov eax, dword ptr [ebp]\nmov edx, dword ptr [eax+ebp*1]\nret',
            'lea eax, fs:[ebp+4]\nret',  # LEA never accesses null FS.
            'mov eax, cs:[0x100]\nmov word ptr ds:[0x2004], ax\nret',
            'mov eax, fs:[0x2000]\nret',
            'mov eax,0x10\nadd eax,0x20\nmov edx,[0x2000]\nret',
            'pusha\nxor eax,eax\nxor edx,edx\npopa\nret',
            'mov ebx,0x2000\nmov eax,0x12340000\nxlatb\nret',
            'mov ebx,0x2000\nmov eax,0x12340000\nfs xlatb\nret',
            'mov ebx,0x100\nmov eax,0x12340000\ncs xlatb\nret',
            'push eax\npop edx\nret',
            'push ax\npop dx\nret',
            'push dword ptr ds:[0x2000]\npop dword ptr ss:[esp+4]\nret',
            'push esp\npop esp\nret',
            'push eax\npop dword ptr ds:[0x2000]\nret',
            'push ebp\nmov ebp,esp\nsub esp,16\nleave\nret',
            'push ebp\nmov ebp,esp\nleave\nret',
            'mov esi,0x2000\nmov edi,0x2200\nmov ecx,4\nrep movsb\nret',
            'mov esi,0x2000\nmov edi,0x2200\nmov ecx,4\nrep movsd\nret',
            'mov edi,0x2200\nmov ecx,4\nrep stosw\nret',
            'mov esi,0x2000\nlodsd\nret',
            'mov esi,0x2000\nmov edi,0x2200\nmov ecx,4\nrepe cmpsb\nret',
            'mov edi,0x2200\nmov ecx,4\nrepne scasb\nret',
            'mov esi,0x2003\nmov edi,0x2203\nmov ecx,4\nstd\nrep movsb\ncld\nret',
            'mov esi,0x2000\nmov edi,0x2200\nmov ecx,4\nfs rep movsb\nret',
            'mov esi,0\nmov edi,0\nmov ecx,0\nrep movsd\nret',
            'fninit\nfld1\nfchs\nfabs\nfldz\nfxch st(1)\nfstp st(1)\nfnstsw ax\nret',
            'fninit\nfnstcw word ptr [0x2000]\nfldcw word ptr [0x2000]\nfnclex\nfwait\nret',
            'fld tbyte ptr [0x2000]\nret',
            'fninit\nfild word ptr [0x2000]\nfstp tbyte ptr [0x2200]\nret',
            'fninit\nfld dword ptr [0x2000]\nfstp tbyte ptr [0x2200]\nret',
            'fninit\nfld tbyte ptr fs:[0x2000]\nfstp tbyte ptr [0x2200]\nret',
            'fninit\nfldl2e\nfstp tbyte ptr [0x2200]\nret',
            'fninit\nfldlg2\nfstp tbyte ptr [0x2200]\nret',
            'fninit\nfldln2\nfstp tbyte ptr [0x2200]\nfldpi\nfldl2t\nret',
            'enter 16,0\nmov eax,[ebp]\nleave\nret',
            'enter 0,0\nleave\nret',
            'bsr eax,dword ptr [0x2000]\nbsf dx,ax\nret',
            'xor eax,eax\nbsr edx,eax\nbsf dx,ax\nret',
        ]
        segment_push_start = len(sources)
        sources += [f'{push} {segment}\npop {destination}\nret'
                    for push, destination in (('push', 'eax'), ('pushw', 'ax'))
                    for segment in ('es', 'cs', 'ss', 'ds', 'fs', 'gs')]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for number, source in enumerate(sources):
                with self.subTest(source=source):
                    image, record = assemble(root, source)
                    capture = fixture()
                    memory = bytearray(capture.guest_ram)
                    memory[:image.stat().st_size] = image.read_bytes()
                    memory[0x2000:0x2004] = struct.pack('<I', 0x10)
                    memory[0x2100:0x2104] = struct.pack('<I', 0x2000)
                    if number >= segment_push_start:
                        memory[0x17ff0:0x18000] = bytes([0xa5])*16
                    regs = dict(capture.registers, eip=0x100, ebp=0x2100)
                    tables = copy.deepcopy(capture.table_context)
                    # Non-null FS for the explicit override load.
                    if number in (4, 8, 24, 31):
                        regs['fs'] = 7
                        tables['descriptors']['fs'] = tables['descriptors']['ds']
                    # Execute-only CS permits instruction fetch, not data loads.
                    if number == 5:
                        tables['descriptors']['cs'] &= ~(2 << 40)
                        data = bytearray(tables['ldtr']['bytes'])
                        data[8:16] = struct.pack('<Q', tables['descriptors']['cs'])
                        tables['ldtr']['bytes'] = bytes(data)
                    capture = replace(capture, address=0x100, registers=regs, guest_ram=bytes(memory),
                                      table_context=tables)
                    (root/'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
                    (root/'lift.c').write_text(asm_to_c.translate(record))
                    library_path = root/('scalar%d.so' % number)
                    subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-std=c99',
                                    '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
                                    str(root/'lift.c'), '-o', str(library_path)], check=True, capture_output=True)
                    library = ctypes.CDLL(str(library_path))
                    function = library.ef_lift_00000100
                    function.argtypes = [ctypes.POINTER(asm_c_differential.State), ctypes.c_uint32]
                    function.restype = ctypes.c_uint
                    native = None
                    if number >= 26:
                        native, _ = asm_x87_oracle.build(root/('native%d' % number))
                    result = asm_c_differential.isolated_probes(function, b'', 0x100, 1, 100, False,
                                                               native_x87=native, captured=capture)[0]
                    self.assertEqual(result['state'], 'pass', result)
                    if number in (0, 6, 7, 10, 14, 15, 16, 17, 28, 29) or number >= segment_push_start:
                        # Test the generated access path, not only the helper:
                        # A scalar, XLAT or stack access outside its limit must stop before
                        # changing EAX, flags, or any RAM.
                        ram = bytearray(capture.guest_ram)
                        native_ram = (ctypes.c_uint8 * len(ram)).from_buffer(ram)
                        state = asm_c_differential.State(
                            (ctypes.c_uint32 * 8)(*[regs[n] for n in asm_to_c.REGS]),
                            regs['eflags'], 0x100, 0, native_ram, len(ram))
                        state.segment[:] = [regs[n] for n in asm_to_c.SEGMENTS]
                        state.segment_profile = 1
                        from tools import asm_segments
                        for index, name in enumerate(asm_to_c.SEGMENTS):
                            state.segment_cache[index] = asm_c_differential.Segment(
                                *asm_segments.descriptor(regs[name], tables['descriptors'][name]))
                        if number in (0, 7, 14):
                            state.segment_cache[3].limit = 0x2002 if number == 0 else 0x2000
                        if number == 6:
                            state.segment_cache[2].limit = regs['esp'] - 32
                        if number == 10 or number >= segment_push_start:
                            width = 2 if record['instructions'][0]['mnemonic'] == 'pushw' else 4
                            state.segment_cache[2].limit = regs['esp'] - width
                        if number in (28, 29):
                            # Reject before x87 stack/status or memory changes.
                            state.segment_cache[3].limit = 0x2000 if number == 28 else 0x2200
                            if number == 29:
                                state.pc = next(i['address'] for i in record['instructions']
                                                if i['mnemonic'] == 'fstp')
                        if number == 17:
                            # A failing REP iteration must not consume its count.
                            state.pc = next(i['address'] for i in record['instructions']
                                            if i['mnemonic'] == 'rep')
                            state.r[6], state.r[7], state.r[1] = 0x2000, 0x2200, 4
                            state.segment_cache[3].limit = 0x2000
                        if number in (15, 16):
                            # Isolate LEAVE with a failing saved-frame read.
                            state.pc = next(i['address'] for i in record['instructions']
                                            if i['mnemonic'] == 'leave')
                            if number == 15:
                                state.r[5] = 0x100  # Expand-down lower boundary.
                            else:
                                state.r[5] = len(ram)-2  # RAM extent crossing.
                        if number == 14:
                            # Isolate POP: source SS succeeds, destination DS fails.
                            state.pc = record['instructions'][1]['address']
                        if number == 7:
                            # Skip the setup MOVs to isolate fault atomicity.
                            state.pc = record['instructions'][2]['address']
                            state.r[3], state.r[0] = 0x2000, 0x12340000
                        x87_before = (bytes(state.x87), tuple(getattr(state, n) for n in asm_c_differential.X87_FIELDS))
                        before = (list(state.r), state.flags, bytes(ram))
                        expected_fault = 3 if number == 16 else 7
                        self.assertEqual(function(ctypes.byref(state), 100), expected_fault)
                        self.assertEqual(state.fault, expected_fault)
                        self.assertEqual((list(state.r), state.flags, bytes(ram)), before)
                        self.assertEqual((bytes(state.x87), tuple(getattr(state, n) for n in asm_c_differential.X87_FIELDS)), x87_before)
                        if number == 6:
                            # Reject POPA before changing any of its seven outputs.
                            state.fault = 0
                            state.pc = next(i['address'] for i in record['instructions']
                                            if i['mnemonic'] == 'popa')
                            state.segment_cache[2].limit = regs['esp'] + 28
                            self.assertEqual(function(ctypes.byref(state), 100), 7)
                            self.assertEqual((list(state.r), state.flags, bytes(ram)), before)


    @unittest.skipUnless(shutil.which('cc') and shutil.which('objdump'), 'requires C compiler and objdump')
    def test_captured_input_reaches_generated_c_without_flattening_selectors(self):
        from tools import asm_inventory, asm_to_c
        capture = fixture()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image = root/'fixture.bin'
            image.write_bytes(capture.guest_ram)
            instructions = asm_inventory.decode_window(image, 0x1000, 7)
            record = dict(address=0x1000, boundary='ghidra-body-ranges',
                          ghidra_instruction_layout_matches=True, overlapping_entries=[],
                          undecodable_addresses=[], ranges=[[0x1000, 0x1007]], instructions=instructions)
            (root/'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
            (root/'lift.c').write_text(asm_to_c.translate(record))
            subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-std=c99',
                            '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
                            str(root/'lift.c'), '-o', str(root/'lift.so')], check=True, capture_output=True)
            library = ctypes.CDLL(str(root/'lift.so'))
            function = library.ef_lift_00001000
            function.argtypes = [ctypes.POINTER(asm_c_differential.State), ctypes.c_uint32]
            function.restype = ctypes.c_uint
            results = asm_c_differential.isolated_probes(function, b'', 0x1000, 1, 100, False,
                                                        captured=capture)
            self.assertEqual(results[0]['state'], 'mismatch')
            self.assertIn('C status 7', results[0]['differences'][0])
            # Segment reloads require table lookup even for the same selector.
            # Until implemented, retain this explicit failure with real selectors.
            memory = bytearray(capture.guest_ram)
            memory[0x1000:0x1002] = b'\x0f\x0b'  # Original UD2 fault.
            faulting = replace(capture, guest_ram=bytes(memory))
            result = asm_c_differential.isolated_probes(function, b'', 0x1000, 1, 100, False,
                                                       captured=faulting)[0]
            self.assertEqual(result['state'], 'inconclusive')
            self.assertIn('original fault:', result['reason'])

    @unittest.skipUnless(all(shutil.which(tool) for tool in ('cc', 'as', 'objcopy', 'objdump')),
                         'requires C compiler and GNU binutils')
    def test_null_data_selector_reload(self):
        from tools import asm_to_c, asm_segments
        from tests.test_asm_to_c import assemble
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for index, segment in enumerate(asm_to_c.SEGMENTS):
                if segment in ('cs', 'ss'):
                    continue
                for selector in range(4):
                    for form in ('mov', 'pop', 'popw', 'far16', 'far32'):
                        with self.subTest(segment=segment, selector=selector, form=form):
                            if form == 'mov':
                                source = 'mov eax,%d\nmov %s,ax\nret' % (selector, segment)
                            elif form.startswith('far'):
                                source = 'l%s %s,cs:[0x2000]\nret' % (
                                    segment, 'ax' if form == 'far16' else 'eax')
                            else:
                                source = '%s %d\n%s %s\nret' % (
                                    'pushw' if form == 'popw' else 'push',
                                    selector if form == 'popw' else selector | 0xabcd0000, form, segment)
                            image, record = assemble(root, source)
                            capture = fixture()
                            ram = bytearray(capture.guest_ram)
                            ram[:image.stat().st_size] = image.read_bytes()
                            if form.startswith('far'):
                                pointer = (struct.pack('<HH', 0x1234, selector) if form == 'far16' else
                                           struct.pack('<IH', 0x87654321, selector))
                                ram[0x2000:0x2000+len(pointer)] = pointer
                            regs = dict(capture.registers, eip=0x100, eax=0xdeadbeef)
                            tables = copy.deepcopy(capture.table_context)
                            regs[segment] = 7
                            tables['descriptors'][segment] = tables['descriptors']['ds']
                            capture = replace(capture, address=0x100, registers=regs,
                                              guest_ram=bytes(ram), table_context=tables)
                            (root/'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
                            (root/'lift.c').write_text(asm_to_c.translate(record) + '''
    unsigned null_read(ef_lift_state *s, unsigned segment) {
        (void)ef_read_segment(s, segment, 0x2000u, 32);
        return s->fault;
    }
    ''')
                            library_path = root/('null%d_%d_%s.so' % (index, selector, form))
                            subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-std=c99',
                                            '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
                                            str(root/'lift.c'), '-o', str(library_path)],
                                           check=True, capture_output=True)
                            library = ctypes.CDLL(str(library_path))
                            function = library.ef_lift_00000100
                            function.argtypes = [ctypes.POINTER(asm_c_differential.State), ctypes.c_uint32]
                            function.restype = ctypes.c_uint
                            result = asm_c_differential.isolated_probes(
                                function, b'', 0x100, 1, 100, False, captured=capture)[0]
                            self.assertEqual(result['state'], 'pass', result)
                            memory = (ctypes.c_uint8 * len(ram)).from_buffer(ram)
                            state = asm_c_differential.State(
                                (ctypes.c_uint32 * 8)(*[regs[n] for n in asm_to_c.REGS]),
                                regs['eflags'], 0x100, 0, memory, len(ram))
                            state.segment[:] = [regs[n] for n in asm_to_c.SEGMENTS]
                            state.segment_profile = 1
                            for n, name in enumerate(asm_to_c.SEGMENTS):
                                state.segment_cache[n] = asm_c_differential.Segment(
                                    *asm_segments.descriptor(regs[name], tables['descriptors'][name]))
                            self.assertEqual(function(ctypes.byref(state), 100), 0)
                            cache = state.segment_cache[index]
                            self.assertEqual((cache.selector, cache.base, cache.limit,
                                              cache.access, cache.flags), (selector, 0, 0, 0, 0))
                            null_read = library.null_read
                            null_read.argtypes = [ctypes.POINTER(asm_c_differential.State), ctypes.c_uint]
                            null_read.restype = ctypes.c_uint
                            before = (list(state.r), state.flags, bytes(memory))
                            self.assertEqual(null_read(ctypes.byref(state), index), 7)
                            self.assertEqual((list(state.r), state.flags, bytes(memory)), before)
                            if form.startswith('far'):
                                width = 2 if form == 'far16' else 4
                                for fault in ('segment', 'memory', 'reload'):
                                    state.fault = 0
                                    state.pc = 0x100
                                    state.r[0] = 0xdeadbeef
                                    state.segment_cache[1].limit = (0x2000 + width if fault == 'segment'
                                                                    else 0xffffffff)
                                    state.memory_size = (0x2000 + width + 1 if fault == 'memory' else len(ram))
                                    memory[0x2000 + width] = 7 if fault == 'reload' else selector
                                    before = (list(state.r), list(state.segment), state.flags,
                                              bytes(state.segment_cache), bytes(memory))
                                    self.assertEqual(function(ctypes.byref(state), 1),
                                                     3 if fault == 'memory' else 7)
                                    self.assertEqual((list(state.r), list(state.segment), state.flags,
                                                      bytes(state.segment_cache), bytes(memory)), before)
                            if form in ('pop', 'popw'):
                                # A bad stack extent must fault before changing
                                # the selector/cache or advancing ESP.
                                state.fault = 0
                                state.pc = next(i['address'] for i in record['instructions']
                                                if i['mnemonic'] in ('pop', 'popw'))
                                state.r[4] = regs['esp']
                                state.segment_cache[2].limit = regs['esp']
                                before = (list(state.r), list(state.segment), state.flags,
                                          bytes(state.segment_cache), bytes(memory))
                                self.assertEqual(function(ctypes.byref(state), 1), 7)
                                self.assertEqual((list(state.r), list(state.segment), state.flags,
                                                  bytes(state.segment_cache), bytes(memory)), before)
                                # Only the selector word is fetched, even
                                # when POP advances the 32-bit stack by four.
                                state.fault = 0
                                state.segment_cache[2].access = 0xf3
                                state.segment_cache[2].limit = regs['esp'] + 1
                                memory[regs['esp']] = selector
                                memory[regs['esp'] + 1] = 0
                                self.assertEqual(function(ctypes.byref(state), 1), 1)
                                self.assertEqual(state.r[4], regs['esp'] + (2 if form == 'popw' else 4))
                                self.assertEqual(state.segment[index], selector)

    def test_restore_privilege_memory_tables_and_physical_x87(self):
        from unicorn import UcError, x86_const
        capture = fixture()
        cpu = asm_capture.restore(capture)
        self.assertEqual(bytes(cpu.mem_read(0, len(capture.guest_ram))), capture.guest_ram)
        self.assertEqual(asm_c_differential.capture_x87(cpu), capture.x87)
        for name in ('gdtr', 'ldtr'):
            table = capture.table_context[name]
            self.assertEqual(bytes(cpu.mem_read(table['base'], len(table['bytes']))), table['bytes'])
            self.assertEqual(cpu.reg_read(getattr(x86_const, 'UC_X86_REG_'+name.upper()))[:3],
                             tuple(table[k] for k in ('selector', 'base', 'limit')))
        with self.assertRaises(UcError):
            cpu.mem_read(0x10000000, 1)
        cpu.emu_start(0x1000, 0x1100, count=3)
        self.assertEqual(cpu.reg_read(x86_const.UC_X86_REG_EIP), 0x1100)
        self.assertEqual(cpu.reg_read(x86_const.UC_X86_REG_SS), 7)
        self.assertEqual(cpu.reg_read(x86_const.UC_X86_REG_ESP), 0x18004)

    def test_unsupported_context_is_rejected(self):
        capture = fixture()
        with self.assertRaisesRegex(ValueError, 'EFCAP002'):
            asm_capture.restore(replace(capture, format_version=1))
        for change, message in (
                (lambda c: c.table_context['gdtr'].update(bytes=None), 'complete'),
                (lambda c: c.table_context['ldtr'].update(base=0x10000000), 'overlaps'),
                (lambda c: c.table_context['descriptors'].update(ds=None), 'unavailable'),
                (lambda c: c.table_context['descriptors'].update(ds=0x0040f70000010100), 'zero-base'),
                (lambda c: c.registers.update(cs=12), 'ring-3'),
                (lambda c: c.registers.update(eflags=0x202 | (1 << 17)), 'VM86'),
                (lambda c: c.registers.update(eflags=0x202 | (1 << 14)), 'nested-task')):
            current = copy.deepcopy(capture)
            change(current)
            with self.subTest(message=message), self.assertRaisesRegex(ValueError, message):
                asm_capture.restore(current)


if __name__ == '__main__':
    unittest.main()
