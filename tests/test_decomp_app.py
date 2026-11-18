# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Asset-free checks for independently supplied OpenWatcom objects."""

from contextlib import redirect_stderr
import hashlib
import io
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools import decomp_app


class DecompAppTests(unittest.TestCase):
    def test_patch_builder_passes_native_variadic_contracts(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            header = root / 'decomp/include/ef_abi.h'
            header.parent.mkdir(parents=True)
            header.write_text('int status(int mode, const char *fmt, ...);\n')
            game = root / '_EF2000.EXE'
            game.write_bytes(b'executable')
            objects = root / 'objects'
            objects.mkdir()
            (objects / 'status.obj').touch()
            output = root / 'result.efpatch'
            report = dict(functions_redirected=1, redirect_bytes=5,
                          arena_bytes=32, linked_modules=1, object_modules=1)
            with patch.object(decomp_app, 'ROOT', root), \
                    patch.object(decomp_app.extract_image, 'extract',
                                 return_value=(None, {'payload.bin': bytes(16)})), \
                    patch.object(decomp_app.hybrid_link, 'read_tsv', return_value=[]), \
                    patch.object(decomp_app.hybrid_link, 'load_exclusions', return_value={}), \
                    patch.object(decomp_app.hybrid_link, 'link',
                                 return_value=(b'patch', report)) as link, \
                    redirect_stderr(io.StringIO()):
                decomp_app.build_patch(game, objects, output)
            self.assertEqual(link.call_args.kwargs['variadic_names'], {'status'})
            self.assertEqual(output.read_bytes(), b'patch')

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

    def test_english_build_uses_a_disposable_dosbox_copy(self):
        with tempfile.TemporaryDirectory() as directory:
            game = Path(directory)
            executable = b'english fixture'
            (game / '_EF2000.EXE').write_bytes(executable)
            (game / 'DID.DAT').write_bytes(b'assets')
            (game / '.git').mkdir()

            def run(command, *, cwd):
                self.assertEqual(command[0], '/bin/dosbox-x')
                self.assertEqual(command[-2:], ['-time-limit', '3'])
                copy = cwd / 'drive' / 'EF2000'
                self.assertEqual((copy / '_EF2000.EXE').read_bytes(), executable)
                self.assertFalse((copy / '.git').exists())
                (copy / 'DID.DAT').write_bytes(b'changed')
                return 7

            with patch.dict(os.environ, {}, clear=True), \
                    patch.object(decomp_app, 'ENGLISH_SHA256', hashlib.sha256(executable).hexdigest()), \
                    patch.object(decomp_app, 'build_patch') as build, \
                    patch.object(decomp_app.subprocess, 'call', side_effect=run), \
                    redirect_stderr(io.StringIO()):
                result = decomp_app.main([str(game), '--launcher', '/bin/false',
                                          '--dosbox', '/bin/dosbox-x', '--', '-time-limit', '3'])
            self.assertEqual(result, 7)
            build.assert_not_called()
            self.assertEqual((game / 'DID.DAT').read_bytes(), b'assets')

    def test_missing_object_directory_has_actionable_error(self):
        error = io.StringIO()
        with tempfile.TemporaryDirectory() as directory, \
                patch.dict(os.environ, {}, clear=True), redirect_stderr(error):
            game = Path(directory) / '_EF2000.EXE'
            game.touch()
            with self.assertRaises(SystemExit) as raised:
                decomp_app.main([str(game), '--launcher', '/bin/ef2000'])
        self.assertEqual(raised.exception.code, 2)
        self.assertIn('build .#ef2000-decomp-watcom first', error.getvalue())


if __name__ == '__main__':
    unittest.main()
