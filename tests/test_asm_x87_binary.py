# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Hardware checks for masked raw arithmetic, separate from guest effects."""

import ctypes
import json
import os
from pathlib import Path
import platform
import random
import shutil
import struct
import subprocess
import tempfile
import unittest

from tools import asm_softfloat


@unittest.skipUnless(os.environ.get('ASM_SOFTFLOAT_BUILD') and platform.machine() == 'x86_64'
                     and shutil.which('cc'), 'requires x86-64, cc and ASM_SOFTFLOAT_BUILD')
class BinaryTests(unittest.TestCase):
    def test_masked_results_flags_rounding_and_context(self):
        dependency = Path(os.environ['ASM_SOFTFLOAT_BUILD']).resolve()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            report = asm_softfloat.build_binary(dependency, root / 'adapter')
            library = ctypes.CDLL(str(root / 'adapter/binary.so'))
            convert = library.ef_x87_binary
            pointer = ctypes.POINTER(ctypes.c_uint32)
            convert.argtypes = [pointer, pointer, ctypes.c_uint, ctypes.c_uint,
                                ctypes.c_uint, pointer]
            convert.restype = ctypes.c_uint
            controlled = library.ef_x87_binary_control
            controlled.argtypes = [pointer, pointer, ctypes.c_uint, ctypes.c_uint, pointer]
            controlled.restype = ctypes.c_uint
            globals_ = [ctypes.c_uint8.in_dll(library, name) for name in
                        ('softfloat_roundingMode', 'softfloat_exceptionFlags',
                         'softfloat_detectTininess', 'extF80_roundingPrecision')]
            previous = [v.value for v in globals_]
            sentinels = [4, 0x15, 0, 32]
            for value, sentinel in zip(globals_, sentinels):
                value.value = sentinel
            source = root / 'native.c'
            source.write_text(r'''#include <stdint.h>
#define PROBE(NAME, INSN) \
void NAME(const void *initial, void *out) { \
 unsigned char saved[108]; \
 __asm__ volatile("fnsave %[save]\n\tfrstor (%[initial])\n\t" INSN "\n\t" \
 "fnsave (%[out])\n\tfrstor %[save]" : [save] "+m"(saved) : \
 [initial] "r"(initial),[out] "r"(out) : "memory"); }
PROBE(add, ".byte 0xd8,0xc1")
PROBE(sub, ".byte 0xd8,0xe1")
PROBE(mul, ".byte 0xd8,0xc9")
PROBE(divide, ".byte 0xd8,0xf1")
''')
            native_path = root / 'native.so'
            subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                            str(source), '-o', str(native_path)], check=True)
            native = ctypes.CDLL(str(native_path))
            operations = [getattr(native, name) for name in ('add', 'sub', 'mul', 'divide')]
            for function in operations:
                function.argtypes, function.restype = [ctypes.c_void_p] * 2, None
            left, right, result = [(ctypes.c_uint32 * 3)() for _ in range(3)]
            state = bytearray(108)
            initial = (ctypes.c_uint8 * 108).from_buffer(state)
            captured = (ctypes.c_uint8 * 108)()
            cases = 0

            def check(a, b, operation, precision, rounding, masks=None):
                nonlocal cases
                for raw, value in ((left, a), (right, b)):
                    raw[:] = [value[0] & 0xffffffff, value[0] >> 32, value[1]]
                control = 0x40 | (63 if masks is None else masks) | precision << 8 | rounding << 10
                struct.pack_into('<H', state, 0, control)
                struct.pack_into('<H', state, 4, 0x4500)
                struct.pack_into('<H', state, 8, 0xfff0)
                struct.pack_into('<QH', state, 28, *a)
                struct.pack_into('<QH', state, 38, *b)
                operations[operation](initial, captured)
                result[:] = [123, 456, 789]
                flags = (convert(left, right, operation, precision, rounding, result) if masks is None else
                         controlled(left, right, operation, control, result))
                expected = struct.unpack_from('<QH', bytes(captured), 28)
                if flags & 0x20000:
                    self.assertEqual(list(result), [123, 456, 789])
                    actual = a
                    flags &= ~0x20000
                else:
                    actual = (result[0] | result[1] << 32, result[2])
                context = (a, b, operation, precision, rounding, masks)
                self.assertEqual(actual, expected, context)
                self.assertEqual(flags, struct.unpack_from('<H', bytes(captured), 4)[0] & 0x23f,
                                 context)
                self.assertEqual([v.value for v in globals_], sentinels)
                cases += 1

            try:
                values = [(0, 0), (1, 0), (0x7fffffffffffffff, 0),
                          (0x8000000000000000, 0), (0x8000000000000000, 1),
                          (0x8000000000000000, 0x3ffe), (0x8000000000000000, 0x3fff),
                          (0x8000000000000001, 0x3fff), (0xffffffffffffffff, 0x3fff),
                          (0x8000000000000000, 0x4000), (0xffffffffffffffff, 0x7ffe),
                          (0x8000000000000000, 0x7fff), (0xc000000000000123, 0x7fff),
                          (0xc000000000000456, 0x7fff), (0x8000000000000123, 0x7fff),
                          (0x8000000000000456, 0x7fff), (0x4000000000000000, 0x3fff),
                          (0, 0x7fff), (0x4000000000000000, 0x7fff)]
                values += [(sig, exp | 0x8000) for sig, exp in values]
                pairs = [(a, b) for a in values for b in values]
                rng = random.Random(0x87b1)
                pairs += [((rng.getrandbits(63) | 1 << 63, rng.randrange(1, 0x7fff) |
                            rng.randrange(2) << 15),
                           (rng.getrandbits(63) | 1 << 63, rng.randrange(1, 0x7fff) |
                            rng.randrange(2) << 15)) for _ in range(2048)]
                pairs += [tuple((rng.getrandbits(64), rng.choice((0, 1, 0x3fff, 0x7fff)) |
                                 rng.randrange(2) << 15) for _ in range(2)) for _ in range(2048)]
                # Correlated inputs expose cancellation and guard/round/sticky
                # boundaries that unrelated exponents rarely exercise.
                for precision in (24, 53, 64):
                    step = 1 << (64 - precision)
                    for offset in (-1, 0, 1):
                        for exponent in (1, 2, 0x3ffe, 0x3fff, 0x4000, 0x7ffd, 0x7ffe):
                            for sign in (0, 0x8000):
                                a = (0x8000000000000000 + step + offset, exponent | sign)
                                pairs.extend((a, (0x8000000000000000, exponent | other))
                                             for other in (0, 0x8000))
                for a, b in pairs:
                    for operation in range(4):
                        for precision in (0, 2, 3):
                            for rounding in range(4):
                                check(a, b, operation, precision, rounding)
                control_pairs = [(a, b) for a in values for b in values]
                control_pairs += [((0xffffffffffffffff, 0x7ffe), (0x8000000000000000, 0x4000)),
                                  ((0x8000000000000000, 1), (0x8000000000000000, 0x3ffe)),
                                  ((0x7fffffffffffffff, 0), (0x8000000000000001, 0x3fff)),
                                  ((0xffffffffffffffff, 0x7ffe), (1, 0))]
                for a, b in control_pairs:
                    for masks in range(64):
                        for operation in range(4):
                            for precision in (0, 2, 3):
                                for rounding in range(4):
                                    check(a, b, operation, precision, rounding, masks)
                # Sweep the extended exponent range through every precision
                # and rounding mode, crossing tiny and overflow boundaries.
                for exponent in range(1, 0x7fff):
                    a = (0x8000000000000001, exponent | (0x8000 if exponent & 1 else 0))
                    b = (0xffffffffffffffff, 0x4000)
                    for operation in (2, 3):
                        for precision in (0, 2, 3):
                            for rounding in range(4):
                                check(a, b, operation, precision, rounding)
                for operation, precision, rounding in ((4, 3, 0), (0, 1, 0), (0, 3, 4)):
                    result[:] = [123, 456, 789]
                    self.assertEqual(convert(left, right, operation, precision, rounding, result),
                                     0x10000)
                    self.assertEqual(list(result), [123, 456, 789])
                    self.assertEqual([v.value for v in globals_], sentinels)
                operand = library.ef_x87_binary_operand
                operand.argtypes = [pointer, pointer, ctypes.c_uint, ctypes.c_uint, ctypes.c_uint, pointer]
                operand.restype = ctypes.c_uint
                result[:] = [123, 456, 789]
                self.assertEqual(operand(left, right, 0, 0x37f, 2, result), 0x10000)
                self.assertEqual(list(result), [123, 456, 789])
                self.assertEqual([v.value for v in globals_], sentinels)
                if os.environ.get('ASM_BINARY_EVIDENCE'):
                    Path(os.environ['ASM_BINARY_EVIDENCE']).write_text(json.dumps(dict(
                        cases=cases, adapter=report['source_sha256'],
                        dependency=report['dependency_library_sha256'],
                        scope='Raw arithmetic with all exception masks; no guest effects or delivery.'),
                        indent=2) + '\n')
            finally:
                for value, old in zip(globals_, previous):
                    value.value = old
