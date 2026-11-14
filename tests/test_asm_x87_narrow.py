# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Independent masked narrowing-conversion checks, not store/pop semantics."""

import ctypes
import json
import os
from pathlib import Path
import platform
import shutil
import struct
import subprocess
import tempfile
import unittest

from tools import asm_softfloat


@unittest.skipUnless(os.environ.get('ASM_SOFTFLOAT_BUILD') and platform.machine() == 'x86_64'
                     and shutil.which('cc'), 'requires x86-64, cc and ASM_SOFTFLOAT_BUILD')
class NarrowTests(unittest.TestCase):
    def test_rounding_values_flags_and_context_preservation(self):
        dependency = Path(os.environ['ASM_SOFTFLOAT_BUILD']).resolve()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            report = asm_softfloat.build_narrow(dependency, root / 'adapter')
            library = ctypes.CDLL(str(root / 'adapter/narrow.so'))
            convert = library.ef_x87_narrow
            convert.argtypes = [ctypes.POINTER(ctypes.c_uint32), ctypes.c_uint, ctypes.c_uint,
                                ctypes.POINTER(ctypes.c_uint32)]
            convert.restype = ctypes.c_uint
            globals_ = [ctypes.c_uint8.in_dll(library, name) for name in
                        ('softfloat_roundingMode', 'softfloat_exceptionFlags',
                         'softfloat_detectTininess', 'extF80_roundingPrecision')]
            previous = [value.value for value in globals_]
            sentinels = [4, 0x15, 0, 32]
            for value, sentinel in zip(globals_, sentinels):
                value.value = sentinel
            native_source = root / 'native.c'
            native_source.write_text(r'''#include <stdint.h>
#define PROBE(NAME, INSN) \
void NAME(const void *initial, void *out, void *data) { \
 unsigned char saved[108]; \
 __asm__ volatile("fnsave %[save]\n\tfrstor (%[initial])\n\t" INSN " (%[data])\n\t" \
 "fnsave (%[out])\n\tfrstor %[save]" : [save] "+m"(saved) : \
 [initial] "r"(initial),[out] "r"(out),[data] "r"(data) : "memory"); }
PROBE(store32, "fstps")
PROBE(store64, "fstpl")
''')
            native_library = root / 'native.so'
            subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                            str(native_source), '-o', str(native_library)], check=True)
            hardware = ctypes.CDLL(str(native_library))
            raw, output = (ctypes.c_uint32 * 3)(), (ctypes.c_uint32 * 2)()
            state = bytearray(108)
            initial = (ctypes.c_uint8 * 108).from_buffer(state)
            captured, data = (ctypes.c_uint8 * 108)(), (ctypes.c_uint8 * 8)()
            cases = 0
            try:
                for width in (32, 64):
                    native = getattr(hardware, 'store%d' % width)
                    native.argtypes, native.restype = [ctypes.c_void_p] * 3, None
                    def check(significand, exponent, rounding, precision=3):
                        nonlocal cases
                        raw[:] = [significand & 0xffffffff, significand >> 32, exponent]
                        control = 0x7f | (precision << 8) | (rounding << 10)
                        magnitude_exp = exponent & 0x7fff
                        tag = 1 if not magnitude_exp and not significand else (
                            2 if magnitude_exp in (0, 0x7fff) or not significand >> 63 else 0)
                        struct.pack_into('<H', state, 0, control)
                        struct.pack_into('<H', state, 4, 0x4700)
                        struct.pack_into('<H', state, 8, 0xfffc | tag)
                        struct.pack_into('<QH', state, 28, significand, exponent)
                        native(initial, captured, data)
                        flags = convert(raw, width, rounding, output)
                        expected = int.from_bytes(bytes(data[:width//8]), 'little')
                        actual = output[0] | ((output[1] << 32) if width == 64 else 0)
                        context = (width, hex(significand), hex(exponent), rounding, precision)
                        self.assertEqual(actual, expected, context)
                        self.assertEqual(flags, struct.unpack_from('<H', bytes(captured), 4)[0] & 0x23f, context)
                        self.assertEqual([v.value for v in globals_], sentinels)
                        cases += 1
                    # A nonzero discarded low bit probes all extended exponents
                    # in every direction, spanning normal/tiny/overflow/NaN.
                    for exponent in range(32768):
                        for rounding in range(4):
                            check(0x8000000000000001, exponent, rounding)
                    boundaries = [0, 1, 0x3ffe, 0x3fff, 0x4000, 0x7ffe, 0x7fff]
                    for exponent in ((16234, 16257, 16510) if width == 32 else (15309, 15361, 17406)):
                        boundaries.extend(range(exponent-2, exponent+3))
                    significands = [0, 1, 0x4000000000000000, 0x7fffffffffffffff,
                                    0x8000000000000000, 0x8000000000000001,
                                    0x8000008000000000, 0x8000010000000000,
                                    0xbfffffffffffffff, 0xc000000000000000,
                                    0xffffff0000000000, 0xffffff8000000000,
                                    0xfffffffffffff800, 0xfffffffffffffc00, 0xffffffffffffffff]
                    for exponent in sorted(set(boundaries)):
                        for significand in significands:
                            for sign in (0, 0x8000):
                                for precision in (0, 2, 3):
                                    for rounding in range(4):
                                        check(significand, exponent | sign, rounding, precision)
                for width, rounding in [(16, 0), (32, 4), (80, 0)]:
                    output[:] = [0x12345678, 0x87654321]
                    self.assertEqual(convert(raw, width, rounding, output), 0x10000)
                    self.assertEqual(list(output), [0x12345678, 0x87654321])
                    self.assertEqual([v.value for v in globals_], sentinels)
                evidence = dict(conversion_cases=cases, adapter=report['source_sha256'],
                                dependency=report['dependency_library_sha256'],
                                scope='Masked conversion values/flags/C1 only; no unmasked store/pop proof.')
                if os.environ.get('ASM_NARROW_EVIDENCE'):
                    Path(os.environ['ASM_NARROW_EVIDENCE']).write_text(json.dumps(evidence, indent=2)+'\n')
            finally:
                for value, old in zip(globals_, previous):
                    value.value = old
