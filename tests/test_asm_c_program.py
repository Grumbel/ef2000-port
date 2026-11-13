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
            self.assertTrue(all('C status 6' in p['differences'][0]
                                for p in report['entries'][0]['probes']))

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
.size leaf, .-leaf'''
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
            self.assertFalse((root / 'out/ef_lift_00000100.c').exists())
            self.assertEqual(sum(e['state'] == 'generated-unverified' for e in result['entries']), 1)

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
            asm_c_program.generate(directory, out, image)
            asm_c_differential.verify(out, root / 'evidence', image, scenarios=1)
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
            state.pc = 0x700
            self.assertEqual(function(ctypes.byref(state), 1, 0xf00), 2)
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
