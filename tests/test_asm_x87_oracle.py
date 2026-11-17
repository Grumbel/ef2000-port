# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Independent coprocessor integration and strict comparison regression tests."""

import ctypes
import json
import platform
from pathlib import Path
import shutil
import signal
import struct
import subprocess
import tempfile
import unittest
from unittest import mock

from tools import asm_x87_oracle as oracle, asm_c_differential as diff, asm_to_c
from tests.test_asm_to_c import assemble, unicorn
from tests.test_asm_c_program import inventory


class DecodeTests(unittest.TestCase):
    def test_segments_sib_signed_displacements_and_wrap(self):
        registers = dict(zip(oracle.REGISTERS, (0xffffffff, 3, 5, 0x2000, 0x3000, 0x4000, 0x5000, 0x6000)))
        for encoded, address, segment in [
                ('d900', 0xffffffff, 'DS'), ('d945fc', 0x3ffc, 'SS'),
                ('26d9448d10', 0x401c, 'ES'), ('d904247f', None, None),
                ('d904247f000000', None, None), ('d90424', 0x3000, 'SS'),
                ('d9042578563412', 0x12345678, 'DS'),
                ('d9048dfcffffff', 8, 'DS'), ('d9444801', 6, 'DS')]:
            if address is None:
                with self.assertRaises(oracle.Unavailable):
                    oracle.decode(bytes.fromhex(encoded), registers)
            else:
                result = oracle.decode(bytes.fromhex(encoded), registers)
                self.assertEqual((result['address'], result['segment']), (address, segment), encoded)
        for encoded in ('66d900', '67d900', 'f3d900', '26d9e8', 'd9f5', 'df0c24', 'd9', ''):
            with self.assertRaises(oracle.Unavailable, msg=encoded):
                oracle.decode(bytes.fromhex(encoded), registers)


@unittest.skipUnless(unicorn and platform.system() == 'Linux' and platform.machine() == 'x86_64'
                     and all(shutil.which(t) for t in ('as', 'objcopy', 'cc')),
                     'requires Linux x86-64, Unicorn, binutils and cc')
class OracleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory()
        cls.root = Path(cls.temporary.name)
        cls.native, cls.report = oracle.build(cls.root/'native')

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def test_trigonometric_raw_results_range_and_stack_effects(self):
        # Exact architectural identities, independent of translator helpers.
        one = [0, 0x80000000, 0x3fff]
        for top in range(8):
            for negative in (0, 0x8000):
                for operation, expected in ((0xd9fe, [0, 0, negative]),
                                            (0xd9ff, one)):
                    values = dict(x87_control=0x37f, x87_status=top << 11,
                                  x87_tag=0, x87_opcode=0x321, x87=[one[:] for _ in range(8)])
                    values['x87'][top] = [0, 0, negative]
                    result, data, ax, _, _, _ = self.native.execute(operation, values, b'', 0x12345678)
                    self.assertEqual(result['x87'][top], expected)
                    self.assertEqual(result['x87_status'] & 0x7fff, top << 11)
                    self.assertEqual((data, ax), (b'', 0x12345678))
                    self.assertEqual(result['x87_control'], values['x87_control'])
                    self.assertEqual((result['x87_tag'] >> (2*top)) & 3,
                                     1 if operation == 0xd9fe else 0)
                # FPATAN computes atan2(ST1, ST0), writes ST1 and pops ST0.
                values['x87'][top] = one[:]
                values['x87'][(top+1) & 7] = [0, 0, negative]
                result, _, _, _, _, _ = self.native.execute(0xd9f3, values, b'', 0)
                self.assertEqual(result['x87'][(top+1) & 7], [0, 0, negative])
                self.assertEqual((result['x87_status'] >> 11) & 7, (top+1) & 7)
                self.assertEqual((result['x87_tag'] >> (2*top)) & 3, 3)
            for operation in (0xd9fe, 0xd9ff):
                # |ST0| >= 2**63 is left unchanged and sets C2.
                values['x87'][top] = [0, 0x80000000, 0x403e]
                values['x87_status'] = top << 11
                result, _, _, _, _, _ = self.native.execute(operation, values, b'', 0)
                self.assertEqual(result['x87'][top], values['x87'][top])
                self.assertEqual(result['x87_status'] & 0x7fff, (top << 11) | 0x400)
        for encoded in ('d9fe', 'd9ff', 'd9f3'):
            self.assertEqual(oracle.decode(bytes.fromhex(encoded), {})['operation'],
                             int(encoded, 16))

    def test_trigonometric_push_and_range_rejection(self):
        one = [0, 0x80000000, 0x3fff]
        out_of_range = [0, 0x80000000, 0x403e]
        for operation in (0xd9f2, 0xd9fb):
            for top in range(8):
                pushed = (top - 1) & 7
                for operand in ([0, 0, 0], [0, 0, 0x8000], out_of_range):
                    with self.subTest(operation=operation, top=top, operand=operand):
                        values = dict(x87_control=0x37f, x87_status=top << 11,
                                      x87_tag=3 << (2 * pushed), x87_opcode=0,
                                      x87=[one[:] for _ in range(8)])
                        values['x87'][top] = operand
                        result, _, _, _, _, _ = self.native.execute(operation, values, b'', 0)
                        self.assertEqual(result['x87'][top], operand)
                        if operand == out_of_range:
                            self.assertEqual(result['x87_status'] & 0x7fff, (top << 11) | 0x400)
                            self.assertEqual(result['x87_tag'], values['x87_tag'])
                        else:
                            self.assertEqual(result['x87'][pushed], one)
                            self.assertEqual(result['x87_status'] & 0x7fff, pushed << 11)
                            self.assertEqual((result['x87_tag'] >> (2 * pushed)) & 3, 0)

    def test_exponential_logarithm_and_remainder_exact_operands(self):
        one = [0, 0x80000000, 0x3fff]
        two = [0, 0x80000000, 0x4000]
        three = [0, 0xc0000000, 0x4000]
        for operation, left, right, pops in (
                (0xd9f0, one, two, False),    # 2**1 - 1
                (0xd9f1, two, one, True),     # 1 * log2(2)
                (0xd9f8, three, two, False)): # 3 % 2, quotient 1
            decoded = oracle.decode(operation.to_bytes(2, 'big'), {})
            self.assertEqual(decoded['operation'], operation)
            for top in range(8):
                with self.subTest(operation=operation, top=top):
                    next_top = (top + 1) & 7
                    values = dict(x87_control=0x37f, x87_status=top << 11,
                                  x87_tag=0, x87_opcode=0, x87=[one[:] for _ in range(8)])
                    values['x87'][top], values['x87'][next_top] = left, right
                    result, data, ax, _, _, _ = self.native.execute(operation, values, b'', 17)
                    destination = next_top if pops else top
                    expected_registers = list(values['x87'])
                    expected_registers[destination] = one
                    self.assertEqual(result['x87'], expected_registers)
                    self.assertEqual((result['x87_status'] >> 11) & 7, destination)
                    self.assertEqual(result['x87_tag'], (3 << (top * 2)) if pops else 0)
                    if operation == 0xd9f8:
                        self.assertEqual(result['x87_status'] & 0x4700, 0x200)
                    self.assertEqual((data, ax), (b'', 17))

    def test_native_observation_defers_signals_and_restores_mask(self):
        # Inject a real asynchronous signal inside dispatch. It must arrive
        # after dispatch returns, including the unsupported-operation path.
        source = oracle.source().replace(
            'static uintptr_t ef_x87_dispatch(',
            'static volatile sig_atomic_t ef_test_seen;\n'
            'static void ef_test_handler(int n) { (void)n; ++ef_test_seen; }\n'
            'unsigned ef_test_signal_count(void) { return ef_test_seen; }\n'
            'static uintptr_t ef_x87_dispatch(').replace(
            'switch(op) {',
            'signal(SIGUSR1,ef_test_handler); ef_test_seen=0;\n'
            'raise(SIGUSR1); if (ef_test_seen) return 0;\n'
            'switch(op) {')
        previous_handler = signal.getsignal(signal.SIGUSR1)
        previous_mask = signal.pthread_sigmask(signal.SIG_BLOCK, [])
        expected_mask = (previous_mask | {signal.SIGUSR2}) - {signal.SIGUSR1}
        try:
            signal.pthread_sigmask(signal.SIG_SETMASK, expected_mask)
            with mock.patch.object(oracle, 'source', return_value=source):
                native, _ = oracle.build(self.root/'signal-observation')
            values = dict(x87_control=0x37f, x87_status=0, x87_tag=0,
                          x87_opcode=0x321,
                          x87=[[0, 0x80000000, 0x3fff] for _ in range(8)])
            result, _, _, _, _, _ = native.execute(0xd8d1, values, b'', 0)
            self.assertEqual((result['x87_dp'], result['x87_ds']),
                             (0xdecaf002, 0x5678))
            self.assertEqual(native.library.ef_test_signal_count(), 1)
            self.assertEqual(signal.pthread_sigmask(signal.SIG_BLOCK, []), expected_mask)
            self.assertEqual(native.run(0, None, None, None, None), 0)
            self.assertEqual(native.library.ef_test_signal_count(), 1)
            self.assertEqual(signal.pthread_sigmask(signal.SIG_BLOCK, []), expected_mask)
        finally:
            signal.signal(signal.SIGUSR1, previous_handler)
            signal.pthread_sigmask(signal.SIG_SETMASK, previous_mask)

    def compiled(self, root, source):
        image, record = assemble(root, source)
        (root/'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
        (root/'lifted.c').write_text(asm_to_c.translate(record))
        subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-std=c99', '-Wall', '-Wextra', '-Werror',
                        '-Wno-unused-function', str(root/'lifted.c'), '-o', str(root/'lifted.so')], check=True)
        library = ctypes.CDLL(str(root/'lifted.so'))
        function = library.ef_lift_00000100
        function.argtypes, function.restype = [ctypes.POINTER(diff.State), ctypes.c_uint32], ctypes.c_uint
        return image.read_bytes(), function

    def compare(self, function, payload, **kwargs):
        return diff.compare(function, payload, 0x100, [17, 19, 23, 29, 0x3f0000, 31, 37, 41],
                            0xad7, native_x87=self.native, **kwargs)

    def test_all_modrm_sib_addresses_against_independent_unicorn_accesses(self):
        x = unicorn.x86_const
        cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
        cpu.mem_map(0, 0x100000)
        diff.bind_flat_segments(cpu)
        registers = dict(zip(oracle.REGISTERS, (0x1000, 0x20, 0x10, 0x2000, 0x3000, 0x4000, 0x5000, 0x6000)))
        accesses = []
        cpu.hook_add(unicorn.UC_HOOK_MEM_READ, lambda u, access, address, size, value, data: accesses.append(address))
        cases = 0
        for mode in range(3):
            for base in range(8):
                sibs = range(256) if base == 4 else (None,)
                for sib in sibs:
                    for prefix in (b'', b'\x26', b'\x36', b'\x64'):
                        encoded = prefix+bytes((0xd9, mode << 6 | base))
                        if sib is not None:
                            encoded += bytes((sib,))
                        absolute = mode == 0 and (sib & 7 if sib is not None else base) == 5
                        if absolute or mode == 2:
                            encoded += struct.pack('<i', 7 if absolute else -7)
                        elif mode == 1:
                            encoded += b'\xf9'
                        decoded = oracle.decode(encoded, registers)
                        for name, value in registers.items():
                            cpu.reg_write(getattr(x, 'UC_X86_REG_'+name), value)
                        diff.bind_x87(cpu)
                        cpu.mem_write(0x80000, encoded)
                        accesses.clear()
                        cpu.emu_start(0x80000, 0x80000+len(encoded), count=1)
                        self.assertTrue(accesses, encoded.hex())
                        self.assertEqual(decoded['address'], accesses[0], encoded.hex())
                        cases += 1
        self.assertEqual(cases, 3156)

    def test_control_and_raw_memory_round_trip_without_softfloat(self):
        from tests.test_asm_c_x87 import seeded_x87
        sources = ['fninit\nret', 'fnclex\nret', 'fnstsw ax\nret',
                   'fnstcw word ptr [0x300200]\nfnstsw word ptr [0x300202]\nret',
                   'mov word ptr [0x300100],0xffff\nfldcw word ptr [0x300100]\nret',
                   'fld1\nfstp tbyte ptr [0x300200]\nfld tbyte ptr [0x300200]\nret',
                   'fldl2e\nfstp tbyte ptr [0x300200]\nret',
                   'fldlg2\nfstp tbyte ptr [0x300200]\nret',
                   'fldln2\nfstp tbyte ptr [0x300200]\nfldpi\nfldl2t\nret',
                   'fldz\nfchs\nfabs\nfld st(0)\nfxch st(1)\nfstp st(1)\nffree st(0)\nfincstp\nfdecstp\nret']
        for source in sources:
            with self.subTest(source=source), tempfile.TemporaryDirectory() as temporary:
                payload, function = self.compiled(Path(temporary), source)
                initial = dict(seeded_x87(), x87_status=0x6d21)
                if source.startswith('fld'):
                    initial.update(x87_status=0, x87_tag=0xffff)
                result = self.compare(function, payload, initial_x87=initial)
                self.assertEqual(result['state'], 'pass', result)
                self.assertTrue(result['original_x87']['operations'])

    def test_signed_integer_comparison_matches_native_state(self):
        for width, values in (('word', (-32768, 0, 32767)),
                              ('dword', (-2147483648, 0, 2147483647))):
            for mnemonic in ('ficom', 'ficomp'):
                for value in values:
                    with self.subTest(width=width, mnemonic=mnemonic, value=value):
                        with tempfile.TemporaryDirectory() as temporary:
                            payload, function = self.compiled(Path(temporary),
                                f'mov {width} ptr [0x300100], {value}\nfldz\n'
                                f'{mnemonic} {width} ptr [0x300100]\nret')
                            result = self.compare(function, payload)
                            self.assertEqual(result['state'], 'pass', result)

    def test_hardware_result_does_not_waive_any_compared_state(self):
        with tempfile.TemporaryDirectory() as temporary:
            payload, function = self.compiled(Path(temporary), 'fld1\nfstp tbyte ptr [0x300200]\nfnstsw ax\nret')
            baseline = self.compare(function, payload)
            self.assertEqual(baseline['state'], 'pass', baseline)
            mutations = [(field, lambda state, field=field: setattr(state, field, getattr(state, field)^1))
                         for field in diff.X87_FIELDS]
            mutations += [('raw FP%d' % index, lambda state, index=index: state.x87[index].__setitem__(0, state.x87[index][0]^1))
                          for index in range(8)]
            mutations += [('eax', lambda state: state.r.__setitem__(0, state.r[0]^1)),
                          ('comparison profile', lambda state: setattr(state, 'x87_compare_profile', state.x87_compare_profile^1)),
                          ('flags', lambda state: setattr(state, 'flags', state.flags^1)),
                          ('return PC', lambda state: setattr(state, 'pc', state.pc^1)),
                          ('RAM', lambda state: state.memory.__setitem__(0x300200, state.memory[0x300200]^1))]
            for name, mutation in mutations:
                def wrong(pointer, fuel):
                    status = function(pointer, fuel)
                    mutation(ctypes.cast(pointer, ctypes.POINTER(diff.State)).contents)
                    return status
                result = self.compare(wrong, payload)
                self.assertEqual(result['state'], 'mismatch', (name, result))
                self.assertTrue(result['differences'], name)

    def test_unsupported_prefixes_delivery_and_budget_are_inconclusive(self):
        def must_not_run(*args):
            self.fail('C must not run after an unobserved original execution')
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for source, reason in [('fprem1\nret', 'register encoding'),
                                   ('.byte 0x66,0xd9,0xe8\nret', 'prefix'),
                                   ('fwait\nret', 'pending'),
                                   ('fld1\nret', 'budget')]:
                image, _ = assemble(root, source)
                kwargs = dict(budget=1) if reason == 'budget' else {}
                if reason == 'pending':
                    kwargs['initial_x87'] = dict(x87_control=0x37e, x87_status=0x8081)
                result = self.compare(must_not_run, image.read_bytes(), **kwargs)
                self.assertEqual(result['state'], 'inconclusive', result)
                self.assertIn(reason, result['reason'])
        values = dict(x87_control=0x37f, x87_status=0, x87_tag=0xffff, x87_opcode=0,
                      x87=[[0, 0, 0] for _ in range(8)])
        for operation, data in ((0xd916, b''), (0xd9f5, b'')):
            with self.assertRaises(oracle.Unavailable):
                self.native.execute(operation, values, data, 0)

    def test_report_explicitly_binds_the_original_oracle(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, inv, _ = inventory(root, '.type root,@function\nroot: fld1\nfstp tbyte ptr [0x300200]\nret\n.size root,.-root')
            asm_to_c.generate(inv, root/'out', image)
            report = diff.verify(root/'out', root/'evidence', image, scenarios=1, x87_oracle='hardware')
            self.assertEqual(report['original_x87_oracle']['model'], 'unicorn-with-native-x87-v1')
            self.assertTrue(report['original_x87_oracle']['source_sha256'])
            self.assertEqual(report['entries'][0]['counts'], {'pass': 1})
            self.assertTrue(report['entries'][0]['probes'][0]['original_x87']['operations'])
