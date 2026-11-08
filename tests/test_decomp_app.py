# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Asset-free checks for independently supplied OpenWatcom objects."""

from contextlib import redirect_stderr
import io
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools import decomp_app


class DecompAppTests(unittest.TestCase):
    def test_explicit_object_directory_is_passed_to_patch_builder(self):
        with tempfile.TemporaryDirectory() as directory:
            game = Path(directory) / '_EF2000.EXE'
            game.touch()
            objects = Path(directory) / 'objects'
            objects.mkdir()
            with patch.dict(os.environ, {'EF2000_DECOMP_OBJECTS': '/unused'}), \
                    patch.object(decomp_app, 'build_patch') as build, \
                    patch.object(decomp_app.subprocess, 'call', return_value=7) as run:
                result = decomp_app.main([
                    str(game), '--objects', str(objects), '--launcher', '/bin/ef2000',
                    '--', '--windowed'])
            self.assertEqual(result, 7)
            build.assert_called_once()
            self.assertEqual(build.call_args.args[:2], (game, objects))
            self.assertEqual(run.call_args.args[0][:3],
                             ['/bin/ef2000', str(game), '--patch'])
            self.assertEqual(run.call_args.args[0][-1], '--windowed')

    def test_object_directory_can_come_from_environment(self):
        with tempfile.TemporaryDirectory() as directory:
            game = Path(directory) / '_EF2000.EXE'
            game.touch()
            objects = Path(directory) / 'objects'
            objects.mkdir()
            with patch.dict(os.environ, {'EF2000_DECOMP_OBJECTS': str(objects)}), \
                    patch.object(decomp_app, 'build_patch') as build, \
                    patch.object(decomp_app.subprocess, 'call', return_value=0):
                self.assertEqual(decomp_app.main([
                    str(game), '--launcher', '/bin/ef2000']), 0)
            self.assertEqual(build.call_args.args[:2], (game, objects))

    def test_missing_object_directory_has_actionable_error(self):
        error = io.StringIO()
        with patch.dict(os.environ, {}, clear=True), redirect_stderr(error):
            with self.assertRaises(SystemExit) as raised:
                decomp_app.main(['/nonexistent/_EF2000.EXE', '--launcher', '/bin/ef2000'])
        self.assertEqual(raised.exception.code, 2)
        self.assertIn('build .#ef2000-decomp-watcom first', error.getvalue())


if __name__ == '__main__':
    unittest.main()
