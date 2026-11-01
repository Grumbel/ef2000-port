# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from tools.run_startup import gdb_command


SCRIPT = Path(__file__).resolve().parent.parent / 'tools/run_startup.py'


class CLIPathTests(unittest.TestCase):
    def run_cli(self, *options, env=None):
        return subprocess.run([sys.executable, str(SCRIPT), *options],
                              capture_output=True, text=True, env=env)

    def test_show_paths_needs_no_assets_or_video(self):
        with tempfile.TemporaryDirectory() as temporary:
            env = dict(os.environ, HOME=temporary, XDG_STATE_HOME='not/absolute')
            run = self.run_cli('--show-paths', '--state-dir', str(Path(temporary) / 'states'), env=env)
            self.assertEqual(run.returncode, 0, run.stderr)
            self.assertIn(f'state: {temporary}/states (option)', run.stdout)
            self.assertIn(f'config: {temporary}/.config/ef2000 (default)', run.stdout)

    def test_bad_geometry_and_missing_assets_are_errors(self):
        run = self.run_cli('--geometry', 'nonsense', '--show-paths')
        self.assertNotEqual(run.returncode, 0)
        self.assertIn('--geometry requires', run.stderr)
        with tempfile.TemporaryDirectory() as temporary:
            run = self.run_cli('--data-dir', temporary, '--output', str(Path(temporary) / 'run'))
            self.assertNotEqual(run.returncode, 0)
            self.assertIn('_EF2000.EXE', run.stderr)

    def test_help_and_version_work_without_game_data(self):
        self.assertIn('ef2000 ', self.run_cli('-v').stdout)
        help_text = self.run_cli('--help').stdout
        self.assertIn('--portable', help_text)
        self.assertIn('1280x960', help_text)
        self.assertIn('--aspect-ratio', help_text)
        self.assertIn('--smooth-scaling', help_text)
        self.assertIn('--gdb', help_text)
        run = self.run_cli('--aspect-ratio', '5:4', '--show-paths')
        self.assertNotEqual(run.returncode, 0)

    def test_gdb_starts_the_guest_elf_not_the_python_bridge(self):
        command = gdb_command(['/tmp/guest/startup.elf', '/tmp/game'])
        self.assertEqual(command[command.index('--args') + 1:],
                         ['/tmp/guest/startup.elf', '/tmp/game'])
        self.assertIn('run', command)
        self.assertIn('$_exitcode', ' '.join(command))
