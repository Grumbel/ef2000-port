# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""The Linux decomp bootstrap must actually link and execute recovered C."""

from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class DecompStandaloneTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('gcc'), 'requires GCC')
    def test_gcc_executable_runs_recovered_highscore_code(self):
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / 'ef2000-decomp'
            subprocess.run([
                'gcc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                '-DEF_DECOMP_NATIVE=1', '-fno-strict-aliasing', '-fwrapv',
                '-ffunction-sections', '-fdata-sections',
                '-Wno-error=int-to-pointer-cast',
                '-I' + str(ROOT / 'decomp/include'),
                '-I' + str(ROOT / 'runtime'),
                str(ROOT / 'decomp/standalone/main.c'),
                str(ROOT / 'decomp/src/hiscore.c'),
                str(ROOT / 'decomp/src/runtime_helpers25.c'),
                str(ROOT / 'decomp/src/math_string.c'),
                str(ROOT / 'decomp/src/game_main.c'),
                str(ROOT / 'decomp/src/tactcom_prefix.c'),
                str(ROOT / 'decomp/src/ascii_stricmp.c'),
                str(ROOT / 'decomp/src/runtime_helpers19.c'),
                str(ROOT / 'decomp/src/game_helpers81.c'),
                str(ROOT / 'decomp/src/ui_widget_helpers.c'),
                str(ROOT / 'decomp/src/leaf_helpers1.c'),
                str(ROOT / 'runtime/decomp_image.c'),
                str(ROOT / 'runtime/decomp_startup.c'),
                '-lcrypto', '-Wl,--gc-sections',
                '-o', str(executable),
            ], check=True, capture_output=True)
            result = subprocess.run([str(executable), '--self-test'],
                                    capture_output=True, text=True, check=True)
            self.assertIn('Linux / GCC', result.stdout)
            self.assertIn('high-score comparison: passed', result.stdout)

            default = subprocess.run([str(executable)], capture_output=True,
                                     text=True, check=True)
            self.assertIn('TACTCOM', default.stdout)

            invalid = subprocess.run([str(executable), '--data-dir', '/unused'],
                                     capture_output=True, text=True)
            self.assertEqual(invalid.returncode, 2)

            bad_image = Path(directory) / '_EF2000.EXE'
            bad_image.write_bytes(b'not original game data')
            rejected = subprocess.run([str(executable), '--image', str(bad_image)],
                                      capture_output=True, text=True)
            self.assertEqual(rejected.returncode, 1)
            self.assertIn('unsupported executable SHA-256', rejected.stderr)

            unsupported = subprocess.run([
                str(executable), '--image', str(bad_image), '--probe-arena',
            ], capture_output=True, text=True)
            self.assertEqual(unsupported.returncode, 1)

            # Opt-in only: proprietary game bytes are never a test fixture.
            # The loader itself checks the SHA-pinned German executable.
            original = os.environ.get('EF2000_EXE')
            if original:
                real = subprocess.run([
                    str(executable), '--image', original, '--probe-main',
                ], capture_output=True, text=True, check=True)
                self.assertIn('arena 0x004df000 (3000000 bytes)', real.stdout)
                self.assertIn('TACTCOM\n\nfound 57,618K bytes of memory',
                              real.stdout)
                self.assertIn('stopped at unimplemented call 0x0009c980',
                              real.stdout)

    @unittest.skipUnless(shutil.which('gcc'), 'requires GCC')
    def test_guest_memory_and_x32_envelope(self):
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / 'decomp-image-test'
            subprocess.run([
                'gcc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                '-DEF_DECOMP_NATIVE=1', '-fno-strict-aliasing', '-fwrapv',
                '-I' + str(ROOT / 'decomp/include'),
                '-I' + str(ROOT / 'runtime'),
                str(ROOT / 'tests/decomp_image_test.c'),
                str(ROOT / 'decomp/src/game_helpers25.c'),
                str(ROOT / 'runtime/decomp_image.c'), '-lcrypto',
                '-o', str(executable),
            ], check=True, capture_output=True)
            subprocess.run([str(executable)], capture_output=True, check=True)

    @unittest.skipUnless(shutil.which('gcc'), 'requires GCC')
    def test_first_recovered_game_startup_stage(self):
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / 'decomp-startup-test'
            subprocess.run([
                'gcc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                '-DEF_DECOMP_NATIVE=1', '-fno-strict-aliasing', '-fwrapv',
                '-ffunction-sections', '-fdata-sections',
                '-Wno-error=int-to-pointer-cast',
                '-I' + str(ROOT / 'decomp/include'),
                '-I' + str(ROOT / 'runtime'),
                str(ROOT / 'tests/decomp_startup_test.c'),
                str(ROOT / 'decomp/src/runtime_helpers25.c'),
                str(ROOT / 'decomp/src/math_string.c'),
                str(ROOT / 'decomp/src/game_main.c'),
                str(ROOT / 'decomp/src/tactcom_prefix.c'),
                str(ROOT / 'decomp/src/ascii_stricmp.c'),
                str(ROOT / 'decomp/src/runtime_helpers19.c'),
                str(ROOT / 'decomp/src/game_helpers81.c'),
                str(ROOT / 'decomp/src/ui_widget_helpers.c'),
                str(ROOT / 'decomp/src/leaf_helpers1.c'),
                str(ROOT / 'runtime/decomp_image.c'),
                str(ROOT / 'runtime/decomp_startup.c'),
                '-lcrypto', '-Wl,--gc-sections', '-o', str(executable),
            ], check=True, capture_output=True)
            result = subprocess.run([str(executable)], capture_output=True, check=True)
            self.assertIn(b'Memory: 60,415K', result.stdout)


if __name__ == '__main__':
    unittest.main()
