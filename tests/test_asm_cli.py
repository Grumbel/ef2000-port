# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""CLI clears only IF; unsupported privilege contexts stop before mutation."""

import ctypes
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from tools import asm_to_c, asm_c_differential as diff
from tests.test_asm_to_c import assemble, unicorn


@unittest.skipUnless(all(shutil.which(t) for t in ('as', 'objcopy', 'cc')),
                     'requires binutils and cc')
class CliTests(unittest.TestCase):
    def test_flags_privilege_and_restart(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, record = assemble(root, 'cli\nret')
            (root/'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
            (root/'lift.c').write_text(asm_to_c.translate(record))
            subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-std=c99',
                            '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
                            str(root/'lift.c'), '-o', str(root/'lift.so')], check=True)
            library = ctypes.CDLL(str(root/'lift.so'))
            function = library.ef_lift_00000100
            function.argtypes = [ctypes.POINTER(diff.State), ctypes.c_uint32]
            function.restype = ctypes.c_uint
            for cpl in (0, 3):
                for iopl in range(4):
                    for vm in (0, 0x20000):
                        for enabled in (0, 0x200):
                            ram = (ctypes.c_uint8*4096)(*([0xa5]*4096))
                            flags = 0x240cd7 | (iopl << 12) | vm | enabled
                            state = diff.State((ctypes.c_uint32*8)(*range(8)),
                                               flags, 0x100, 0, ram, len(ram))
                            state.segment[:] = [16, 8, 16, 16, 16, 16]
                            if cpl:
                                state.segment_profile = 1
                                state.segment[:] = [19, 11, 19, 19, 19, 19]
                                for index in range(6):
                                    state.segment_cache[index].selector = state.segment[index]
                                    state.segment_cache[index].flags = 4
                                    state.segment_cache[index].limit = 0xffffffff
                                    state.segment_cache[index].access = 0xfb if index == 1 else 0xf3
                            previous = bytes(state)
                            self.assertEqual(function(ctypes.byref(state), 0), 1)
                            self.assertEqual(bytes(state), previous)
                            allowed = not vm and cpl <= iopl
                            self.assertEqual(function(ctypes.byref(state), 1), 1 if allowed else 7)
                            if allowed:
                                self.assertEqual(state.flags, flags & ~0x200)
                                self.assertEqual(state.pc, 0x101)
                                state.flags, state.pc = flags, 0x100
                            self.assertEqual(bytes(state), previous)
                            self.assertEqual(bytes(ram), bytes([0xa5])*4096)
            if unicorn:
                for flags in (0x2, 0x202, 0x3202, 0x3ad7):
                    result = diff.compare(function, image.read_bytes(), 0x100,
                                          [1, 2, 3, 4, 0x3f0000, 5, 6, 7], flags)
                    self.assertEqual(result['state'], 'pass', result)

    def test_rejects_mislabeled_cli_encoding(self):
        with tempfile.TemporaryDirectory() as temporary:
            _, record = assemble(Path(temporary), 'cli\nret')
            record['instructions'][0]['bytes'] = 'fb'
            with self.assertRaises(asm_to_c.Unsupported):
                asm_to_c.translate(record)

    def test_sti_remains_unsupported(self):
        with tempfile.TemporaryDirectory() as temporary:
            _, record = assemble(Path(temporary), 'sti\nret')
            with self.assertRaises(asm_to_c.Unsupported):
                asm_to_c.translate(record)
