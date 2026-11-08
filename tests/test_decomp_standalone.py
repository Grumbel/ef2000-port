# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""The Linux decomp bootstrap must actually link and execute recovered C."""

from pathlib import Path
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
                '-I' + str(ROOT / 'decomp/include'),
                '-I' + str(ROOT / 'runtime'),
                str(ROOT / 'decomp/standalone/main.c'),
                str(ROOT / 'decomp/src/hiscore.c'),
                str(ROOT / 'runtime/decomp_image.c'), '-lcrypto',
                '-o', str(executable),
            ], check=True, capture_output=True)
            result = subprocess.run([str(executable), '--self-test'],
                                    capture_output=True, text=True, check=True)
            self.assertIn('Linux / GCC', result.stdout)
            self.assertIn('high-score comparison: passed', result.stdout)

            default = subprocess.run([str(executable)], capture_output=True,
                                     text=True, check=True)
            self.assertIn('Game entry point', default.stdout)

            invalid = subprocess.run([str(executable), '--data-dir', '/unused'],
                                     capture_output=True, text=True)
            self.assertEqual(invalid.returncode, 2)

            bad_image = Path(directory) / '_EF2000.EXE'
            bad_image.write_bytes(b'not original game data')
            rejected = subprocess.run([str(executable), '--image', str(bad_image)],
                                      capture_output=True, text=True)
            self.assertEqual(rejected.returncode, 1)
            self.assertIn('unsupported executable SHA-256', rejected.stderr)

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


if __name__ == '__main__':
    unittest.main()
