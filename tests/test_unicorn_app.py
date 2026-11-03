# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""CLI contract for the experimental windowed Unicorn flake app."""

from contextlib import redirect_stderr, redirect_stdout
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.unicorn_backend import main


class UnicornAppTests(unittest.TestCase):
    def test_interactive_app_discovers_data_dir_and_sets_private_overlay(self):
        with tempfile.TemporaryDirectory() as root:
            game = Path(root) / 'game'
            game.mkdir()
            (game / '_EF2000.EXE').write_bytes(b'original')
            state = Path(root) / 'state'
            result = {'status': 'window_closed', 'timer': {'instructions': 100}}
            with patch('tools.extract_image.extract', return_value=({}, {'payload.bin': b'payload'})), \
                 patch('tools.unicorn_backend.run', return_value=result) as runner, \
                 redirect_stdout(io.StringIO()) as output:
                code = main(['--data-dir', str(game), '--state-dir', str(state)],
                            interactive=True)
            self.assertEqual(code, 0)
            self.assertIn('window_closed', output.getvalue())
            self.assertEqual(runner.call_args.args, (b'payload', None))
            self.assertTrue(runner.call_args.kwargs['windowed'])
            self.assertTrue(runner.call_args.kwargs['fast_execution'])
            self.assertEqual(runner.call_args.kwargs['timer_instructions'], 100000)
            self.assertEqual(runner.call_args.kwargs['progress_instructions'], 100000000)
            self.assertEqual(runner.call_args.kwargs['game_dir'], game)
            self.assertEqual(runner.call_args.kwargs['overlay_dir'], state / 'unicorn' / 'drive')
            self.assertTrue((state / 'unicorn' / 'drive').is_dir())

    def test_probe_remains_bounded_headless_and_does_not_create_overlay(self):
        with tempfile.TemporaryDirectory() as root:
            exe = Path(root) / '_EF2000.EXE'
            exe.write_bytes(b'original')
            result = {'status': 'instruction_limit', 'timer': {'instructions': 10}}
            with patch('tools.extract_image.extract', return_value=({}, {'payload.bin': b'payload'})), \
                 patch('tools.unicorn_backend.run', return_value=result) as runner, \
                 redirect_stdout(io.StringIO()) as output:
                code = main([str(exe)], interactive=False)
            self.assertEqual(code, 21)
            self.assertIn('instruction_limit', output.getvalue())
            self.assertFalse(runner.call_args.kwargs['windowed'])
            self.assertFalse(runner.call_args.kwargs['fast_execution'])
            self.assertEqual(runner.call_args.kwargs['timer_instructions'], 250000)
            self.assertEqual(runner.call_args.kwargs['progress_instructions'], 0)
            self.assertIsNone(runner.call_args.kwargs['overlay_dir'])

    def test_probe_with_explicit_executable_needs_no_xdg_or_home(self):
        # The direct Python probe is also used on Windows without XDG/HOME.
        with tempfile.TemporaryDirectory() as root:
            exe = Path(root) / '_EF2000.EXE'
            exe.write_bytes(b'original')
            result = {'status': 'instruction_limit', 'timer': {'instructions': 1}}
            with patch.dict('os.environ', {}, clear=True), \
                 patch('tools.extract_image.extract', return_value=({}, {'payload.bin': b'payload'})), \
                 patch('tools.unicorn_backend.run', return_value=result), \
                 redirect_stdout(io.StringIO()):
                self.assertEqual(main([str(exe)]), 21)

    def test_app_headless_override_and_xdg_discovery(self):
        with tempfile.TemporaryDirectory() as root:
            data = Path(root) / 'share' / 'ef2000'
            data.mkdir(parents=True)
            (data / '_EF2000.EXE').write_bytes(b'original')
            result = {'status': 'instruction_limit', 'timer': {'instructions': 10}}
            with patch.dict('os.environ', {'XDG_DATA_HOME': str(data.parent),
                                           'XDG_STATE_HOME': str(Path(root) / 'state')}), \
                 patch('tools.extract_image.extract', return_value=({}, {'payload.bin': b'payload'})), \
                 patch('tools.unicorn_backend.run', return_value=result) as runner, \
                 redirect_stdout(io.StringIO()):
                code = main(['--headless', '--instruction-limit', '10'], interactive=True)
            self.assertEqual(code, 21)
            self.assertEqual(runner.call_args.kwargs['game_dir'], data)
            self.assertFalse(runner.call_args.kwargs['windowed'])

    def test_show_paths_needs_no_assets(self):
        with tempfile.TemporaryDirectory() as root:
            with redirect_stdout(io.StringIO()) as output:
                code = main(['--show-paths', '--user-data-dir', root], interactive=True)
            self.assertEqual(code, 0)
            self.assertIn(f'user-data: {root} (option)', output.getvalue())

    def test_fast_execution_rejects_incompatible_traces_and_invalid_quantum(self):
        for arguments in (['--fast-execution', '--trace-decompress'],
                          ['--fast-execution', '--trace-mouse'],
                          ['--fast-quantum', '0']):
            with self.subTest(arguments=arguments), redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as failure:
                    main(arguments, interactive=True)
                self.assertEqual(failure.exception.code, 2)


if __name__ == '__main__':
    unittest.main()
