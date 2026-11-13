# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Synthetic instruction-to-C comparisons; no original executable required."""

import ctypes
import json
from pathlib import Path
import random
import shutil
import subprocess
import tempfile
import unittest

from tools import asm_inventory, asm_to_c, asm_c_differential

try:
    import unicorn
    from unicorn import x86_const
except ImportError:
    unicorn = None


State = asm_c_differential.State


def assemble(root, source):
    assembly, obj, image = root / 'fixture.s', root / 'fixture.o', root / 'fixture.bin'
    assembly.write_text('.intel_syntax noprefix\n.text\n.org 0x100\n' + source + '\n')
    subprocess.run(['as', '--32', '-o', str(obj), str(assembly)], check=True, capture_output=True)
    subprocess.run(['objcopy', '-O', 'binary', '-j', '.text', str(obj), str(image)], check=True)
    instructions = asm_inventory.decode_window(image, 0x100, image.stat().st_size - 0x100)
    record = dict(address=0x100, boundary='ghidra-body-ranges',
                  ghidra_instruction_layout_matches=True, overlapping_entries=[],
                  undecodable_addresses=[], ranges=[[0x100, image.stat().st_size]],
                  instructions=instructions)
    return image, record


@unittest.skipUnless(shutil.which('as') and shutil.which('objcopy') and shutil.which('cc'),
                     'requires GNU assembler, objcopy and a C compiler')
