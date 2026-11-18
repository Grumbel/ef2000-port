# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Synthetic direct-call, return-stack and dependency-closure evidence."""

import ctypes
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from tools import asm_c_program, asm_c_differential, asm_inventory
from tests.test_asm_to_c import assemble, unicorn


def inventory(root, source):
    image, _ = assemble(root, source)
    symbols = subprocess.run(['nm', '-S', '--defined-only', str(root / 'fixture.o')],
                             check=True, capture_output=True, text=True).stdout
    records = []
    directory = root / 'inventory'
    directory.mkdir()
    entries = []
    for line in symbols.splitlines():
        parts = line.split()
        if len(parts) != 4 or parts[2] not in ('t', 'T'):
            continue
        address, length = int(parts[0], 16), int(parts[1], 16)
        instructions = asm_inventory.decode_window(image, address, length)
        record = dict(address=address, boundary='ghidra-body-ranges',
                      ghidra_instruction_layout_matches=True, overlapping_entries=[],
                      undecodable_addresses=[], ranges=[[address, address+length]],
                      instructions=instructions)
        records.append(record)
        name = '%08x.json' % address
        (directory / name).write_text(json.dumps(record))
        entries.append(dict(address=address, file=name))
    manifest = dict(schema=3, boundary='ghidra-body-ranges',
                    payload_sha256=hashlib.sha256(image.read_bytes()).hexdigest(), entries=entries)
    (directory / 'manifest.json').write_text(json.dumps(manifest))
    return image, directory, records


@unittest.skipUnless(all(shutil.which(tool) for tool in ('as', 'objcopy', 'nm', 'cc')),
                     'requires GNU binutils and a C compiler')
