# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

import tempfile
from pathlib import Path
import unittest

from tools.paths import resolve_paths
from tools.save_states import SaveStates


class PathTests(unittest.TestCase):
    def test_xdg_defaults_and_invalid_relative_environment(self):
        env = {'HOME': '/tmp/home', 'XDG_STATE_HOME': '/tmp/states',
               'XDG_CONFIG_HOME': 'relative'}
        paths = resolve_paths(environ=env)
        self.assertEqual(paths['state'], (Path('/tmp/states/ef2000'), 'XDG_STATE_HOME'))
        self.assertEqual(paths['config'], (Path('/tmp/home/.config/ef2000'), 'default'))

    def test_overrides_are_independent_and_portable_marker(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            exe = root / '_EF2000.EXE'
            (root / 'portable.txt').touch()
            paths = resolve_paths({'state': root / 'custom'}, executable=exe,
                                  environ={'HOME': '/tmp/home'})
            self.assertEqual(paths['state'], (root / 'custom', 'option'))
            self.assertEqual(paths['config'], (root / 'userdata/config', 'portable'))

    def test_legacy_states_remain_visible_when_new_library_empty(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            legacy = root / 'old'
            legacy.mkdir()
            saved = legacy / 'older.efstate'
            saved.touch()
            states = SaveStates(root / 'new', root / 'run', {}, legacy=legacy)
            self.assertEqual(states.entries(), [saved])
            (states.directory / 'recent.efstate').touch()
            self.assertEqual([p.name for p in states.entries()], ['recent.efstate'])
