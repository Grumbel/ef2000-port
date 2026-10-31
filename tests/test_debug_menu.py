# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
import zipfile

from tools.debug_menu import DebugMenu
from tools.save_states import SaveStates, default_directory
from unittest.mock import patch


class DebugMenuTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        root = Path(self.directory.name)
        self.states = SaveStates(root / 'states', root, {'test': 1})
        self.display = SimpleNamespace(fullscreen=False, closed=False,
                                       toggle_fullscreen=lambda: setattr(self.display, 'fullscreen', True))
        self.menu = DebugMenu(self.display, self.states)

    def test_default_library_is_shared_between_output_directories(self):
        with patch.dict('os.environ', {'XDG_DATA_HOME': self.directory.name}):
            self.assertEqual(default_directory(), Path(self.directory.name) / 'ef2000/states')
        with patch.dict('os.environ', {'XDG_DATA_HOME': ''}):
            self.assertEqual(default_directory(), Path.home() / '.local/share/ef2000/states')

    def test_keyboard_submenus_new_save_overwrite_and_load(self):
        self.menu.selected = 2
        self.menu.key(40)
        self.assertEqual(self.menu.rows(), ['New Save', 'Back'])
        self.menu.key(40)
        self.assertEqual(self.states.pending, ('save', None))
        path = self.states.directory / 'test-menu.efstate'
        path.touch()
        self.menu.selected = 1
        self.menu.key(40)
        self.assertEqual(self.states.pending, ('save', path))
        self.menu.key(41)
        self.menu.selected = 3
        self.menu.key(40)
        self.assertEqual(self.menu.rows(), ['test-menu', 'Back'])
        self.menu.key(40)
        self.assertEqual(self.states.pending, ('load', path))

    def test_unavailable_save_explains_backend_requirement(self):
        menu = DebugMenu(self.display, None)
        menu.open = True
        menu.selected = 2
        menu.key(40)
        self.assertEqual(menu.rows(), ['New Save (Unavailable)', 'Back'])
        menu.key(40)
        self.assertTrue(menu.open)
        self.assertIn('native64', menu.status)
        menu.selected = 1
        menu.key(40)
        self.assertEqual(menu.page, 'main')

    def test_fullscreen_resume_and_quit(self):
        self.menu.selected = 1
        self.menu.key(40)
        self.assertTrue(self.display.fullscreen)
        self.menu.selected = 0
        self.menu.open = True
        self.menu.key(40)
        self.assertFalse(self.menu.open)
        self.menu.selected = 4
        self.menu.key(40)
        self.assertTrue(self.display.closed)

    def test_scrolling_and_rendering(self):
        for index in range(30):
            (self.states.directory / f'save-{index:02}.efstate').touch()
        self.menu.page, self.menu.selected = 'load', 25
        pixels = self.menu.render(bytes(640 * 400 * 3), 640, 400)
        self.assertEqual(len(pixels), 640 * 400 * 3)
        self.assertGreater(self.menu.scroll, 0)
        self.assertTrue(any(pixels))

    def test_reject_incompatible_state_before_changing_drive(self):
        drive = self.states.output / 'drive'
        drive.mkdir()
        (drive / 'keep').write_text('unchanged')
        path = self.states.directory / 'wrong.efstate'
        with zipfile.ZipFile(path, 'w') as archive:
            archive.writestr('metadata.json', json.dumps({'identity': {'test': 2}}))
        with self.assertRaisesRegex(ValueError, 'different executable'):
            self.states.prepare_load(path)
        self.assertEqual((drive / 'keep').read_text(), 'unchanged')

    def test_restore_keeps_native_directory_descriptors_usable(self):
        import os
        drive = self.states.output / 'drive' / 'EF2000'
        drive.mkdir(parents=True)
        (drive / 'EF2000.CFG').write_text('saved config')
        with (self.states.output / 'checkpoint.bin').open('wb') as native:
            native.truncate(0x4100000)
        engine = SimpleNamespace(video_modes={0x100: (640, 400)}, video_mode=0x100,
                                 game_phase='menu', sound_device=None)
        self.states.save(engine)
        directory_fd = os.open(drive, os.O_RDONLY | os.O_DIRECTORY)
        try:
            (drive / 'EF2000.CFG').write_text('later config')
            (drive / 'extra').write_text('later file')
            self.states.prepare_load(self.states.current)
            config_fd = os.open('EF2000.CFG', os.O_RDONLY, dir_fd=directory_fd)
            try:
                self.assertEqual(os.read(config_fd, 100), b'saved config')
            finally:
                os.close(config_fd)
            self.assertFalse((drive / 'extra').exists())
        finally:
            os.close(directory_fd)