class ProgramTests(unittest.TestCase):
    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_prepared_ram_covers_high_runtime_globals(self):
        source = '''.type root,@function
root: mov DWORD PTR [0x484480],eax
inc DWORD PTR [0x484484]
ret
.size root, .-root'''
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, directory, _ = inventory(root, source)
            out = root/'out'
            asm_c_program.generate(directory, out, image)
            subprocess.run(['cc', '-O2', '-shared', '-fPIC', '-I'+str(out),
                            *map(str, out.glob('*.c')), '-o', str(root/'program.so')],
                           check=True, capture_output=True)
            function = ctypes.CDLL(str(root/'program.so')).ef_lift_run
            function.argtypes = [ctypes.POINTER(asm_c_differential.State),
                                 ctypes.c_uint32, ctypes.c_uint32]
            args = (function, image.read_bytes(), 0x100,
                    [5, 2, 3, 4, 0x300000, 6, 7, 8], 0x246)
            self.assertEqual(asm_c_differential.compare(*args, program=True,
                             prepared_memory_size=0x500000)['state'], 'pass')
            self.assertEqual(asm_c_differential.compare(*args, program=True)['state'],
                             'inconclusive')
            for size in (0, 0x3ff000, 0x400001, 0x10001000, True):
                with self.assertRaises(ValueError):
                    asm_c_differential.compare(*args, prepared_memory_size=size)
            with self.assertRaisesRegex(ValueError, 'payload exceeds'):
                asm_c_differential.compare(function, bytes(0x400001), *args[2:])

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_address16_wraps_offsets_and_preserves_register_high_halves(self):
        from dataclasses import replace
        from tests.test_asm_capture import fixture
        expressions = ['bx', 'bp', 'si', 'di', 'bx+si', 'bx+di', 'bp+si', 'bp+di',
                       'fs:bx', 'fs:si+0x100']
        for expression in expressions:
            prefix, address = expression.split(':') if ':' in expression else ('', expression)
            memory = (prefix+':' if prefix else '') + '[%s]'
            source = '''.type root,@function
root: mov ebx,0xabcdfff0
mov ebp,0xdeadfff8
mov esi,0x12340220
mov edi,0x56780230
mov eax,DWORD PTR %s
mov DWORD PTR %s,ecx
lea edx,[%s]
ret
.size root, .-root''' % (memory % address, memory % (address+'+4'), address)
            with self.subTest(expression=expression), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                image, directory, _ = inventory(root, source)
                asm_c_program.generate(directory, root/'out', image)
                report = asm_c_differential.verify(root/'out', root/'evidence', image, scenarios=4)
                self.assertEqual(report['entries'][0]['counts'], {'pass': 4}, report)
                function = ctypes.CDLL(str(root/'evidence/translations.so')).ef_lift_run
                function.argtypes = [ctypes.POINTER(asm_c_differential.State),
                                     ctypes.c_uint32, ctypes.c_uint32]
                capture = fixture()
                context = dict(capture.table_context)
                context['descriptors'] = dict(context['descriptors'], fs=context['descriptors']['ds'])
                ram = bytearray(capture.guest_ram)
                ram[:image.stat().st_size] = image.read_bytes()
                capture = replace(capture, address=0x100,
                                  registers=dict(capture.registers, eip=0x100, fs=7),
                                  table_context=context, guest_ram=bytes(ram))
                result = asm_c_differential.isolated_probes(
                    function, b'', 0x100, 1, 100, True, captured=capture)[0]
                self.assertEqual(result['state'], 'pass', result)

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_data16_and_register_segment_prefixes(self):
        from dataclasses import replace
        from tests.test_asm_capture import fixture
        sources = [
            '.byte 0x66,0x8c,0x05\n.long 0x2000\nret',
            '.byte 0x66,0x8c,0x1d\n.long 0x2000\nret',
            'mov eax,0x2000\n.byte 0x66,0x64,0x8c,0x00\nret',
            'mov eax,0x2000\n.byte 0x66,0x64,0x8c,0x58,0x06\nret',
            'mov esi,0x2000\nmov edi,0x2200\nmov ecx,4\n.byte 0x66\nrep movsb\nret',
            '.byte 0x3e\nmov ecx,0x12345678\nret',
            '.byte 0x2e,0x8b,0xc0\nret',
            '.byte 0x3e,0x89,0xd8\nret',
        ]
        for source in sources:
            with self.subTest(source=source), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                image, directory, _ = inventory(root, '.type root,@function\nroot: '+source+
                                                 '\n.size root, .-root')
                asm_c_program.generate(directory, root/'out', image)
                report = asm_c_differential.verify(root/'out', root/'evidence', image, scenarios=8)
                self.assertEqual(report['entries'][0]['counts'], {'pass': 8}, report)
                function = ctypes.CDLL(str(root/'evidence/translations.so')).ef_lift_run
                function.argtypes = [ctypes.POINTER(asm_c_differential.State),
                                     ctypes.c_uint32, ctypes.c_uint32]
                capture = fixture()
                context = dict(capture.table_context)
                context['descriptors'] = dict(context['descriptors'], fs=context['descriptors']['ds'])
                ram = bytearray(capture.guest_ram)
                ram[:image.stat().st_size] = image.read_bytes()
                ram[0x2000:0x2008] = b'abcdefgh'
                capture = replace(capture, address=0x100,
                                  registers=dict(capture.registers, eip=0x100, fs=7),
                                  table_context=context, guest_ram=bytes(ram))
                result = asm_c_differential.isolated_probes(
                    function, b'', 0x100, 1, 100, True, captured=capture)[0]
                self.assertEqual(result['state'], 'pass', result)

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_data16_cs_segment_load(self):
        # Flat-profile DS=0 reload; the source is explicitly read through CS.
        source = '.type root,@function\nroot: .byte 0x66,0x2e,0x8e,0x1d\n.long 0x2000\nret\n.size root, .-root'
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, directory, _ = inventory(root, source)
            asm_c_program.generate(directory, root/'out', image)
            report = asm_c_differential.verify(root/'out', root/'evidence', image, scenarios=8)
            self.assertEqual(report['entries'][0]['counts'], {'pass': 8}, report)

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_push_flags_widths(self):
        from dataclasses import replace
        from tests.test_asm_capture import fixture
        for instruction, register in [('pushf', 'eax'), ('pushfw', 'ax')]:
            with self.subTest(instruction=instruction), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                source = '.type root,@function\nroot: %s\npop %s\nret\n.size root, .-root' % (instruction, register)
                image, directory, _ = inventory(root, source)
                asm_c_program.generate(directory, root/'out', image)
                report = asm_c_differential.verify(root/'out', root/'evidence', image, scenarios=16)
                self.assertEqual(report['entries'][0]['counts'], {'pass': 16}, report)
                function = ctypes.CDLL(str(root/'evidence/translations.so')).ef_lift_run
                function.argtypes = [ctypes.POINTER(asm_c_differential.State), ctypes.c_uint32, ctypes.c_uint32]
                for flags in (0x202, 0xad7, 0x200202):
                    capture = fixture()
                    ram = bytearray(capture.guest_ram)
                    ram[:image.stat().st_size] = image.read_bytes()
                    capture = replace(capture, address=0x100,
                                      registers=dict(capture.registers, eip=0x100, eflags=flags), guest_ram=bytes(ram))
                    result = asm_c_differential.isolated_probes(function, b'', 0x100, 1, 100, True, captured=capture)[0]
                    self.assertEqual(result['state'], 'pass', result)

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_pop_flags_privilege_and_width(self):
        from dataclasses import replace
        from tests.test_asm_capture import fixture
        for width in (16, 32):
            for value in (0, 0x3000, 0x244ed5, 0x180000, 0x10000, 0xfffffeff):
                with self.subTest(width=width, value=value), tempfile.TemporaryDirectory() as temporary:
                    root = Path(temporary)
                    source = '.type root,@function\nroot: mov eax,0x%x\npush %s\npopf%s\nret\n.size root, .-root' % (value, 'ax' if width == 16 else 'eax', 'w' if width == 16 else '')
                    image, directory, _ = inventory(root, source)
                    asm_c_program.generate(directory, root/'out', image)
                    report = asm_c_differential.verify(root/'out', root/'evidence', image, scenarios=4)
                    self.assertEqual(report['entries'][0]['counts'], {'pass': 4}, report)
                    function = ctypes.CDLL(str(root/'evidence/translations.so')).ef_lift_run
                    function.argtypes = [ctypes.POINTER(asm_c_differential.State), ctypes.c_uint32, ctypes.c_uint32]
                    for flags in (0x202, 0x3202):
                        capture = fixture()
                        ram = bytearray(capture.guest_ram)
                        ram[:image.stat().st_size] = image.read_bytes()
                        capture = replace(capture, address=0x100,
                                          registers=dict(capture.registers, eip=0x100, eflags=flags), guest_ram=bytes(ram))
                        result = asm_c_differential.isolated_probes(function, b'', 0x100, 1, 100, True, captured=capture)[0]
                        self.assertEqual(result['state'], 'pass', result)

    def test_legacy_notrack_rejects_other_encodings(self):
        for encoded, text in [('ff6576', 'jmp DWORD PTR [ebp+0x76]'),
                              ('3eff5576', 'call DWORD PTR [ebp+0x76]'),
                              ('3effe0', 'jmp eax'),
                              ('3eff6576', 'jmp WORD PTR [ebp+0x76]'),
                              ('3eff6576', 'jmp DWORD PTR ss:[ebp+0x76]')]:
            with self.subTest(encoded=encoded, text=text), self.assertRaises(ValueError):
                asm_c_program.legacy_transfer(dict(address=0x100, bytes=encoded,
                                                  mnemonic='notrack', operands=text))

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_legacy_notrack_jump_uses_ds_for_ebp_memory(self):
        from dataclasses import replace
        from tests.test_asm_capture import fixture
        source = '''.type root,@function
root: mov dword ptr [0x400],0x200
mov ebp,0x400
.byte 0x3e
jmp dword ptr [ebp]
.size root, .-root
.org 0x200
.type leaf,@function
leaf: inc edx
ret
.size leaf, .-leaf'''
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, directory, records = inventory(root, source)
            generated = asm_c_program.generate(directory, root/'out', image)
            entry = next(e for e in generated['entries'] if e['address'] == 0x100)
            self.assertEqual(len(entry['dynamic_transfer_sites']), 1)
            report = asm_c_differential.verify(root/'out', root/'evidence', image, scenarios=8)
            self.assertTrue(all(e['counts'] == {'pass': 8} for e in report['entries']), report)
            function = ctypes.CDLL(str(root/'evidence/translations.so')).ef_lift_run
            function.argtypes = [ctypes.POINTER(asm_c_differential.State),
                                 ctypes.c_uint32, ctypes.c_uint32]
            capture = fixture()
            ram = bytearray(capture.guest_ram)
            ram[:image.stat().st_size] = image.read_bytes()
            # Separate LDT entries: [0x400] fails through expand-down SS,
            # whose lower limit is 0x600, but succeeds through DS (0x100).
            context = dict(capture.table_context)
            descriptors = dict(context['descriptors'])
            data = descriptors['ds']
            descriptors['ss'] = descriptors['es'] = (data & ~0xffff) | 0x600
            context['descriptors'] = descriptors
            context['ldtr'] = dict(context['ldtr'], limit=23,
                                  bytes=b''.join(v.to_bytes(8, 'little') for v in
                                      (descriptors['ss'], descriptors['cs'], data)))
            gdt = bytearray(context['gdtr']['bytes'])
            gdt[8:10] = (23).to_bytes(2, 'little')
            context['gdtr'] = dict(context['gdtr'], bytes=bytes(gdt))
            capture = replace(capture, address=0x100,
                              registers=dict(capture.registers, eip=0x100, ds=23),
                              table_context=context, guest_ram=bytes(ram))
            result = asm_c_differential.isolated_probes(
                function, b'', 0x100, 1, 100, True, captured=capture)[0]
            self.assertEqual(result['state'], 'pass', result)

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_inline_data_continuation_consumed_by_pop_ret(self):
        for body, supported in [('pop edi\nret', True),
                                ('pop esp\nret', False),
                                ('nop\nret', False),
                                ('pop edi\nret 4', False)]:
            source = '''.type root,@function
root: call accessor
.size root, .-root
.byte 0x0f,0x0b,0xcc,0xff
.org 0x200
.type accessor,@function
accessor: %s
.size accessor, .-accessor''' % body
            with self.subTest(body=body), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                image, directory, records = inventory(root, source)
                codes, _, closed, reasons = asm_c_program.analyse(records)
                self.assertEqual(0x100 in closed, supported, reasons)
                if supported:
                    # Keep the actual CALL write and POP/RET operations: replacing
                    # these with a constant assignment loses observable stack RAM.
                    self.assertIn('0x105u', codes[0x100])
                    out = root/'out'
                    asm_c_program.generate(directory, out, image)
                    report = asm_c_differential.verify(
                        out, root/'evidence', image, scenarios=8)
                    entry = next(e for e in report['entries'] if e['address'] == 0x100)
                    self.assertEqual(entry['counts'], {'pass': 8}, entry)

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_descriptor_calls_preserve_target_and_stack_order(self):
        from dataclasses import replace
        from tests.test_asm_capture import fixture
        from tools import asm_segments, asm_to_c
        programs = [
            'call leaf\nret',
            'mov eax,0x200\ncall eax\nret',
            'mov dword ptr [esp-4],0x200\ncall dword ptr [esp-4]\nret',
            'push 0x200\ncall dword ptr [esp]\nadd esp,4\nret',
        ]
        for index, program in enumerate(programs):
            source = '.type root,@function\nroot: ' + program + '\n.size root, .-root\n' + '''
.org 0x200
.type leaf,@function
leaf: inc edx
ret
.size leaf, .-leaf'''
            with self.subTest(index=index), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                image, directory, records = inventory(root, source)
                out = root/'out'
                asm_c_program.generate(directory, out, image)
                asm_c_differential.verify(out, root/'evidence', image, scenarios=1)
                function = ctypes.CDLL(str(root/'evidence/translations.so')).ef_lift_run
                function.argtypes = [ctypes.POINTER(asm_c_differential.State),
                                     ctypes.c_uint32, ctypes.c_uint32]
                capture = fixture()
                ram = bytearray(capture.guest_ram)
                ram[:image.stat().st_size] = image.read_bytes()
                regs = dict(capture.registers, eip=0x100)
                capture = replace(capture, address=0x100, registers=regs, guest_ram=bytes(ram))
                result = asm_c_differential.isolated_probes(
                    function, b'', 0x100, 1, 100, True, captured=capture)[0]
                self.assertEqual(result['state'], 'pass', result)
                if index == 0:
                    for segment, limit, pc in ((2, regs['esp']-4, 0x100),
                                               (1, 0x1ff, 0x100),
                                               (2, regs['esp'], 0x201),
                                               (1, 0x201, 0x201)):
                        native_ram = (ctypes.c_uint8 * len(ram)).from_buffer(ram)
                        state = asm_c_differential.State(
                            (ctypes.c_uint32 * 8)(*[regs[n] for n in asm_to_c.REGS]),
                            regs['eflags'], pc, 0, native_ram, len(ram))
                        state.segment[:] = [regs[n] for n in asm_to_c.SEGMENTS]
                        state.segment_profile = 1
                        for i, name in enumerate(asm_to_c.SEGMENTS):
                            state.segment_cache[i] = asm_c_differential.Segment(
                                *asm_segments.descriptor(regs[name], capture.table_context['descriptors'][name]))
                        state.segment_cache[segment].limit = limit
                        before = (list(state.r), state.flags, state.pc, bytes(ram))
                        self.assertEqual(function(ctypes.byref(state), 100, 0x1100), 7)
                        self.assertEqual(state.fault, 7)
                        self.assertEqual((list(state.r), state.flags, state.pc, bytes(ram)), before)

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_indirect_register_memory_stack_aliases_and_jump_tables(self):
        programs = [
            'mov eax,0x200\ncall eax\nret',
            'mov dword ptr [0x300400],0x200\ncall dword ptr [0x300400]\nret',
            'push 0x200\ncall dword ptr [esp]\nadd esp,4\nret',
            'mov dword ptr [esp-4],0x200\ncall dword ptr [esp-4]\nret',
            'mov ebx,esp\nmov esp,0x200\ncall esp\nmov esp,ebx\nret',
            'mov eax,0x200\njmp eax',
            'mov dword ptr [0x300400],0x200\nmov dword ptr [0x300404],0x300\n'
            'and eax,1\njmp dword ptr [eax*4+0x300400]',
        ]
        for index, program in enumerate(programs):
            source = '.type root,@function\nroot: ' + program + '\n.size root, .-root\n' + '''
.org 0x200
.type first,@function
first: inc edx
ret
.size first, .-first
.org 0x300
.type second,@function
second: dec edx
ret
.size second, .-second'''
            with self.subTest(index=index), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                image, directory, _ = inventory(root, source)
                out = root/'out'
                generated = asm_c_program.generate(directory, out, image)
                caller = next(e for e in generated['entries'] if e['address'] == 0x100)
                self.assertTrue(caller['dynamic_transfer_sites'])
                self.assertEqual(caller['state'], 'generated-unverified')
                self.assertIn('direct-edges-only', generated['closure_scope'])
                report = asm_c_differential.verify(out, root/'evidence', image, scenarios=16)
                self.assertTrue(all(e['counts'] == {'pass': 16} for e in report['entries']), report)

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_untranslated_indirect_target_stops_without_original_fallback(self):
        source = '''.type root,@function
root: mov eax,0x200
call eax
ret
.size root, .-root
.org 0x200
.type unsupported,@function
unsupported: cpuid
ret
.size unsupported, .-unsupported'''
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, directory, _ = inventory(root, source)
            out = root/'out'
            generated = asm_c_program.generate(directory, out, image)
            self.assertEqual(next(e for e in generated['entries'] if e['address'] == 0x200)['state'], 'unsupported')
            report = asm_c_differential.verify(out, root/'evidence', image, scenarios=2)
            self.assertEqual(report['entries'][0]['counts'], {'mismatch': 2})
            self.assertTrue(all('C status 2' in p['differences'][0] for p in report['entries'][0]['probes']))
            self.assertEqual([p['translated_stop'] for p in report['entries'][0]['probes']],
                             [dict(status=2, pc=0x200, fault=0, registered_pc=False)] * 2)

    def test_indirect_widths_and_far_transfers_still_fail_closed(self):
        for mnemonic, operand in [('call', 'ax'), ('jmp', 'WORD PTR [eax]'),
                                  ('call', 'FWORD PTR [eax]')]:
            with self.subTest(mnemonic=mnemonic, operand=operand), self.assertRaises(ValueError):
                asm_c_program.instruction_c(dict(address=0x100, bytes='ffff', mnemonic=mnemonic,
                                                operands=operand), {0x100, 0x102})

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_changed_instruction_bytes_are_rejected_even_when_effects_match(self):
        # Replacing NOP with CLC after CLC leaves the same final registers,
        # flags and RAM. The static-code precondition still must be enforced.
        source = '''.type root,@function
root: clc
mov byte ptr [patched], 0xf8
patched: nop
ret
.size root, .-root'''
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, directory, records = inventory(root, source)
            out = root / 'out'
            asm_c_program.generate(directory, out, image)
            report = asm_c_differential.verify(out, root / 'evidence', image, scenarios=16)
            self.assertEqual(report['entries'][0]['counts'], {'mismatch': 16})
            self.assertTrue(all('C status 6 (guest instruction bytes differ from translated code)'
                                in p['differences'][0]
                                for p in report['entries'][0]['probes']))

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_patched_imul_and_accumulator_add_immediates_match(self):
        source = '''.type root,@function
root: mov ecx, 3
mov dword ptr [full+2], 0x19
mov byte ptr [short_imm+2], 0xfb
mov word ptr [bias+2], 0x1234
full: .byte 0x69, 0xc1, 0x78, 0x56, 0x34, 0x12
short_imm: .byte 0x6b, 0xc0, 0x03
bias: .byte 0x66, 0x05, 0x78, 0x56
ret
.size root, .-root'''
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, directory, _ = inventory(root, source)
            out = root / 'out'
            asm_c_program.generate(directory, out, image)
            report = asm_c_differential.verify(out, root / 'evidence', image, scenarios=16)
            self.assertEqual(report['entries'][0]['counts'], {'pass': 16}, report['entries'][0]['probes'][0])

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_composed_call_frames_tail_jumps_and_recursion_match_all_ram(self):
        programs = [
            '''.type root,@function
root: push 35
call middle
add esp, 4
ret
.size root, .-root
.org 0x200
.type middle,@function
middle: enter 16, 0
mov [0x300100], esp
call leaf
mov ecx, [ebp+8]
add eax, ecx
leave
ret
.size middle, .-middle
.org 0x300
.type leaf,@function
leaf: mov eax, esp
mov [0x300104], eax
add eax, 17
ret
.size leaf, .-leaf''',
            '''.type root,@function
root: mov ecx, 6
call recursive
ret
.size root, .-root
.org 0x200
.type recursive,@function
recursive: inc eax
dec ecx
je 1f
call recursive
1: ret
.size recursive, .-recursive''',
            '''.type root,@function
root: jmp middle
.size root, .-root
.org 0x200
.type middle,@function
middle: add eax, 17
jmp leaf
.size middle, .-middle
.org 0x300
.type leaf,@function
leaf: sub edx, 3
ret
.size leaf, .-leaf''',
            '''.type root,@function
root: mov ecx, 6
call first
ret
.size root, .-root
.org 0x200
.type first,@function
first: inc eax
dec ecx
je 1f
call second
1: ret
.size first, .-first
.org 0x300
.type second,@function
second: inc edx
dec ecx
je 2f
call first
2: ret
.size second, .-second''',
        ]
        for branch, count in [('loop', 2), ('jecxz', 0)]:
            programs.append('''.type root,@function
root: mov ecx, %d
%s leaf
ret
.size root, .-root
.org 0x140
.type leaf,@function
leaf: inc eax
ret
.size leaf, .-leaf''' % (count, branch))
        for index, source in enumerate(programs):
            with self.subTest(index=index), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                image, input_directory, records = inventory(root, source)
                directory = root / 'translations'
                generated = asm_c_program.generate(input_directory, directory, image)
                self.assertTrue(all(e['state'] == 'generated-unverified' for e in generated['entries']))
                report = asm_c_differential.verify(directory, root / 'evidence', image, scenarios=16)
                entry = next(e for e in report['entries'] if e['address'] == 0x100)
                self.assertEqual(entry['counts'], {'pass': 16}, entry)
                self.assertFalse(any(e['state'] == 'mismatch' for e in report['entries']))

    def test_unsupported_callee_removes_all_transitive_callers(self):
        source = '''.type root,@function
root: call middle
ret
.size root, .-root
.org 0x200
.type middle,@function
middle: call unsupported
ret
.size middle, .-middle
.org 0x300
.type unsupported,@function
unsupported: cpuid
ret
.size unsupported, .-unsupported
.org 0x400
.type leaf,@function
leaf: ret
.size leaf, .-leaf
.org 0x500
.type guarded,@function
guarded: xor eax,eax
jz done
call unsupported
done: ret
.size guarded, .-guarded'''
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, directory, records = inventory(root, source)
            codes, dependencies, closed, reasons = asm_c_program.analyse(records)
            self.assertEqual(closed, {0x400})
            self.assertEqual(dependencies[0x100], {0x200})
            self.assertEqual(dependencies[0x200], {0x300})
            self.assertIn('untranslated direct dependencies', reasons[0x100])
            self.assertIn('untranslated direct dependencies', reasons[0x200])
            self.assertIn('unsupported instruction', reasons[0x300])
            result = asm_c_program.generate(directory, root / 'out', image)
            entries = {e['address']: e for e in result['entries']}
            for address in (0x100, 0x200, 0x300, 0x500):
                self.assertEqual(entries[address]['root_blockers'], [0x300])
            self.assertNotIn('root_blockers', entries[0x400])
            partial = asm_c_program.generate(directory, root / 'partial', image,
                                             allow_partial_closure=True)
            entries = {e['address']: e for e in partial['entries']}
            self.assertTrue(entries[0x500]['incomplete_direct_closure'])
            self.assertEqual(entries[0x500]['root_blockers'], [0x300])
            self.assertEqual(entries[0x300]['state'], 'unsupported')
            compared = asm_c_differential.verify(root / 'partial', root / 'evidence', image,
                                                 scenarios=1, addresses=[0x100, 0x500])
            entries = {e['address']: e for e in compared['entries']}
            self.assertEqual(entries[0x500]['state'], 'probes-pass')
            self.assertEqual(entries[0x100]['state'], 'mismatch')
            self.assertEqual(entries[0x100]['probes'][0]['translated_stop']['pc'], 0x300)
            self.assertFalse((root / 'out/ef_lift_00000100.c').exists())
            self.assertEqual(sum(e['state'] == 'generated-unverified' for e in result['entries']), 1)

    def test_conditional_fallthrough_requires_adjacent_body(self):
        source = """.type root,@function
root: jz leaf
.size root, .-root
.type adjacent,@function
adjacent: cpuid
ret
.size adjacent, .-adjacent
.org 0x200
.type leaf,@function
leaf: ret
.size leaf, .-leaf"""
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, directory, _ = inventory(root, source)
            report = asm_c_program.generate(directory, root / 'out', image)
            entries = {entry['address']: entry for entry in report['entries']}
            adjacent = next(address for address in entries if 0x100 < address < 0x200)
            self.assertEqual(entries[0x100]['dependencies'], [adjacent, 0x200])
            self.assertEqual(entries[0x100]['root_blockers'], [adjacent])
            self.assertEqual(entries[0x100]['state'], 'unsupported')
            self.assertEqual(entries[0x200]['state'], 'generated-unverified')

    def test_reports_failures_after_first_unsupported_instruction(self):
        source = '''.type root,@function
root: call bad
ret
.size root, .-root
.org 0x200
.type bad,@function
bad: cpuid
nop
sti
int3
ret
.size bad, .-bad
.org 0x300
.type leaf,@function
leaf: ret
.size leaf, .-leaf'''
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, directory, _ = inventory(root, source)
            report = asm_c_program.generate(directory, root / 'out', image)
            entries = {e['address']: e for e in report['entries']}
            self.assertEqual(entries[0x100]['root_blockers'], [0x200])
            self.assertEqual(report['root_blocker_impact'], [
                dict(address=0x200, affected_entries=2, reason='unsupported instruction: cpuid')])
            self.assertNotIn('instruction_failures', entries[0x100])
            self.assertEqual(entries[0x200]['instruction_failures'], [
                dict(address=0x200, mnemonic='cpuid', reason='unsupported instruction: cpuid'),
                dict(address=0x203, mnemonic='sti', reason='unsupported instruction: sti'),
                dict(address=0x204, mnemonic='int3', reason='unsupported instruction: int3')])
            self.assertEqual(entries[0x200]['reason'], 'unsupported instruction: cpuid')
            self.assertEqual(report['syntactically_supported'], 2)
            self.assertFalse((root / 'out/ef_lift_00000100.c').exists())
            self.assertFalse((root / 'out/ef_lift_00000200.c').exists())

    def test_failure_audit_excludes_unestablished_bodies(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            _, _, records = inventory(root, '''.type bad,@function
bad: cpuid
ret
.size bad, .-bad''')
            for field, value in (('boundary', 'guessed'),
                                 ('ghidra_instruction_layout_matches', False),
                                 ('overlapping_entries', [0x101]),
                                 ('undecodable_addresses', [0x101]),
                                 ('address', 0x101)):
                record = dict(records[0], **{field: value})
                failures = {}
                _, _, closed, reasons = asm_c_program.analyse([record], failure_details=failures)
                self.assertFalse(closed)
                self.assertIn(record['address'], reasons)
                self.assertEqual(failures, {})

    def test_root_blockers_propagate_through_recursive_dependencies(self):
        dependencies = {1: {2}, 2: {1, 3, 4}, 5: {5}}
        reasons = {1: 'dependency', 2: 'dependency', 3: 'int', 4: 'in'}
        self.assertEqual(asm_c_program.root_blockers(dependencies, {5}, reasons),
                         {1: [3, 4], 2: [3, 4], 3: [3], 4: [4]})

    def test_port_boundaries_preserve_state_and_request_exact_width(self):
        forms = [('in', 'al,dx'), ('in', 'ax,dx'), ('in', 'eax,dx'),
                 ('out', 'dx,al'), ('out', 'dx,ax'), ('out', 'dx,eax'),
                 ('in', 'al,0x80'), ('in', 'ax,0x80'), ('in', 'eax,0x80'),
                 ('out', '0x80,al'), ('out', '0x80,ax'), ('out', '0x80,eax')]
        source = '\n'.join('.org 0x%x\n.type f%d,@function\nf%d: %s %s\nret\n.size f%d, .-f%d' %
                           (0x100 + n*16, n, n, op, operands, n, n)
                           for n, (op, operands) in enumerate(forms))
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, directory, records = inventory(root, source)
            self.assertFalse(asm_c_program.analyse(records)[2])
            self.assertEqual(len(asm_c_program.analyse(records, port_boundaries=True,
                                                      port_sites={0x100})[2]), 1)
            out = root/'out'
            report = asm_c_program.generate(directory, out, image, port_boundaries=True)
            with self.assertRaisesRegex(ValueError, 'service-aware'):
                asm_c_differential.verify(out, root/'evidence', image, scenarios=1)
            with self.assertRaisesRegex(ValueError, 'require port boundaries'):
                asm_c_program.generate(directory, root/'bad', image, port_sites={0x100})
            subprocess.run(['cc', '-O2', '-shared', '-fPIC', '-I'+str(out),
                            *map(str, sorted(out.glob('*.c'))), '-o', str(root/'program.so')],
                           check=True, capture_output=True)
            library = ctypes.CDLL(str(root/'program.so'))
            run = library.ef_lift_run
            run.argtypes = [ctypes.POINTER(asm_c_differential.State), ctypes.c_uint32, ctypes.c_uint32]
            callback_type = ctypes.CFUNCTYPE(ctypes.c_uint, ctypes.POINTER(asm_c_differential.State),
                                             ctypes.c_uint32, ctypes.c_uint, ctypes.c_void_p)
            service_run = library.ef_lift_run_services
            service_run.argtypes = run.argtypes + [callback_type, ctypes.c_void_p]
            requests = report['host_port_service_boundaries']['requests']
            self.assertEqual(len(requests), 12)
            for request in requests:
                with self.subTest(request=request):
                    memory = bytearray(4096)
                    memory[:image.stat().st_size] = image.read_bytes()
                    memory[0x800:0x804] = (0xf00).to_bytes(4, 'little')
                    array = (ctypes.c_uint8 * len(memory)).from_buffer(memory)
                    state = asm_c_differential.State(
                        (ctypes.c_uint32 * 8)(0xabcd1234, 2, 0xbeef03da, 4, 0x800, 6, 7, 8),
                        0xad7, request['ip'], 0, array, len(memory))
                    before, ram_before = bytes(state), bytes(memory)
                    self.assertEqual(run(ctypes.byref(state), 1, 0xf00), request['status'])
                    self.assertEqual(state.pc, request['ip'] + request['length'])
                    state.pc = request['ip']
                    self.assertEqual(bytes(state), before)
                    self.assertEqual(bytes(memory), ram_before)
                    events = []
                    @callback_type
                    def service(guest, ip, status, context):
                        events.append((ip, status))
                        if request['direction'] == 'in':
                            mask = (1 << request['width']) - 1
                            guest.contents.r[0] = (guest.contents.r[0] & ~mask) | (0x87654321 & mask)
                        return 0
                    # One instruction consumes the complete fuel unit, including service.
                    self.assertEqual(service_run(ctypes.byref(state), 1, 0xf00, service, None), 1)
                    self.assertEqual(events, [(request['ip'], request['status'])])
                    self.assertEqual(service_run(ctypes.byref(state), 1, 0xf00, service, None), 0)
                    self.assertEqual(state.pc, 0xf00)
                    self.assertEqual(state.r[4], 0x804)
                    self.assertEqual(state.flags, 0xad7)
                    mask = (1 << request['width']) - 1
                    expected = ((0xabcd1234 & ~mask) | (0x87654321 & mask)
                                if request['direction'] == 'in' else 0xabcd1234)
                    self.assertEqual(state.r[0], expected)
                    self.assertEqual(list(state.r)[1:4], [2, 0xbeef03da, 4])
                    self.assertEqual(list(state.r)[5:], [6, 7, 8])
                    self.assertEqual(bytes(memory), ram_before)
                    if unicorn:
                        from unicorn import x86_const as x86
                        cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
                        cpu.mem_map(0, 4096)
                        cpu.mem_write(0, ram_before)
                        registers = [x86.UC_X86_REG_EAX, x86.UC_X86_REG_ECX, x86.UC_X86_REG_EDX,
                                     x86.UC_X86_REG_EBX, x86.UC_X86_REG_ESP, x86.UC_X86_REG_EBP,
                                     x86.UC_X86_REG_ESI, x86.UC_X86_REG_EDI]
                        for register, value in zip(registers, [0xabcd1234, 2, 0xbeef03da, 4, 0x800, 6, 7, 8]):
                            cpu.reg_write(register, value)
                        cpu.reg_write(x86.UC_X86_REG_EFLAGS, 0xad7)
                        ports = []
                        def port_in(cpu, port, size, context):
                            ports.append(('in', port, size))
                            return 0x87654321 & ((1 << (size*8))-1)
                        def port_out(cpu, port, size, value, context):
                            ports.append(('out', port, size, value))
                        cpu.hook_add(unicorn.UC_HOOK_INSN, port_in, None, 1, 0, x86.UC_X86_INS_IN)
                        cpu.hook_add(unicorn.UC_HOOK_INSN, port_out, None, 1, 0, x86.UC_X86_INS_OUT)
                        cpu.emu_start(request['ip'], 0xf00, count=2)
                        port = 0x3da if request['port'] == 'dx' else request['port']
                        event = (request['direction'], port, request['width']//8)
                        if request['direction'] == 'out':
                            event += (0xabcd1234 & mask,)
                        self.assertEqual(ports, [event])
                        self.assertEqual([cpu.reg_read(r) for r in registers], list(state.r))
                        self.assertEqual(cpu.reg_read(x86.UC_X86_REG_EFLAGS), state.flags)
                        self.assertEqual(bytes(cpu.mem_read(0, 4096)), bytes(memory))

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_service_aware_port_differential_checks_traces_and_ram(self):
        forms = ['in al,dx', 'in ax,dx', 'in eax,dx',
                 'out dx,al', 'out dx,ax', 'out dx,eax',
                 'in al,0x80', 'in ax,0x80', 'in eax,0x80',
                 'out 0x80,al', 'out 0x80,ax', 'out 0x80,eax']
        source = '.type root,@function\nroot: ' + '\n'.join(forms) + '\nret\n.size root, .-root'
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, directory, _ = inventory(root, source)
            out = root/'out'
            asm_c_program.generate(directory, out, image, port_boundaries=True)
            subprocess.run(['cc', '-O2', '-shared', '-fPIC', '-I'+str(out),
                            *map(str, sorted(out.glob('*.c'))), '-o', str(root/'program.so')],
                           check=True, capture_output=True)
            library = ctypes.CDLL(str(root/'program.so'))
            run = library.ef_lift_run
            run.argtypes = [ctypes.POINTER(asm_c_differential.State), ctypes.c_uint32, ctypes.c_uint32]
            registers = [0xabcd1234, 2, 0xbeef03da, 4, 0x3f0000, 6, 7, 8]
            for memory_services in (False, True):
                def factory():
                    calls = 0
                    def handler(request, values, *ram):
                        nonlocal calls
                        calls += 1
                        if request['direction'] == 'in':
                            mask = (1 << request['width']) - 1
                            values['ax'] = (values['ax'] & ~mask) | ((0x87654321+calls) & mask)
                        elif ram:
                            ram[0][0x300100+calls] = values['ax'] & 255
                        return values
                    return handler
                result = asm_c_differential.compare(run, image.read_bytes(), 0x100,
                    registers, 0xad7, program=True, port_factory=factory, memory_services=memory_services)
                self.assertEqual(result['state'], 'pass', result)
                self.assertEqual(len(result['port_services']['original']), 12)
                self.assertEqual(result['port_services']['original'], result['port_services']['translated'])
                if memory_services:
                    self.assertIn('memory', result['port_services']['original'][0])
            def invalid_factory():
                def handler(request, values):
                    values['cx'] ^= 1
                    return values
                return handler
            result = asm_c_differential.compare(run, image.read_bytes(), 0x100,
                registers, 0xad7, program=True, port_factory=invalid_factory)
            self.assertEqual(result['state'], 'inconclusive', result)
            self.assertIn('unrelated registers', result['reason'])
            instances = 0
            def divergent_factory():
                nonlocal instances
                instances += 1
                value = instances
                def handler(request, values):
                    if request['direction'] == 'in':
                        mask = (1 << request['width']) - 1
                        values['ax'] = (values['ax'] & ~mask) | value
                    return values
                return handler
            result = asm_c_differential.compare(run, image.read_bytes(), 0x100,
                registers, 0xad7, program=True, port_factory=divergent_factory)
            self.assertEqual(result['state'], 'mismatch', result)
            self.assertIn('port service request/response trace', result['differences'])

    def test_port_service_decoder_rejects_ambiguous_encodings(self):
        for encoded, mnemonic, operands in [('f3ec', 'in', 'al,dx'),
                                             ('66ec', 'in', 'al,dx'),
                                             ('ec00', 'in', 'al,dx'),
                                             ('ec', 'in', 'eax,dx'),
                                             ('ee', 'in', 'al,dx')]:
            with self.subTest(encoded=encoded, operands=operands):
                with self.assertRaises(asm_c_program.asm_to_c.Unsupported):
                    asm_c_program.port_service_request(dict(address=0x100, bytes=encoded,
                                                           mnemonic=mnemonic, operands=operands))

    def test_service_reachability_handles_cycles_and_excludes_unsupported_bodies(self):
        records = [dict(address=a, instructions=insns) for a, insns in [
            (1, []), (2, []),
            (3, [dict(address=30, mnemonic='int', bytes='cd21')]),
            (4, [dict(address=40, mnemonic='int', bytes='cd31')]),
            (5, []), (6, [dict(address=60, mnemonic='int', bytes='cd21')])]]
        dependencies = {1: {2}, 2: {1, 3, 4}, 3: set(), 4: set(), 5: {5}, 6: set()}
        self.assertEqual(asm_c_program.reachable_service_sites(
            records, dependencies, {1, 2, 3, 4, 5}),
            {1: [30, 40], 2: [30, 40], 3: [30], 4: [40], 5: []})

    def test_interrupt_sites_admit_only_audited_service_callers(self):
        source = '''.type root,@function
root: int 0x21
ret
.size root, .-root
.org 0x200
.type other,@function
other: int 0x21
ret
.size other, .-other'''
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, directory, records = inventory(root, source)
            self.assertEqual(asm_c_program.analyse(records, interrupt_boundaries=True)[2],
                             {0x100, 0x200})
            self.assertEqual(asm_c_program.analyse(records, interrupt_boundaries=True,
                                                  interrupt_sites=frozenset())[2], set())
            report = asm_c_program.generate(directory, root/'out', image,
                                            interrupt_boundaries=True, interrupt_sites={0x100})
            entries = {e['address']: e for e in report['entries']}
            self.assertEqual(entries[0x100]['state'], 'generated-unverified')
            self.assertEqual(entries[0x200]['state'], 'unsupported')
            self.assertEqual(report['host_service_boundaries']['sites'], [0x100])
            self.assertEqual(entries[0x100]['reachable_host_service_sites'], [0x100])
            with self.assertRaisesRegex(ValueError, 'require interrupt boundaries'):
                asm_c_program.generate(directory, root/'bad', image, interrupt_sites={0x100})

    def test_explicit_interrupt_site_requires_vector_aware_service(self):
        source = '.type root,@function\nroot: int 6\nret\n.size root, .-root'
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, directory, records = inventory(root, source)
            self.assertFalse(asm_c_program.analyse(records, interrupt_boundaries=True)[2])
            out = root/'out'
            report = asm_c_program.generate(directory, out, image,
                                            interrupt_boundaries=True, interrupt_sites={0x100})
            self.assertEqual(report['host_service_boundaries']['requests'], [dict(ip=0x100, vector=6)])
            self.assertEqual(report['entries'][0]['reachable_host_service_sites'], [0x100])
            subprocess.run(['cc', '-shared', '-fPIC', *map(str, sorted(out.glob('*.c'))),
                            '-o', str(root/'program.so')], check=True, capture_output=True)
            library = ctypes.CDLL(str(root/'program.so'))
            function = library.ef_lift_run
            function.argtypes = [ctypes.POINTER(asm_c_differential.State), ctypes.c_uint32, ctypes.c_uint32]

            ram = (ctypes.c_uint8 * 4096)()
            ram[:len(image.read_bytes())] = image.read_bytes()
            state = asm_c_differential.State((ctypes.c_uint32 * 8)(), 0x246, 0x100, 0, ram, len(ram))
            self.assertEqual(function(ctypes.byref(state), 1, 0xf00), 0x106)
            self.assertEqual(state.pc, 0x102)
            callback_type = ctypes.CFUNCTYPE(ctypes.c_uint, ctypes.POINTER(asm_c_differential.State),
                                             ctypes.c_uint32, ctypes.c_uint, ctypes.c_void_p)
            requests = []

            @callback_type
            def respond(guest, ip, vector, context):
                requests.append((ip, vector))
                guest.contents.r[0] = 42
                return 0

            state.pc = 0x100
            run = library.ef_lift_run_services
            run.argtypes = [*function.argtypes, callback_type, ctypes.c_void_p]
            self.assertEqual(run(ctypes.byref(state), 1, 0xf00, respond, None), 1)
            self.assertEqual(requests, [(0x100, 6)])
            self.assertEqual(state.r[0], 42)

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_explicit_breakpoint_yields_single_byte_continuation(self):
        for tail in ('ret', ''):
            with self.subTest(tail=tail), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                source = f'.type root,@function\nroot: int3\n{tail}\n.size root, .-root'
                image, directory, records = inventory(root, source)
                self.assertFalse(asm_c_program.analyse(records, interrupt_boundaries=True)[2])
                out = root/'out'
                report = asm_c_program.generate(directory, out, image,
                    interrupt_boundaries=True, interrupt_sites={0x100})
                self.assertEqual(report['host_service_boundaries']['requests'],
                                 [dict(ip=0x100, vector=3, length=1, status=0x203)])
                subprocess.run(['cc', '-shared', '-fPIC', *map(str, sorted(out.glob('*.c'))),
                               '-o', str(root/'program.so')], check=True, capture_output=True)
                library = ctypes.CDLL(str(root/'program.so'))
                function = library.ef_lift_run
                function.argtypes = [ctypes.POINTER(asm_c_differential.State), ctypes.c_uint32, ctypes.c_uint32]
                ram = (ctypes.c_uint8 * 4096)()
                ram[:len(image.read_bytes())] = image.read_bytes()
                state = asm_c_differential.State((ctypes.c_uint32 * 8)(), 0x246, 0x100, 0, ram, len(ram))
                self.assertEqual(function(ctypes.byref(state), 1, 0xf00), 0x203)
                self.assertEqual(state.pc, 0x101)
                if not tail:
                    self.assertEqual(function(ctypes.byref(state), 1, 0xf00), 2)
                    continue

                def respond(vector, ip, values):
                    self.assertEqual((vector, ip), (3, 0x100))
                    values['ax'] = 42
                    return values

                result = asm_c_differential.compare(function, image.read_bytes(), 0x100,
                    [1, 2, 3, 4, 0x300000, 6, 7, 8], 0x246, program=True,
                    service_factory=lambda: respond, vector_services=True)
                self.assertEqual(result['state'], 'pass', result)

    def test_port_requirements_propagate_through_call_cycles(self):
        source = '''.type root,@function
root: call middle
ret
.size root, .-root
.org 0x200
.type middle,@function
middle: call root
call device
ret
.size middle, .-middle
.org 0x300
.type device,@function
device: in al,dx
out 0x80,al
ret
.size device, .-device
.org 0x400
.type isolated,@function
isolated: ret
.size isolated, .-isolated'''
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, directory, records = inventory(root, source)
            report = asm_c_program.generate(directory, root/'out', image, port_boundaries=True)
            entries = {e['address']: e for e in report['entries']}
            for address in (0x100, 0x200, 0x300):
                self.assertEqual(entries[address]['reachable_host_port_service_sites'], [0x300, 0x301])
                self.assertEqual(entries[address]['reachable_host_service_sites'], [])
            self.assertEqual(entries[0x400]['reachable_host_port_service_sites'], [])
            restricted = asm_c_program.generate(directory, root/'restricted', image,
                                                port_boundaries=True, port_sites={0x300})
            for entry in restricted['entries']:
                if entry['address'] != 0x400:
                    self.assertEqual(entry['state'], 'unsupported')
                    self.assertNotIn('reachable_host_port_service_sites', entry)

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_mixed_dos_dpmi_boundaries_preserve_vector_and_shared_fuel(self):
        source = """.type root,@function
root: int 0x31
adc eax,0
int 0x21
adc ebx,0
ret
.size root, .-root"""
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, directory, records = inventory(root, source)
            self.assertFalse(asm_c_program.analyse(records)[2])
            self.assertFalse(asm_c_program.analyse(
                records, interrupt_boundaries=True, interrupt_sites={0x105})[2])
            out = root/'out'
            report = asm_c_program.generate(directory, out, image, interrupt_boundaries=True)
            self.assertEqual(report['host_service_boundaries']['requests'],
                             [dict(ip=0x100, vector=0x31), dict(ip=0x105, vector=0x21)])
            subprocess.run(['cc', '-O2', '-shared', '-fPIC', '-I'+str(out),
                            *map(str, sorted(out.glob('*.c'))), '-o', str(root/'program.so')],
                           check=True, capture_output=True)
            library = ctypes.CDLL(str(root/'program.so'))
            function = library.ef_lift_run
            function.argtypes = [ctypes.POINTER(asm_c_differential.State),
                                 ctypes.c_uint32, ctypes.c_uint32]
            def respond(vector, ip, values):
                self.assertIn((vector, ip), ((0x31, 0x100), (0x21, 0x105)))
                values['ax' if vector == 0x31 else 'bx'] = 41
                values['flags'] |= 1
                return values
            result = asm_c_differential.compare(
                function, image.read_bytes(), 0x100,
                [0x501, 2, 3, 4, 0x300000, 6, 7, 8], 0x246,
                program=True, service_factory=lambda: respond, vector_services=True)
            self.assertEqual(result['state'], 'pass', result)
            self.assertEqual([e['vector'] for e in result['host_services']['original']],
                             [0x31, 0x21])
            # Legacy INT21-only handlers must reject DPMI rather than misroute it.
            result = asm_c_differential.compare(
                function, image.read_bytes(), 0x100,
                [0x501, 2, 3, 4, 0x300000, 6, 7, 8], 0x246,
                program=True, service_factory=lambda: lambda ip, values: values)
            self.assertEqual(result['state'], 'inconclusive', result)
            memory = bytearray(4096)
            memory[:image.stat().st_size] = image.read_bytes()
            memory[0x800:0x804] = (0xf00).to_bytes(4, 'little')
            array = (ctypes.c_uint8 * len(memory)).from_buffer(memory)
            state = asm_c_differential.State(
                (ctypes.c_uint32 * 8)(0x501, 2, 3, 4, 0x800, 6, 7, 8),
                0x246, 0x100, 0, array, len(memory))
            run = library.ef_lift_run_services
            callback_type = ctypes.CFUNCTYPE(ctypes.c_uint,
                ctypes.POINTER(asm_c_differential.State), ctypes.c_uint32,
                ctypes.c_uint, ctypes.c_void_p)
            run.argtypes = [ctypes.POINTER(asm_c_differential.State), ctypes.c_uint32,
                            ctypes.c_uint32, ctypes.c_void_p, ctypes.c_void_p]
            self.assertEqual(run(ctypes.byref(state), 1, 0xf00, None, None), 0x131)
            self.assertEqual(state.pc, 0x102)
            state.pc = 0x100
            events = []
            @callback_type
            def callback(guest, ip, vector, context):
                events.append((ip, vector))
                guest.contents.r[0 if vector == 0x31 else 3] = 41
                guest.contents.flags |= 1
                return 0
            self.assertEqual(run(ctypes.byref(state), 3, 0xf00, callback, None), 1)
            self.assertEqual(events, [(0x100, 0x31), (0x105, 0x21)])
            self.assertEqual((state.pc, state.r[0], state.r[3]), (0x107, 42, 41))
            self.assertEqual(run(ctypes.byref(state), 2, 0xf00, callback, None), 0)
            self.assertEqual((state.r[3], state.r[4]), (42, 0x804))

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_cpuid_boundaries_resume_with_matching_cpu_model(self):
        source = """
.intel_syntax noprefix
.code32
.org 0x100
.type identify,@function
identify:
    xor eax,eax
    cpuid
    mov eax,1
    cpuid
    ret
.size identify, .-identify
"""
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, directory, _ = inventory(root, source)
            out = root / 'out'
            sites = (0x102, 0x109)
            asm_c_program.generate(directory, out, image, cpuid_sites=sites)
            report = asm_c_differential.verify(out, root / 'evidence', image,
                                               scenarios=1, service_free_paths=True)
            entry, = report['entries']
            self.assertEqual(entry['state'], 'probes-pass', entry)
            trace = entry['probes'][0]['cpuid']
            self.assertEqual(trace['original'], trace['translated'])
            self.assertEqual([(event['ip'], event['eax']) for event in trace['original']],
                             list(zip(sites, (0, 1))))
            with self.assertRaisesRegex(ValueError, 'reviewed CPUID'):
                asm_c_program.generate(directory, root / 'bad', image, cpuid_sites=(0x103,))

    def test_differential_checks_paths_without_servicing_boundaries(self):
        source = """.type leaf,@function
leaf: inc eax
ret
.size leaf, .-leaf
.org 0x200
.type dos,@function
dos: mov ax,0x4000
mov ebx,1
mov ecx,4
mov edx,0x300000
int 0x21
mov al,0x34
out 0x43,al
mov al,0
out 0x40,al
out 0x40,al
ret
.size dos, .-dos
.org 0x300
.type device,@function
device: in al,0x61
ret
.size device, .-device
.org 0x400
.type terminate,@function
terminate: mov ax,0x4c07
int 0x21
inc eax
ret
.size terminate, .-terminate
.org 0x500
.type bad_open,@function
bad_open: mov ax,0x3d01
xor edx,edx
int 0x21
adc eax,0
ret
.size bad_open, .-bad_open
.org 0x600
.type named_open,@function
named_open: mov ax,0x3d00
mov edx,0x680
int 0x21
ret
.size named_open, .-named_open
.org 0x680
.asciz "EXAMPLE.TXT"
.org 0x700
.type write_file,@function
write_file: mov eax,0x3c00
mov edx,0x680
xor ecx,ecx
int 0x21
mov ebx,eax
mov eax,0x4400
int 0x21
mov eax,0x4000
mov edx,0x680
mov ecx,4
int 0x21
mov eax,0x6800
int 0x21
mov eax,0x4201
xor ecx,ecx
xor edx,edx
int 0x21
mov eax,0x3e00
int 0x21
mov eax,0x3d00
mov edx,0x680
int 0x21
mov ebx,eax
mov eax,0x3f00
mov edx,0x300000
mov ecx,8
int 0x21
mov esi,dword ptr [0x300000]
ret
.size write_file, .-write_file"""
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, directory, _ = inventory(root, source)
            out = root / 'out'
            asm_c_program.generate(directory, out, image, interrupt_boundaries=True,
                                   port_boundaries=True)
            evidence = root / 'evidence'
            with patch.object(asm_c_differential, 'HOST_UNIT_BYTES', 1):
                report = asm_c_differential.verify(out, evidence, image, scenarios=1,
                                                   service_free_paths=True, jobs=2)
            units = report['host_translation_units_sha256']
            self.assertGreater(len(units), 1)
            for name, digest in units.items():
                self.assertEqual(hashlib.sha256((evidence / name).read_bytes()).hexdigest(), digest)
            self.assertEqual(report['host_service_policy'], 'reject-on-reach')
            entries = {entry['address']: entry for entry in report['entries']}
            self.assertEqual(entries[0x100]['state'], 'probes-pass')
            for address in (0x200, 0x300):
                self.assertEqual(entries[address]['state'], 'inconclusive')
                self.assertIn('unmodeled host service', entries[address]['probes'][0]['reason'])
            interrupt_stop = entries[0x200]['probes'][0]['original_service_stop']
            self.assertEqual(interrupt_stop, dict(
                kind='interrupt', ip=0x213, vector=0x21, ax=0x4000,
                write_request=dict(handle=1, count=4, offset=0x300000, ds=16)))
            self.assertEqual(entries[0x300]['probes'][0]['original_service_stop'],
                             dict(kind='port', ip=0x300, direction='in', port=0x61, width=8))
            console_report = asm_c_differential.verify(
                out, root / 'console', image, scenarios=1, service_free_paths=True,
                dos_console=True, pit_writes=True, dos_invalid_open=True)
            self.assertEqual(console_report['host_service_policy'], 'dos-console-and-exit-prepared-only')
            self.assertEqual(console_report['pit_write_policy'], 'transcript-prepared-only')
            console_entries = {entry['address']: entry for entry in console_report['entries']}
            self.assertEqual(console_entries[0x300]['state'], 'inconclusive')
            self.assertEqual(console_entries[0x200]['state'], 'probes-pass', console_entries[0x200])
            events = console_entries[0x200]['probes'][0]['host_services']
            self.assertEqual(events['original'], events['translated'])
            self.assertEqual(events['original'][0]['output_sha256'],
                             hashlib.sha256(bytes(range(4))).hexdigest())
            ports = console_entries[0x200]['probes'][0]['port_services']
            self.assertEqual(ports['original'], ports['translated'])
            self.assertEqual([event['request']['port'] for event in ports['original']], [0x43, 0x40, 0x40])
            self.assertEqual(console_entries[0x400]['state'], 'probes-pass', console_entries[0x400])
            self.assertEqual(console_entries[0x400]['probes'][0]['dos_exit'],
                             dict(original=dict(ip=0x404, code=7), translated=dict(ip=0x404, code=7)))

            empty_drive_report = asm_c_differential.verify(
                out, root / 'empty-drive', image, scenarios=1,
                service_free_paths=True, dos_empty_drive=True, addresses=[0x600, 0x700])
            entry, write_entry = empty_drive_report['entries']
            self.assertEqual(entry['state'], 'probes-pass', entry)
            events = entry['probes'][0]['host_services']
            self.assertEqual(events['original'], events['translated'])
            self.assertEqual(events['original'][0]['after']['ax'], 2)
            self.assertEqual(events['original'][0]['after']['flags'] & 1, 1)
            self.assertEqual(write_entry['state'], 'probes-pass', write_entry)
            write_events = write_entry['probes'][0]['host_services']
            self.assertEqual(write_events['original'], write_events['translated'])
            self.assertEqual([event['after']['ax'] for event in write_events['original']],
                             [5, 2, 4, 0x6800, 4, 0x3e01, 5, 4])
            self.assertEqual(write_events['original'][2]['output_sha256'],
                             hashlib.sha256(b'EXAM').hexdigest())
            read_event = write_events['original'][-1]
            self.assertEqual(read_event['input_sha256'], hashlib.sha256(b'EXAM').hexdigest())
            self.assertEqual((read_event['after']['cx'], read_event['after']['dx']), (0, 0))

        self.assertEqual(console_report['dos_invalid_open_policy'], 'prepared-only')
        self.assertEqual(console_entries[0x500]['state'], 'probes-pass')
        open_events = console_entries[0x500]['probes'][0]['host_services']
        self.assertEqual(open_events['original'], open_events['translated'])
        self.assertEqual(open_events['original'][0]['after']['ax'], 3)
        self.assertEqual(open_events['original'][0]['after']['flags'] & 1, 1)
        self.assertEqual(console_entries[0x600]['state'], 'inconclusive')
        self.assertIn('filesystem state', console_entries[0x600]['probes'][0]['reason'])

    def test_interrupt_boundary_preserves_state_and_resumes_after_host_service(self):
        source = '''.type root,@function
root: int 0x21
adc eax,0
ret
.size root, .-root'''
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, directory, records = inventory(root, source)
            self.assertFalse(asm_c_program.analyse(records)[2])
            out = root / 'out'
            report = asm_c_program.generate(directory, out, image, interrupt_boundaries=True)
            self.assertEqual(report['host_service_boundaries']['sites'], [0x100])
            with self.assertRaisesRegex(ValueError, 'service-aware'):
                asm_c_differential.verify(out, root/'evidence', image, scenarios=1)
            subprocess.run(['cc', '-O2', '-shared', '-fPIC', '-I'+str(out),
                            *map(str, sorted(out.glob('*.c'))), '-o', str(root/'program.so')],
                           check=True, capture_output=True)
            function = ctypes.CDLL(str(root/'program.so')).ef_lift_run
            function.argtypes = [ctypes.POINTER(asm_c_differential.State),
                                 ctypes.c_uint32, ctypes.c_uint32]
            memory = bytearray(4096)
            memory[:image.stat().st_size] = image.read_bytes()
            memory[0x800:0x804] = (0xf00).to_bytes(4, 'little')
            array = (ctypes.c_uint8 * len(memory)).from_buffer(memory)
            state = asm_c_differential.State((ctypes.c_uint32 * 8)(0x3508, 2, 3, 4, 0x800, 6, 7, 8),
                                             0x246, 0x100, 0, array, len(memory))
            before = bytes(state)
            before_memory = bytes(memory)
            self.assertEqual(function(ctypes.byref(state), 100, 0xf00), 0x121)
            self.assertEqual(state.pc, 0x102)
            state.pc = 0x100
            self.assertEqual(bytes(state), before)
            self.assertEqual(bytes(memory), before_memory)
            state.pc = 0x102
            # Model a host response; the following ADC must consume its CF.
            state.r[0] = 41
            state.flags |= 1
            self.assertEqual(function(ctypes.byref(state), 100, 0xf00), 0)
            self.assertEqual(state.r[0], 42)
            self.assertEqual(state.pc, 0xf00)
            self.assertEqual(state.r[4], 0x804)

    def test_native_x32_service_runner_accounts_for_fuel_and_rejection(self):
        from tools.x32_allocation import Allocation
        class NativeAllocation(ctypes.Structure):
            _fields_ = [(n, ctypes.c_uint32) for n in
                        ('heap_end', 'stack_end', 'free_bytes', 'minimum_stack',
                         'heap_ceiling', 'arena_start', 'arena_end')]
        source = '''.org 0x240
.type root,@function
root: mov ax,0x3508
nop
int 0x21
adc eax,0
ret
.size root, .-root'''
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, directory, _ = inventory(root, source)
            out = root/'out'
            asm_c_program.generate(directory, out, image, interrupt_boundaries=True)
            runtime = Path(__file__).resolve().parents[1]/'runtime'
            subprocess.run(['cc', '-O2', '-shared', '-fPIC', '-I'+str(out), '-I'+str(runtime),
                            *map(str, sorted(out.glob('*.c'))), str(runtime/'x32.c'),
                            str(runtime/'asm_x32_service.c'), '-o', str(root/'program.so')],
                           check=True, capture_output=True)
            library = ctypes.CDLL(str(root/'program.so'))
            run = library.ef_lift_run_services
            run.argtypes = [ctypes.POINTER(asm_c_differential.State), ctypes.c_uint32,
                            ctypes.c_uint32, ctypes.c_void_p, ctypes.c_void_p]
            callback = ctypes.cast(library.ef_asm_x32_service, ctypes.c_void_p)
            for requested in (0x4df001, 0x4e1001, 0x5000000):
                model = Allocation()
                allocation = NativeAllocation(model.heap_end, model.stack_end, model.free_bytes,
                                              model.minimum_stack, model.heap_ceiling, 0, 0)
                memory = bytearray(4096)
                memory[:image.stat().st_size] = image.read_bytes()
                memory[0x800:0x804] = (0xf00).to_bytes(4, 'little')
                array = (ctypes.c_uint8 * len(memory)).from_buffer(memory)
                state = asm_c_differential.State(
                    (ctypes.c_uint32 * 8)(0xabcd0000, requested, 3, 4, 0x800, 6, 7, 8),
                    0x246, 0x240, 0, array, len(memory))
                context = ctypes.byref(allocation)
                self.assertEqual(run(ctypes.byref(state), 2, 0xf00, callback, context), 1)
                self.assertEqual(state.pc, 0x245)
                before = bytes(state)
                self.assertEqual(run(ctypes.byref(state), 0, 0xf00, callback, context), 1)
                self.assertEqual(bytes(state), before)
                before_allocation = bytes(allocation)
                self.assertEqual(run(ctypes.byref(state), 1, 0xf00, None, context), 0x121)
                self.assertEqual(state.pc, 0x247)
                state.pc = 0x245
                self.assertEqual(bytes(state), before)
                self.assertEqual(bytes(allocation), before_allocation)
                expected = model.grow_heap(requested, state.flags)
                status = run(ctypes.byref(state), 1, 0xf00, callback, context)
                self.assertEqual(state.pc, 0x247)
                if expected is None:
                    self.assertEqual(status, 0x122)
                    state.pc = 0x245
                    self.assertEqual(bytes(state), before)
                else:
                    self.assertEqual(status, 1)
                    self.assertEqual((state.r[0], state.r[1], state.flags), expected)
                    self.assertEqual(allocation.heap_end, model.heap_end)
                    self.assertEqual(allocation.free_bytes, model.free_bytes)
                    self.assertEqual(run(ctypes.byref(state), 2, 0xf00, callback, context), 0)
                    self.assertEqual(state.r[4], 0x804)

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_interrupt_bridge_matches_original_x32_heap_growth(self):
        from tools.unicorn_backend import serve_startup_int21
        from tools.x32_allocation import Allocation
        source = '''.org 0x240
.type root,@function
root: mov ax,0x3508
nop
int 0x21
adc eax,0
ret
.size root, .-root
.org 0x280
.type residency,@function
residency: mov ax,0x252b
int 0x21
ret
.size residency, .-residency'''
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, directory, _ = inventory(root, source)
            out = root/'out'
            asm_c_program.generate(directory, out, image, interrupt_boundaries=True)
            subprocess.run(['cc', '-O2', '-shared', '-fPIC', '-I'+str(out),
                            *map(str, sorted(out.glob('*.c'))), '-o', str(root/'program.so')],
                           check=True, capture_output=True)
            function = ctypes.CDLL(str(root/'program.so')).ef_lift_run
            function.argtypes = [ctypes.POINTER(asm_c_differential.State),
                                 ctypes.c_uint32, ctypes.c_uint32]
            for requested in (0x4df000, 0x4df001, 0x4e0000, 0x4e1001):
                allocations = []
                def factory():
                    allocation = Allocation()
                    allocations.append(allocation)
                    return lambda ip, regs: serve_startup_int21(ip, regs, allocation)
                with self.subTest(requested=requested):
                    result = asm_c_differential.compare(
                        function, image.read_bytes(), 0x240,
                        [0xabcd0000, requested, 3, 4, 0x300000, 6, 7, 8], 0x246,
                        program=True, service_factory=factory)
                    self.assertEqual(result['state'], 'pass', result)
                    self.assertEqual(len(result['host_services']['original']), 1)
                    self.assertEqual(allocations[0], allocations[1])
                    self.assertEqual(allocations[0].heap_end, (requested+4095)&~4095)
                    prepared = asm_c_differential.compare(
                        function, image.read_bytes(), 0x240,
                        [0xabcd0000, requested, 3, 4, 0x300000, 6, 7, 8], 0x246,
                        program=True, x32_heap=True, prepared_memory_size=0x500000)
                    self.assertEqual(prepared['state'], 'pass', prepared)
                    for side in ('original', 'translated'):
                        self.assertEqual(prepared['host_services'][side][0]['after'],
                                         result['host_services'][side][0]['after'])
            unmapped = asm_c_differential.compare(
                function, image.read_bytes(), 0x240,
                [0, 0x500001, 3, 4, 0x300000, 6, 7, 8], 0x246,
                program=True, x32_heap=True, prepared_memory_size=0x500000)
            self.assertEqual(unmapped['state'], 'inconclusive', unmapped)
            self.assertIn('exceeds prepared RAM', unmapped['reason'])
            result = asm_c_differential.compare(
                function, image.read_bytes(), 0x240,
                [0, 0x5000000, 3, 4, 0x300000, 6, 7, 8], 0x246,
                program=True, service_factory=factory)
            self.assertEqual(result['state'], 'inconclusive', result)
            self.assertIn('service unavailable', result['reason'])

            for operation, start, length, expected in (
                (0x501, 0x300000, 0x1000, (0x252b, 0)),
                (0x601, 0x3fffff, 1, (0x252b, 0)),
                (0x700, 0, 0, (0xa5a5a5a5, 1)),
                (0x501, 0x3fffff, 2, None),
                (0x500, 0, 0, None),
            ):
                with self.subTest(operation=operation, start=start, length=length):
                    result = asm_c_differential.compare(
                        function, image.read_bytes(), 0x280,
                        [0, start, length, operation, 0x300000, 6, 7, 8], 0x247,
                        program=True, x32_residency=True)
                    if expected is None:
                        self.assertEqual(result['state'], 'inconclusive', result)
                    else:
                        self.assertEqual(result['state'], 'pass', result)
                        response = result['host_services']['original'][0]['after']
                        self.assertEqual((response['ax'], response['flags'] & 1), expected)

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_memory_service_effects_are_compared_before_later_overwrites(self):
        source = """.type root,@function
root: int 0x21
mov dword ptr [0x210000],0
ret
.size root, .-root"""
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, directory, _ = inventory(root, source)
            out = root/'out'
            asm_c_program.generate(directory, out, image, interrupt_boundaries=True)
            subprocess.run(['cc', '-O2', '-shared', '-fPIC', '-I'+str(out),
                            *map(str, sorted(out.glob('*.c'))), '-o', str(root/'program.so')],
                           check=True, capture_output=True)
            library = ctypes.CDLL(str(root/'program.so'))
            function = library.ef_lift_run
            function.argtypes = [ctypes.POINTER(asm_c_differential.State),
                                 ctypes.c_uint32, ctypes.c_uint32]
            for mode, expected in [('same', 'pass'), ('different', 'mismatch'),
                                   ('resize-original', 'inconclusive'),
                                   ('resize-translated', 'mismatch')]:
                instances = []
                def factory():
                    index = len(instances)
                    instances.append(index)
                    def handler(vector, ip, registers, ram):
                        self.assertEqual((vector, ip), (0x21, 0x100))
                        self.assertEqual(ram[0x100:0x102], bytes.fromhex('cd21'))
                        ram[0x210000] = 1 + (mode == 'different' and index == 1)
                        if mode == ('resize-original' if index == 0 else 'resize-translated'):
                            ram.append(0)
                        return registers
                    return handler
                with self.subTest(mode=mode):
                    result = asm_c_differential.compare(
                        function, image.read_bytes(), 0x100,
                        [1, 2, 3, 4, 0x300000, 6, 7, 8], 0x246,
                        program=True, service_factory=factory,
                        vector_services=True, memory_services=True)
                    self.assertEqual(result['state'], expected, result)
                    if mode == 'same':
                        event = result['host_services']['original'][0]
                        self.assertNotEqual(event['memory']['before_sha256'],
                                            event['memory']['after_sha256'])
                    if mode == 'different':
                        self.assertIn('host service request/response trace', result['differences'])

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_explicit_sti_stop_leaves_unreached_path_comparable(self):
        source = '''.type root,@function
root: ret
.size root, .-root
.org 0x200
.type leaf,@function
leaf: sti
ret
.size leaf, .-leaf'''
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, directory, _ = inventory(root, source)
            out = root / 'out'
            with self.assertRaisesRegex(ValueError, 'reviewed STI'):
                asm_c_program.generate(directory, out, image, sti_stop_sites=[0x201])
            asm_c_program.generate(directory, out, image, sti_stop_sites=[0x200])
            report = asm_c_differential.verify(out, root / 'evidence', image, scenarios=1,
                                                  service_free_paths=True)
            self.assertEqual(report['sti_stop_sites'], [0x200])
            by_address = {e['address']: e for e in report['entries']}
            self.assertEqual(by_address[0x100]['state'], 'probes-pass')
            self.assertEqual(by_address[0x200]['probes'][0]['reason'],
                             'original reached STI stop at 0x200')
            run = ctypes.CDLL(str(root / 'evidence/translations.so')).ef_lift_run
            run.argtypes = [ctypes.POINTER(asm_c_differential.State), ctypes.c_uint32,
                            ctypes.c_uint32]
            memory = (ctypes.c_uint8 * 4096).from_buffer_copy(image.read_bytes().ljust(4096, b'\0'))
            state = asm_c_differential.State(pc=0x200, flags=2, memory=memory, memory_size=4096)
            self.assertEqual(run(ctypes.byref(state), 1, 0xf00), 0x204)
            self.assertEqual((state.pc, state.flags), (0x200, 2))

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_instruction_budget_is_shared_across_calls_and_can_resume(self):
        source = '''.type root,@function
root: call leaf
ret
.size root, .-root
.org 0x200
.type leaf,@function
leaf: inc eax
ret
.size leaf, .-leaf'''
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, directory, records = inventory(root, source)
            out = root / 'out'
            manifest = asm_c_program.generate(directory, out, image, shard_entries=2)
            with self.assertRaisesRegex(ValueError, 'no generated translation at 0x201'):
                asm_c_differential.verify(out, root / 'missing', image, addresses=[0x201])
            report = asm_c_differential.verify(out, root / 'evidence', image, scenarios=1,
                                                  addresses=[0x100])
            self.assertEqual([(e['address'], e['state']) for e in report['entries']],
                             [(0x100, 'probes-pass')])
            function = ctypes.CDLL(str(root / 'evidence/translations.so')).ef_lift_run
            function.argtypes = [ctypes.POINTER(asm_c_differential.State), ctypes.c_uint32, ctypes.c_uint32]
            memory = bytearray(4096)
            memory[:image.stat().st_size] = image.read_bytes()
            memory[0x800:0x804] = (0xf00).to_bytes(4, 'little')
            array = (ctypes.c_uint8 * len(memory)).from_buffer(memory)
            state = asm_c_differential.State((ctypes.c_uint32 * 8)(0, 0, 0, 0, 0x800, 0, 0, 0),
                                             0x202, 0x100, 0, array, len(memory))
            self.assertEqual(function(ctypes.byref(state), 1, 0xf00), 1)
            self.assertEqual(state.pc, 0x200)
            self.assertEqual(state.r[4], 0x7fc)
            self.assertEqual(int.from_bytes(memory[0x7fc:0x800], 'little'), 0x105)
            self.assertEqual(function(ctypes.byref(state), 1, 0xf00), 1)
            self.assertEqual(state.r[0], 1)
            self.assertEqual(function(ctypes.byref(state), 2, 0xf00), 0)
            self.assertEqual(state.pc, 0xf00)
            self.assertEqual(state.r[4], 0x804)
            for pc in (0xff, 0x106, 0x202, 0x700):
                state.pc = pc
                self.assertEqual(function(ctypes.byref(state), 1, 0xf00), 2)
            state.pc = 0x200
            memory[0x200] ^= 1
            self.assertEqual(function(ctypes.byref(state), 1, 0xf00), 6)
            memory[0x200] ^= 1
            for index, artifact in enumerate(manifest['dispatcher_support']):
                path = out/artifact['file']
                saved = path.read_bytes()
                path.write_bytes(saved+b'/* modified registry */')
                evidence = root/('tampered-support-%d'%index)
                with self.assertRaisesRegex(ValueError, 'source digest mismatch'):
                    asm_c_differential.verify(out, evidence, image, scenarios=1)
                self.assertFalse(evidence.exists())
                path.write_bytes(saved)
            manifest_path = out/'report.json'
            saved = manifest_path.read_bytes()
            for support in (manifest['dispatcher_support'][:-1], [None],
                            manifest['dispatcher_support']*2):
                bad = dict(manifest, dispatcher_support=support)
                manifest_path.write_text(json.dumps(bad))
                with self.assertRaisesRegex(ValueError, 'support inventory'):
                    asm_c_differential.verify(out, root/'bad-support', image, scenarios=1)
                self.assertFalse((root/'bad-support').exists())
            for size in (0, 2049, True):
                manifest_path.write_text(json.dumps(dict(manifest, dispatcher_shard_entries=size)))
                with self.assertRaisesRegex(ValueError, 'shard size'):
                    asm_c_differential.verify(out, root/'bad-shard-size', image, scenarios=1)
                self.assertFalse((root/'bad-shard-size').exists())
            manifest_path.write_bytes(saved)
            with (out / 'ef_lift_program.c').open('a') as stream:
                stream.write('/* modified dispatcher */')
            with self.assertRaisesRegex(ValueError, 'source digest mismatch'):
                asm_c_differential.verify(out, root / 'tampered-evidence', image, scenarios=1)
            self.assertFalse((root / 'tampered-evidence').exists())

    def test_ambiguous_instruction_ownership_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            _, _, records = inventory(root, '.type root,@function\nroot: ret\n.size root, .-root')
            with self.assertRaisesRegex(ValueError, 'overlapping instruction ownership'):
                asm_c_program.analyse(records + records)


if __name__ == '__main__':
    unittest.main()
