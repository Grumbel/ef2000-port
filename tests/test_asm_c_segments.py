# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Selector state and string-operation comparisons using synthetic programs."""

import ctypes
import json
from pathlib import Path
import shutil
import tempfile
import unittest

from tools import asm_c_differential, asm_c_program, asm_to_c
from tests.test_asm_c_program import inventory
from tests.test_asm_to_c import unicorn


@unittest.skipUnless(all(shutil.which(t) for t in ('as', 'nm', 'objcopy', 'cc')),
                     'requires binutils and a C compiler')
class SegmentStringTests(unittest.TestCase):
    def verify_programs(self, programs, *, scenarios=2):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            # Distinct owned ranges exclude assembler padding from the bodies.
            source = '\n'.join('.org 0x%x\n.type test_%d,@function\ntest_%d:\n%s\nret\n.size test_%d, .-test_%d' %
                               (0x100 + index*0x100, index, index, program, index, index)
                               for index, program in enumerate(programs))
            image, directory, records = inventory(root, source)
            out = root / 'out'
            generated = asm_c_program.generate(directory, out, image)
            self.assertTrue(all(e['state'] == 'generated-unverified' for e in generated['entries']),
                            [e for e in generated['entries'] if e['state'] != 'generated-unverified'])
            report = asm_c_differential.verify(out, root / 'evidence', image, scenarios=scenarios)
            for index, entry in enumerate(report['entries']):
                self.assertEqual(entry['counts'], {'pass': scenarios}, (programs[index], entry))
            return report

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_patched_mov_immediates_are_data_but_opcode_changes_are_not(self):
        programs = []
        for register, width, offset, value in [('al', 8, 1, 0x80), ('ah', 8, 1, 0xff),
                                                ('ax', 16, 2, 0x8123), ('eax', 32, 1, 0x80000001),
                                                ('edx', 32, 1, 0xffffffff)]:
            name = 'patch_' + register
            programs.append('mov %s ptr [%s+%d],0x%x\n%s: mov %s,0x11' %
                            ({8: 'byte', 16: 'word', 32: 'dword'}[width], name, offset, value, name, register))
        programs += [
            'mov esi,17\nmov dword ptr [patch_add+2],0xfffffffd\npatch_add: add esi,0x12345678',
            'mov esi,17\nmov byte ptr [patch_add8+2],0xf8\npatch_add8: add esi,1',
            'mov si,0x1234\nmov byte ptr [patch_add16+3],0xfe\npatch_add16: add si,1',
            'mov al,0x81\nmov byte ptr [patch_shift+2],7\npatch_shift: shl al,0',
        ]
        self.verify_programs(programs)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            # MOV AL -> MOV CL has the same instruction length and immediate
            # width, but changes the operation and must still fail the guard.
            source = '.type root,@function\nroot: mov byte ptr [patched],0xb1\npatched: mov al,17\nret\n.size root, .-root'
            image, directory, _ = inventory(root, source)
            out = root / 'out'; asm_c_program.generate(directory, out, image)
            report = asm_c_differential.verify(out, root/'evidence', image, scenarios=2)
            self.assertEqual(report['entries'][0]['counts'], {'mismatch': 2})
            self.assertTrue(all('C status 6' in p['differences'][0] for p in report['entries'][0]['probes']))

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_segment_moves_push_pop_and_far_pointer_loads(self):
        programs = []
        for segment in asm_to_c.SEGMENTS:
            programs.append('mov eax,0x12345678\nmov ax,%s\nmov edx,%s\n'
                            'mov dword ptr [esp-4],0xaabbccdd\npush %s\npop ebx\n'
                            'mov word ptr [0x300400],%s' % (segment, segment, segment, segment))
        for segment in ('es', 'ds', 'fs', 'gs', 'ss'):
            for selector in ((16, 24) if segment == 'ss' else (0, 1, 2, 3, 8, 16, 24)):
                programs.append('push %s\nmov eax,0x12340000+%d\nmov %s,eax\nmov edx,%s\n'
                                'mov dword ptr %s:[0x300400],eax\npop %s' %
                                (segment, selector, segment, segment, segment, segment))
        for operation in ('les', 'lds', 'lfs', 'lgs', 'lss'):
            for register, width in [('ax', 16), ('eax', 32)]:
                programs.append('mov esi,0x300400\nmov eax,0x12345678\n'
                                'mov %s ptr [esi],0x500\nmov word ptr [esi+%d],24\n%s %s,[esi]' %
                                ('word' if width == 16 else 'dword', width//8, operation, register))
        programs += ['mov esi,0x300400\nmov dword ptr [esi],0x300500\n'
                     'mov word ptr [esi+4],24\nles esi,[esi]',
                     'mov esi,0x300400\nmov [esi],esp\nmov word ptr [esi+4],24\nlss esp,[esi]']
        self.verify_programs(programs)

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_string_operations_directions_counts_overlap_and_early_exit(self):
        programs = []
        for mnemonic in ('movs', 'stos', 'lods', 'cmps', 'scas'):
            for size in ('b', 'w', 'd'):
                for direction in ('cld', 'std'):
                    for repeat, count in [('', 3), ('rep', 0), ('rep', 1), ('rep', 7)]:
                        prefixes = ('repe', 'repne') if repeat and mnemonic in ('cmps', 'scas') else (repeat,)
                        for prefix in prefixes:
                            # Equal initial regions exercise terminal zero checks;
                            # SCAS encounters a zero then a nonzero byte/word.
                            programs.append('mov esi,0x300880\nmov edi,0x300980\n'
                                            'mov eax,0x80808080\nmov ecx,%d\n%s\n%s %s%s' %
                                            (count, direction, prefix, mnemonic, size))
        for size, width in [('b', 1), ('w', 2), ('d', 4)]:
            for direction in ('cld', 'std'):
                for displacement in (-width, -1, 0, 1, width):
                    programs.append('mov esi,0x300880\nmov edi,0x300880+%d\n'
                                    'mov ecx,7\n%s\nrep movs%s' % (displacement, direction, size))
        self.verify_programs(programs)

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_rep_budget_and_resume_match_the_original_at_every_step(self):
        for operation in ('rep movsb', 'repe cmpsb', 'repne cmpsb', 'repe scasb', 'repne scasb'):
            with self.subTest(operation=operation), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                image, directory, records = inventory(root, '.type root,@function\nroot: ' + operation +
                                                       '\nret\n.size root, .-root')
                out = root / 'out'
                asm_c_program.generate(directory, out, image)
                # Compile through the ordinary verifier before testing step budgets.
                asm_c_differential.verify(out, root / 'evidence', image, scenarios=1)
                run = ctypes.CDLL(str(root / 'evidence/translations.so')).ef_lift_run
                run.argtypes = [ctypes.POINTER(asm_c_differential.State), ctypes.c_uint32, ctypes.c_uint32]
                for count in (0, 1, 2, 7):
                    for flags in (0x202, 0x646):
                        memory = bytearray(4096)
                        memory[:image.stat().st_size] = image.read_bytes()
                        memory[0x400:0x410] = bytes(range(16))
                        memory[0x500:0x510] = bytes(range(16))
                        memory[0x800:0x804] = (0xf00).to_bytes(4, 'little')
                        array = (ctypes.c_uint8 * len(memory)).from_buffer_copy(memory)
                        registers = [0, count, 0, 0, 0x800, 0, 0x408, 0x508]
                        state = asm_c_differential.State((ctypes.c_uint32*8)(*registers), flags, 0x100, 0,
                                                         array, len(memory))
                        cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
                        cpu.mem_map(0, len(memory)); cpu.mem_write(0, bytes(memory))
                        for name, value in zip(asm_to_c.REGS, registers):
                            cpu.reg_write(getattr(unicorn.x86_const, 'UC_X86_REG_' + name.upper()), value)
                        cpu.reg_write(unicorn.x86_const.UC_X86_REG_EFLAGS, flags)
                        for step in range(12):
                            cpu.emu_start(cpu.reg_read(unicorn.x86_const.UC_X86_REG_EIP) or 0x100,
                                          0xf00, count=1)
                            status = run(ctypes.byref(state), 1, 0xf00)
                            self.assertEqual(list(state.r), [cpu.reg_read(getattr(unicorn.x86_const, 'UC_X86_REG_' + n.upper()))
                                                            for n in asm_to_c.REGS], (operation, count, step))
                            self.assertEqual(state.flags, cpu.reg_read(unicorn.x86_const.UC_X86_REG_EFLAGS))
                            self.assertEqual(state.pc, cpu.reg_read(unicorn.x86_const.UC_X86_REG_EIP))
                            self.assertEqual(bytes(array), bytes(cpu.mem_read(0, len(memory))))
                            if status == 0:
                                break
                        self.assertEqual(status, 0, (operation, count))

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_selector_outputs_and_state_abi_are_checked(self):
        def wrong_selector(pointer, budget):
            state = ctypes.cast(pointer, ctypes.POINTER(asm_c_differential.State)).contents
            state.pc = 0x3ff000; state.r[4] += 4; state.segment[0] = 24
            return 0
        registers, flags = next(asm_c_differential.probes(1, 1))
        report = asm_c_differential.compare(wrong_selector, bytes(256) + b'\xc3', 0x100, registers, flags)
        self.assertEqual(report['state'], 'mismatch')
        self.assertTrue(any(d.startswith('es:') for d in report['differences']))
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, directory, _ = inventory(root, '.type root,@function\nroot: ret\n.size root, .-root')
            out = root / 'out'; asm_c_program.generate(directory, out, image)
            manifest = json.loads((out/'report.json').read_text()); manifest['state_abi'] = 0
            (out/'report.json').write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, 'state ABI'):
                asm_c_differential.verify(out, root/'bad-evidence', image)
            self.assertFalse((root/'bad-evidence').exists())


if __name__ == '__main__':
    unittest.main()