class TranslationTests(unittest.TestCase):
    def test_rejects_unsupported_or_uncertain_input(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for source in ('call eax\nret', 'cpuid\nret', 'jmp eax',
                           'in eax,dx\nret', 'push ax\nret', 'pop [eax]\nret',
                           'enter 8, 1\nleave\nret'):
                with self.subTest(source=source):
                    _, record = assemble(root, source)
                    with self.assertRaises(asm_to_c.Unsupported):
                        asm_to_c.translate(record)
            _, record = assemble(root, 'ret')
            for field, value in [('boundary', 'unverified-linear-window'),
                                 ('ghidra_instruction_layout_matches', False),
                                 ('overlapping_entries', [0x101])]:
                bad = dict(record, **{field: value})
                with self.assertRaises(asm_to_c.Unsupported):
                    asm_to_c.translate(bad)

    def test_rejects_external_edges_and_ambiguous_widths(self):
        for insn in [dict(address=0, bytes='eb00', mnemonic='jmp', operands='2', direct_target=2),
                     dict(address=0, bytes='90', mnemonic='mov', operands='[eax],1')]:
            with self.assertRaises(asm_to_c.Unsupported):
                asm_to_c.instruction_c(insn, {0, 1})

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_synthetic_programs_match_registers_flags_and_all_ram(self):
        sources = [
            'mov ah, bl\nmov al, 0x80\nmovzx edx, ah\nmovsx ecx, al\nret',
            'add eax, edx\nret', 'sub al, dl\nret', 'cmp ax, dx\nret',
            'inc ax\ndec ebx\nret', 'neg al\nret', 'neg eax\nret',
            'and ax, dx\nret', 'xor ah, dl\nret', 'or eax, ebx\nret', 'test al, bl\nret',
            'not ax\nret', 'clc\ncmc\nstc\nret',
            'push eax\npush esp\npop ebx\npop eax\nret 8',
            'lea edx, [esi+edi*4-8]\nret',
            'mov dword ptr [esi], eax\nadd word ptr [esi+4], dx\nmovzx ebx, byte ptr [esi+5]\nret',
            'movsx eax, byte ptr [esi]\nmovsx edx, word ptr [esi+2]\nret',
            'push ebp\nmov ebp, esp\nsub esp, 16\nmov eax, [ebp+4]\nleave\nret',
            'mov ecx, 7\n1: add eax, edx\ndec ecx\njne 1b\nret',
            'adc eax, edx\nret', 'sbb ax, dx\nret',
            'cbw\ncwde\ncdq\nret', 'cwd\nret',
            'enter 16, 0\nmov eax, [ebp+4]\nleave\nret',
            'enter 0, 0\nleave\nret',
            'mov dword ptr [0x4000], eax\nret',
            'shl eax, cl\nret', 'shr ax, cl\nret', 'sar al, cl\nret',
            'sar dword ptr [esi], 31\nret', 'shl byte ptr [esi], 9\nret',
            'imul eax, edx\nret', 'imul ax, dx\nret',
            'imul eax, [esi], -17\nret', 'imul ax, [esi], -123\nret',
            'imul dl\nret', 'imul dx\nret', 'imul edx\nret',
            'mul dl\nret', 'mul dx\nret', 'mul edx\nret',
            'ror eax, cl\nret', 'rcl ax, cl\nret', 'rcr al, cl\nret',
            'rol dword ptr [esi], 16\nret', 'ror byte ptr [esi], 8\nret',
            'bswap eax\nret', 'xchg ah, al\nret', 'xchg ax, dx\nret',
            'xchg eax, [esi]\nret', 'xchg esi, [esi]\nret',
            'xchg [esi], esi\nret', 'xchg word ptr [esi], dx\nret',
        ]
        sources += ['%s 1f\nmov eax, 0\nret\n1: mov eax, 1\nret' % branch
                    for branch in asm_to_c.CONDITIONS]
        sources += ['set%s al\nset%s byte ptr [esi]\nret' % (branch[1:], branch[1:])
                    for branch in asm_to_c.CONDITIONS]
        random_ = random.Random(0xEF2000)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
            for index, source in enumerate(sources):
                with self.subTest(index=index, source=source):
                    image, record = assemble(root, source)
                    code = root / 'lift.c'
                    code.write_text(asm_to_c.translate(record))
                    library = root / ('lift%d.so' % index)
                    subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-std=c99',
                                    '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
                                    str(code), '-o', str(library)], check=True, capture_output=True)
                    function = getattr(ctypes.CDLL(str(library)), 'ef_lift_00000100')
                    function.argtypes = [ctypes.POINTER(State), ctypes.c_uint32]
                    function.restype = ctypes.c_uint
                    for scenario in range(32):
                        memory = bytearray(random_.randbytes(65536))
                        memory[:image.stat().st_size] = image.read_bytes()
                        memory[0x8000:0x8004] = (0xff00).to_bytes(4, 'little')
                        registers = [random_.getrandbits(32) for _ in range(8)]
                        # Include overflow, carry and partial-register boundaries.
                        if scenario < 8:
                            registers[0] = [0, 1, 0x7f, 0x80, 0x7fff, 0x8000,
                                            0x7fffffff, 0xffffffff][scenario]
                            registers[2] = 1
                        registers[4:8] = [0x8000, 0x9000, 0x4000, 3]
                        flags = 0x202 | (random_.getrandbits(12) & 0x8d5)
                        cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
                        cpu.mem_map(0, len(memory))
                        cpu.mem_write(0, bytes(memory))
                        for name, value in zip(asm_to_c.REGS, registers):
                            cpu.reg_write(getattr(x86_const, 'UC_X86_REG_' + name.upper()), value)
                        cpu.reg_write(x86_const.UC_X86_REG_EFLAGS, flags)
                        cpu.emu_start(0x100, 0xff00, count=500)
                        native_memory = (ctypes.c_uint8 * len(memory)).from_buffer_copy(memory)
                        state = State((ctypes.c_uint32 * 8)(*registers), flags, 0x100, 0,
                                      native_memory, len(memory))
                        self.assertEqual(function(ctypes.byref(state), 500), 0)
                        expected = [cpu.reg_read(getattr(x86_const, 'UC_X86_REG_' + n.upper()))
                                    for n in asm_to_c.REGS]
                        self.assertEqual(list(state.r), expected, (index, scenario))
                        self.assertEqual(state.pc, cpu.reg_read(x86_const.UC_X86_REG_EIP))
                        self.assertEqual(state.flags, cpu.reg_read(x86_const.UC_X86_REG_EFLAGS),
                                         (index, scenario, source))
                        self.assertEqual(bytes(native_memory), bytes(cpu.mem_read(0, len(memory))))

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_exhaustive_byte_add_sub_adc_sbb_flags(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
            for mnemonic in ('add', 'sub', 'adc', 'sbb'):
                image, record = assemble(root, mnemonic + ' al, dl\nret')
                (root / 'lift.c').write_text(asm_to_c.translate(record))
                library = root / (mnemonic + '.so')
                subprocess.run(['cc', '-shared', '-fPIC', '-O2', str(root / 'lift.c'),
                                '-o', str(library)], check=True)
                function = ctypes.CDLL(str(library)).ef_lift_00000100
                function.argtypes = [ctypes.POINTER(State), ctypes.c_uint32]
                memory = bytearray(4096)
                memory[:image.stat().st_size] = image.read_bytes()
                memory[0x800:0x804] = (0xf00).to_bytes(4, 'little')
                native_memory = (ctypes.c_uint8 * len(memory)).from_buffer(memory)
                state = State((ctypes.c_uint32 * 8)(), 0x202, 0x100, 0, native_memory, len(memory))
                cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
                cpu.mem_map(0, len(memory))
                cpu.mem_write(0, bytes(memory))
                for carry in range(2 if mnemonic in ('adc', 'sbb') else 1):
                    for a in range(256):
                        for b in range(256):
                            state.r[0], state.r[2], state.r[4] = 0x12345600 | a, b, 0x800
                            state.flags, state.pc = 0x202 | carry, 0x100
                            cpu.reg_write(x86_const.UC_X86_REG_EAX, state.r[0])
                            cpu.reg_write(x86_const.UC_X86_REG_EDX, b)
                            cpu.reg_write(x86_const.UC_X86_REG_ESP, 0x800)
                            cpu.reg_write(x86_const.UC_X86_REG_EFLAGS, state.flags)
                            cpu.emu_start(0x100, 0xf00, count=10)
                            self.assertEqual(function(ctypes.byref(state), 10), 0)
                            self.assertEqual(state.r[0], cpu.reg_read(x86_const.UC_X86_REG_EAX),
                                             (mnemonic, carry, a, b))
                            self.assertEqual(state.flags, cpu.reg_read(x86_const.UC_X86_REG_EFLAGS),
                                             (mnemonic, carry, a, b))

    def scalar_grid(self, source, cases, *, allow_faults=False, oracle_unsafe=None):
        """Reuse one original CPU and native library for exhaustive small domains."""
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, record = assemble(root, source + '\nret')
            (root / 'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
            (root / 'lift.c').write_text(asm_to_c.translate(record))
            library = root / 'scalar.so'
            subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-std=c99',
                            '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            str(root / 'lift.c'), '-o', str(library)], check=True)
            function = ctypes.CDLL(str(library)).ef_lift_00000100
            function.argtypes = [ctypes.POINTER(State), ctypes.c_uint32]
            memory = bytearray(4096)
            memory[:image.stat().st_size] = image.read_bytes()
            memory[0x800:0x804] = (0xf00).to_bytes(4, 'little')
            native_memory = (ctypes.c_uint8 * len(memory)).from_buffer(memory)
            state = State((ctypes.c_uint32 * 8)(), 0x202, 0x100, 0, native_memory, len(memory))
            cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
            cpu.mem_map(0, len(memory))
            cpu.mem_write(0, bytes(memory))
            for case in cases:
                a, b, flags = case[:3]
                high = case[3] if len(case) == 4 else b
                initial = [a, b, high, 0x13572468, 0x800, 0x900, 0xa00, 0xb00]
                for index, (name, value) in enumerate(zip(asm_to_c.REGS, initial)):
                    state.r[index] = value
                    cpu.reg_write(getattr(x86_const, 'UC_X86_REG_' + name.upper()), value)
                state.flags, state.pc, state.fault = flags, 0x100, 0
                cpu.reg_write(x86_const.UC_X86_REG_EFLAGS, flags)
                if oracle_unsafe and oracle_unsafe(case):
                    # Checked against the architectural quotient bound here;
                    # the crashing oracle case has a separate process test.
                    self.assertEqual(function(ctypes.byref(state), 10), 5)
                    self.assertEqual(list(state.r), initial)
                    self.assertEqual(state.flags, flags)
                    self.assertEqual(state.pc, 0x100)
                    continue
                expected_status = 0
                try:
                    cpu.emu_start(0x100, 0xf00, count=10)
                except unicorn.UcError as error:
                    if not allow_faults:
                        raise
                    self.assertEqual(error.errno, unicorn.UC_ERR_EXCEPTION)
                    expected_status = 5
                self.assertEqual(function(ctypes.byref(state), 10), expected_status, (source, case))
                self.assertEqual(list(state.r), [cpu.reg_read(getattr(x86_const, 'UC_X86_REG_' + n.upper()))
                                                for n in asm_to_c.REGS], (source, a, b))
                self.assertEqual(state.flags, cpu.reg_read(x86_const.UC_X86_REG_EFLAGS),
                                 (source, a, b, hex(flags)))
                self.assertEqual(state.pc, cpu.reg_read(x86_const.UC_X86_REG_EIP))
                if expected_status == 0:
                    self.assertEqual(state.pc, 0xf00)
                else:
                    # Unicorn retains pending exception state after #DE. A new
                    # CPU is necessary before the next independent fault probe.
                    cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
                    cpu.mem_map(0, len(memory))
                    cpu.mem_write(0, bytes(memory))

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_shift_counts_all_byte_values_and_wide_boundaries(self):
        for operation in ('shl', 'shr', 'sar'):
            self.scalar_grid(operation + ' al, cl',
                             ((0x12345600 | a, count, 0xad7) for a in range(256)
                              for count in range(256)))
            for register, bits in [('ax', 16), ('eax', 32)]:
                values = [0, 1, (1 << (bits-1))-1, 1 << (bits-1), (1 << bits)-1]
                self.scalar_grid(operation + ' ' + register + ', cl',
                                 ((a, count, flags) for a in values for count in range(256)
                                  for flags in (0x202, 0xad7)))

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_multiply_all_byte_pairs_and_wide_boundaries(self):
        for operation in ('mul', 'imul'):
            self.scalar_grid(operation + ' dl',
                             ((0x12345600 | a, b, 0xad7) for a in range(256) for b in range(256)))
            for register, bits in [('dx', 16), ('edx', 32)]:
                values = [0, 1, 2, (1 << (bits-1))-1, 1 << (bits-1), (1 << bits)-1]
                self.scalar_grid(operation + ' ' + register,
                                 ((a, b, flags) for a in values for b in values for flags in (0x202, 0xad7)))
            if operation == 'imul':
                self.scalar_grid('imul eax, edx',
                                 ((a, b, 0xad7) for a in values for b in values))

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_rotate_all_byte_values_counts_carries_and_wide_boundaries(self):
        for operation in ('rol', 'ror', 'rcl', 'rcr'):
            self.scalar_grid(operation + ' al, cl',
                             ((0x12345600 | a, count, flags) for a in range(256)
                              for count in range(256) for flags in (0x202, 0xad7)))
            for register, width in [('ax', 16), ('eax', 32)]:
                values = [0, 1, (1 << (width-1))-1, 1 << (width-1), (1 << width)-1]
                self.scalar_grid(operation + ' ' + register + ', cl',
                                 ((a, count, flags) for a in values for count in range(256)
                                  for flags in (0x202, 0xad7)))
            for count in (0, 1, 2, 7, 8, 9, 16, 17, 31, 32, 33, 255):
                self.scalar_grid(operation + ' al, %d' % count,
                                 ((a, 0, flags) for a in range(256) for flags in (0x202, 0xad7)))

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_division_quotients_remainders_and_fault_boundaries(self):
        for operation in ('div', 'idiv'):
            self.scalar_grid(operation + ' cl',
                             ((0x12340000 | a, b, 0xad7) for a in range(256) for b in range(256)),
                             allow_faults=True)
            values = [0, 1, 127, 128, 255, 256, 32767, 32768, 65408, 65535]
            self.scalar_grid(operation + ' cl',
                             ((0x12340000 | a, b, flags) for a in values for b in range(256)
                              for flags in (0x202, 0xad7)), allow_faults=True)
            for register, width in [('cx', 16), ('ecx', 32)]:
                values = [0, 1, 2, (1 << (width-1))-1, 1 << (width-1), (1 << width)-1]
                self.scalar_grid(operation + ' ' + register,
                                 ((a, b, flags, high) for a in values for b in values for high in values
                                  for flags in (0x202, 0xad7)), allow_faults=True,
                                 oracle_unsafe=(lambda case: case[0] == 0 and case[1] == 0xffffffff and
                                                case[3] == 0x80000000)
                                 if operation == 'idiv' and width == 32 else None)

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_worker_crashes_preserve_evidence_and_distinguish_original_from_c(self):
        import os
        import signal
        from unittest.mock import patch
        calls = 0
        def crash_second_call(pointer, budget):
            nonlocal calls
            calls += 1
            if calls == 2:
                os.kill(os.getpid(), signal.SIGSEGV)
            state = ctypes.cast(pointer, ctypes.POINTER(asm_c_differential.State)).contents
            state.pc = 0x3ff000
            state.r[4] += 4
            return 0
        results = asm_c_differential.isolated_probes(crash_second_call, bytes(256) + b'\xc3',
                                                    0x100, 3, 10, False)
        self.assertEqual([r['state'] for r in results], ['pass', 'mismatch', 'inconclusive'])
        self.assertIn('after original completed', results[1]['differences'][0])
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, _ = assemble(root, 'idiv ecx\nret')
            registers = [0, 0xffffffff, 0x80000000, 0, 0x3f0000, 0, 0, 0]
            with patch.object(asm_c_differential, 'probes', return_value=iter([(registers, 0x202)])):
                results = asm_c_differential.isolated_probes(lambda pointer, budget: 5,
                                                            image.read_bytes(), 0x100, 1, 10, False)
            self.assertEqual(results[0]['state'], 'inconclusive')
            self.assertTrue('original oracle worker exited' in results[0].get('reason', '') or
                            'original fault' in results[0].get('reason', ''))

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_differential_rejects_wrong_translation_and_does_not_pass_original_fault(self):
        payload = bytes(256) + bytes.fromhex('c3')
        def wrong_translation(pointer, budget):
            state = ctypes.cast(pointer, ctypes.POINTER(asm_c_differential.State)).contents
            state.pc = 0x3ff000
            state.r[0] ^= 1
            state.r[4] += 4
            return 0
        registers, flags = next(asm_c_differential.probes(1, 1))
        report = asm_c_differential.compare(wrong_translation, payload, 0x100, registers, flags)
        self.assertEqual(report['state'], 'mismatch')
        self.assertTrue(any('eax' in d for d in report['differences']))
        for status in (1, 2, 3):
            report = asm_c_differential.compare(lambda pointer, budget: status,
                                                payload, 0x100, registers, flags)
            self.assertEqual(report['state'], 'mismatch')
            self.assertIn('after original completed', report['differences'][0])
        # UD2 must never count as semantic success just because both sides stop.
        report = asm_c_differential.compare(wrong_translation, bytes(256) + b'\x0f\x0b',
                                            0x100, registers, flags)
        self.assertEqual(report['state'], 'inconclusive')

    @unittest.skipUnless(unicorn, 'requires Python Unicorn')
    def test_generated_artifact_provenance_and_bulk_verifier(self):
        import hashlib
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, record = assemble(root, 'ret')
            inventory = root / 'inventory'
            inventory.mkdir()
            (inventory / 'entry.json').write_text(json.dumps(record))
            (inventory / 'manifest.json').write_text(json.dumps(dict(schema=3,
                boundary='ghidra-body-ranges',
                payload_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),
                entries=[dict(address=0x100, file='entry.json')])))
            directory = root / 'translations'
            asm_to_c.generate(inventory, directory, image)
            report = asm_c_differential.verify(directory, root / 'evidence', image, scenarios=2)
            self.assertEqual(report['entries'][0]['state'], 'probes-pass')
            self.assertEqual(report['entries'][0]['counts'], {'pass': 2})
            with (directory / 'ef_lift_00000100.c').open('a') as stream:
                stream.write('/* changed */')
            with self.assertRaisesRegex(ValueError, 'source digest mismatch'):
                asm_c_differential.verify(directory, root / 'bad-evidence', image, scenarios=2)
            self.assertFalse((root / 'bad-evidence').exists())

    def test_generate_redecodes_metadata_instead_of_trusting_byte_labels(self):
        import hashlib
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, record = assemble(root, 'ret')
            inventory = root / 'inventory'
            inventory.mkdir()
            # Keep the original byte but lie about its instruction semantics.
            record['instructions'][0]['mnemonic'] = 'nop'
            (inventory / 'entry.json').write_text(json.dumps(record))
            (inventory / 'manifest.json').write_text(json.dumps(dict(schema=3,
                boundary='ghidra-body-ranges',
                payload_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),
                entries=[dict(address=0x100, file='entry.json')])))
            with self.assertRaisesRegex(ValueError, 'decode mismatch'):
                asm_to_c.generate(inventory, root / 'out', image)
            self.assertFalse((root / 'out/report.json').exists())

    def test_memory_fault_and_instruction_budget_are_explicit(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            _, record = assemble(root, 'mov eax, [esi]\nret')
            (root / 'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
            (root / 'lift.c').write_text(asm_to_c.translate(record))
            library = root / 'lift.so'
            subprocess.run(['cc', '-shared', '-fPIC', '-O2', str(root / 'lift.c'),
                            '-o', str(library)], check=True)
            function = ctypes.CDLL(str(library)).ef_lift_00000100
            memory = (ctypes.c_uint8 * 8)()
            state = State((ctypes.c_uint32 * 8)(0, 0, 0, 0, 0, 0, 7, 0),
                          0x202, 0x100, 0, memory, 8)
            self.assertEqual(function(ctypes.byref(state), 0), 1)
            self.assertEqual(function(ctypes.byref(state), 5), 3)
            self.assertEqual(state.r[0], 0)


if __name__ == '__main__':
    unittest.main()
