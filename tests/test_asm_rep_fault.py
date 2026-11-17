# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Compare later-iteration REP faults with the native AMD observation."""
import ctypes
import json
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile
import unittest

from tests.test_asm_to_c import assemble
from tools import asm_to_c, asm_c_differential as diff


@unittest.skipUnless(platform.system() == 'Linux' and platform.machine() == 'x86_64'
                     and all(shutil.which(t) for t in ('cc', 'as', 'objcopy')),
                     'requires Linux x86-64, compiler and binutils')
class RepFaultTests(unittest.TestCase):
    def test_amd_later_iteration_fault_keeps_last_comparison_flags(self):
        # Do not infer an Intel fault policy from an AMD observation.
        if 'AuthenticAMD' not in Path('/proc/cpuinfo').read_text():
            self.skipTest('native evidence currently covers AMD only')
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            native = root/'native'
            subprocess.run(['cc', '-std=c99', '-O0', '-Wall', '-Wextra', '-Werror',
                            str(Path(__file__).with_name('native_rep.c')), '-o', str(native)], check=True)
            (root/'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
            for scan in (0, 1):
                with self.subTest(scan=scan):
                    result = subprocess.run([str(native), str(scan)], capture_output=True, text=True)
                    if result.returncode == 2:
                        self.skipTest('native memory mapping unavailable')
                    self.assertEqual(result.returncode, 0, result.stderr)
                    observed = json.loads(result.stdout)
                    self.assertEqual((observed['trap'], observed['error']), (14, 4))
                    self.assertEqual(observed['selectors'] & 0xffff, 0x23)
                    self.assertEqual((observed['count'], observed['di_delta']), (2, 2))
                    self.assertEqual(observed['si_delta'], 0 if scan else 2)
                    self.assertEqual(observed['flags'] & ~0x10000, 0x246)
                    image, record = assemble(root, 'repe %s\nret' % ('scasb' if scan else 'cmpsb'))
                    (root/'lift.c').write_text(asm_to_c.translate(record))
                    library = root/('lift%d.so' % scan)
                    subprocess.run(['cc', '-shared', '-fPIC', '-O0', '-std=c99', '-Wall',
                                    '-Wextra', '-Werror', '-Wno-unused-function', str(root/'lift.c'),
                                    '-o', str(library)], check=True)
                    function = ctypes.CDLL(str(library)).ef_lift_00000100
                    function.argtypes = [ctypes.POINTER(diff.State), ctypes.c_uint32]
                    function.restype = ctypes.c_uint
                    memory = bytearray(0x4000)
                    memory[:image.stat().st_size] = image.read_bytes()
                    initial = bytes(memory)
                    ram = (ctypes.c_uint8 * len(memory)).from_buffer(memory)
                    regs = [0, 4, 22, 44, 0x3000, 77, 0x2000, len(memory)-2]
                    state = diff.State((ctypes.c_uint32 * 8)(*regs), 0x202, 0x100, 0, ram, len(memory))
                    # Resume across budget boundaries as well as within one call.
                    self.assertEqual(function(ctypes.byref(state), 1), 1)
                    self.assertEqual(state.r[1], 3)
                    self.assertEqual(function(ctypes.byref(state), 100), 3)
                    self.assertEqual(state.fault, 3)
                    expected = list(regs)
                    expected[1] = observed['count']
                    expected[6] += observed['si_delta']
                    expected[7] += observed['di_delta']
                    self.assertEqual(list(state.r), expected)
                    # RF belongs to native exception delivery, outside the model.
                    self.assertEqual(state.flags, observed['flags'] & ~0x10000)
                    self.assertEqual(state.pc, 0x100)
                    self.assertEqual(bytes(memory), initial)
