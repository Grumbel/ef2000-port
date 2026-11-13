# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Hardware-backed extended-memory x87 tests; no game data."""

import ctypes
import multiprocessing
from pathlib import Path
import platform
import resource
import shutil
import signal
import struct
import subprocess
import tempfile
import unittest

from tools import asm_to_c, asm_x87, asm_c_differential as diff
from tests.test_asm_to_c import assemble


@unittest.skipUnless(platform.machine() == 'x86_64' and
                     all(shutil.which(t) for t in ('cc', 'as', 'objcopy')),
                     'requires x86-64, C compiler and binutils')
class X87MemoryTests(unittest.TestCase):
    def test_extended_transfers_and_fault_priority_against_hardware(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
            probes, lifted, records = [], [], []
            for index, mnemonic in enumerate(('fld', 'fstp')):
                _, record = assemble(root, mnemonic + ' tbyte ptr [esi]\nret')
                records.append(record)
                opcode = ','.join(hex(v) for v in bytes.fromhex(record['instructions'][0]['bytes']))
                lifted.append(asm_to_c.translate(record).replace('ef_lift_00000100', 'lift_%d' % index))
                probes.append('''#include <stdint.h>
uintptr_t op%d(const void *initial, void *out, void *data) {
    unsigned char saved[108]; uintptr_t pc;
    __asm__ volatile("fnsave %%[save]\\n\\tfrstor (%%[initial])\\n\\t1: .byte %s\\n\\t"
                     "fnsave (%%[out])\\n\\tfrstor %%[save]\\n\\tleaq 1b(%%%%rip),%%[pc]"
                     : [save] "+m"(saved), [pc] "=r"(pc)
                     : [initial] "r"(initial), [out] "r"(out), "S"(data) : "memory");
    return pc;
}''' % (index, opcode))
            (root / 'hardware.c').write_text('\n'.join(probes))
            (root / 'lifted.c').write_text('\n'.join(lifted))
            for name in ('hardware', 'lifted'):
                command = ['cc', '-shared', '-fPIC', '-O2', '-std=c99', '-Wall', '-Wextra', '-Werror',
                           '-Wno-unused-function', str(root / (name + '.c')), '-o', str(root / (name + '.so'))]
                if name == 'lifted':
                    command += ['-fsanitize=undefined', '-fno-sanitize-recover=all']
                subprocess.run(command, check=True)
            hardware, generated = ctypes.CDLL(str(root / 'hardware.so')), ctypes.CDLL(str(root / 'lifted.so'))
            payloads = [(0x8000000000000001, 0x3fff), (0, 0), (0, 0x8000),
                        (0xc000000000000003, 0x7fff), (0x8000000000000000, 0x7fff),
                        (0x8000000000000001, 0x7fff), (0x800, 0),
                        (0x4000000000000000, 0x3fff), (0x8000000000000000, 0),
                        (0xc000000000000003, 0xffff)]
            values, classes = payloads[:8], [0, 1, 1, 2, 2, 2, 2, 2]
            payload_classes = [0, 1, 1, 2, 2, 2, 2, 2, 2, 2]
            memory = bytearray(b'\x5a' * 4096)
            memory[0x800:0x804] = (0xf00).to_bytes(4, 'little')
            ram = (ctypes.c_uint8 * len(memory)).from_buffer(memory)
            output = (ctypes.c_uint8 * 108)()
            cases = 0
            for index in range(2):
                native = getattr(hardware, 'op%d' % index)
                native.argtypes, native.restype = [ctypes.c_void_p] * 3, ctypes.c_uint64
                function = getattr(generated, 'lift_%d' % index)
                function.argtypes, function.restype = [ctypes.POINTER(diff.State), ctypes.c_uint32], ctypes.c_uint
                for masked in (False, True):
                    for top in range(8):
                        for occupied in range(256):
                            initial = bytearray(108)
                            control, status = 0x37e | masked, 0x4700 | (top << 11)
                            tags = sum((classes[i] if occupied & (1 << i) else 3) << (i*2) for i in range(8))
                            struct.pack_into('<H', initial, 0, control)
                            struct.pack_into('<H', initial, 4, status)
                            struct.pack_into('<H', initial, 8, tags)
                            struct.pack_into('<IHHIH', initial, 12, 0x123456, 16, 0x321, 0x234567, 16)
                            for logical in range(8):
                                struct.pack_into('<QH', initial, 28+logical*10, *values[(top+logical)&7])
                            saved = (ctypes.c_uint8 * 108).from_buffer(initial)
                            for payload, payload_class in zip(payloads, payload_classes):
                                physical_values, operand_tags = list(values), tags
                                if index == 1:
                                    # Vary the actual store source, even when
                                    # its tag is empty, not just destination RAM.
                                    physical_values[top] = payload
                                    struct.pack_into('<QH', initial, 28, *payload)
                                    if occupied & (1 << top):
                                        operand_tags = (tags & ~(3 << (top*2))) | (payload_class << (top*2))
                                    struct.pack_into('<H', initial, 8, operand_tags)
                                data = (ctypes.c_uint8 * 10).from_buffer_copy(struct.pack('<QH', *payload))
                                instruction = native(saved, output, data)
                                observed = bytes(output)
                                memory[0x500:0x50a] = struct.pack('<QH', *payload)
                                state = diff.State((ctypes.c_uint32 * 8)(), 0xad7, 0x100, 0, ram, len(memory))
                                state.r[4], state.r[6] = 0x800, 0x500
                                state.segment[:] = [16, 8, 24, 16, 16, 16]
                                state.x87_control, state.x87_status, state.x87_tag = control, status, operand_tags
                                state.x87_ip, state.x87_cs, state.x87_opcode = 0x123456, 16, 0x321
                                state.x87_dp, state.x87_ds = 0x234567, 16
                                for physical, (significand, exponent) in enumerate(physical_values):
                                    state.x87[physical][:] = [significand & 0xffffffff, significand >> 32, exponent]
                                context = (index, masked, top, hex(tags), payload)
                                self.assertEqual(function(ctypes.byref(state), 10), 0, context)
                                expected_status = struct.unpack_from('<H', observed, 4)[0]
                                expected_top = (expected_status >> 11) & 7
                                expected = [struct.unpack_from('<QH', observed, 28+((i-expected_top)&7)*10)
                                            for i in range(8)]
                                self.assertEqual([(r[0] | (r[1] << 32), r[2]) for r in state.x87], expected, context)
                                self.assertEqual((state.x87_status, state.x87_tag),
                                    (expected_status, struct.unpack_from('<H', observed, 8)[0]), context)
                                self.assertEqual(bytes(memory[0x500:0x50a]), bytes(data), context)
                                self.assertEqual(memory[0x4ff], 0x5a)
                                self.assertEqual(memory[0x50a], 0x5a)
                                self.assertEqual(struct.unpack_from('<I', observed, 12)[0], instruction & 0xffffffff)
                                self.assertEqual(struct.unpack_from('<I', observed, 20)[0], ctypes.addressof(data) & 0xffffffff)
                                self.assertEqual((state.x87_ip, state.x87_cs, state.x87_dp, state.x87_ds),
                                                 (0x100, 8, 0x500, 16), context)
                                self.assertEqual(state.x87_opcode, struct.unpack_from('<H', observed, 18)[0] & 0x7ff)
                                self.assertEqual((state.pc, state.r[4], state.flags), (0xf00, 0x804, 0xad7), context)
                                cases += 1
                # Bounds checks precede any stack/status change or partial store.
                for size, address in [(4096, 4090), (9, 0), (4096, 0xffffffff)]:
                    state.pc, state.r[4], state.r[6], state.fault = 0x100, 0x800, address, 0
                    state.memory_size = size
                    state.x87_status, state.x87_tag = 0, 0 if index == 0 else 0xffff
                    before, before_ram = bytes(state), bytes(memory)
                    self.assertEqual(function(ctypes.byref(state), 10), 3)
                    state.fault = 0
                    self.assertEqual(bytes(state), before)
                    self.assertEqual(bytes(memory), before_ram)
                state.x87_status = 0x80c1
                before = bytes(state)
                self.assertEqual(function(ctypes.byref(state), 10), 8)
                self.assertEqual(bytes(state), before)
                state.pc, state.r[4], state.r[6], state.memory_size = 0x100, 0x800, 4086, 4096
                state.x87_status, state.x87_tag = 0, 0xffff if index == 0 else 0
                self.assertEqual(function(ctypes.byref(state), 10), 0)
                self.assertEqual(memory[4085], 0x5a)
                # Fault-priority probes run in child processes, with no cores.
                for pending in (False, True):
                    for masked in (False, True):
                        for tags in (0, 0xffff):
                            def fault():
                                resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
                                initial = bytearray(108)
                                struct.pack_into('<H', initial, 0, 0x37e | (masked and not pending))
                                struct.pack_into('<H', initial, 4, 0x80c1 if pending else 0)
                                struct.pack_into('<H', initial, 8, tags)
                                native((ctypes.c_uint8 * 108).from_buffer(initial), output, None)
                            process = multiprocessing.get_context('fork').Process(target=fault)
                            process.start()
                            process.join()
                            self.assertEqual(process.exitcode, -signal.SIGFPE if pending else -signal.SIGSEGV)
            self.assertEqual(cases, 81920)

    def test_data_segment_comes_from_encoding_including_sib_base(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for address, expected in [('[esi]', 3), ('[ebp+8]', 2), ('[esp+ecx*2]', 2),
                                      ('[ecx+ebp*2]', 3), ('[ebp+ecx*2]', 2), ('[ebp*2+0x500]', 3),
                                      ('[0x500]', 3), ('es:[ebp]', 0), ('fs:[esp]', 4), ('gs:[esi]', 5)]:
                _, record = assemble(root, 'fld tbyte ptr ' + address + '\nret')
                self.assertEqual(asm_x87.memory80_encoding(record['instructions'][0])[0], expected, address)
            for source in ('addr16 fld tbyte ptr [bx]', 'data16 fld tbyte ptr [esi]'):
                _, record = assemble(root, source + '\nret')
                with self.assertRaises(asm_to_c.Unsupported):
                    asm_to_c.translate(record)
