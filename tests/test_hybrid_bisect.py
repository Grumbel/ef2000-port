# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Pixel comparisons used to isolate small hybrid drawing defects."""

from pathlib import Path
import json
import tempfile
import unittest
from unittest.mock import patch

from tools import hybrid_bisect


class FrameDifferenceTest(unittest.TestCase):
    def test_render_failure_does_not_match_a_different_video_mode(self):
        with tempfile.TemporaryDirectory() as directory:
            frame = Path(directory) / 'frame.ppm'
            result = {'status': {'status': 'instruction_limit'}}
            frame.write_bytes(b'P6\n2 1\n255\n' + bytes(6))
            target = hybrid_bisect.frame_failure_signature(result, result, frame)
            frame.write_bytes(b'P6\n2 1\n255\n' + bytes([255]) * 6)
            self.assertEqual(target, hybrid_bisect.frame_failure_signature(
                result, result, frame))
            frame.write_bytes(b'P6\n4 2\n255\n' + bytes(24))
            self.assertNotEqual(target, hybrid_bisect.frame_failure_signature(
                result, result, frame))
            frame.unlink()
            self.assertNotEqual(target, hybrid_bisect.frame_failure_signature(
                result, result, frame))

    def test_baseline_and_trial_link_keep_variadic_entry_contract(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            game = root / 'game'
            game.mkdir()
            image = game / 'image'
            image.write_bytes(b'\0' * 16)
            header = root / 'abi.h'
            header.write_text('int status(int mode, const char *fmt, ...);\n')
            manifest = root / 'manifest.json'
            manifest.write_text(json.dumps({
                'schema': 'ef2000-golden-replay-v1', 'cases': [{
                    'name': 'main-menu', 'instructions': 1000,
                    'events': [], 'must_hit': ['0x1'],
                    'allow_no_framebuffer': False}]}))
            result = {'status': {'status': 'instruction_limit', 'ip': 0},
                      'patch_hits': {'0x1': 1}}
            with patch.object(hybrid_bisect.hybrid_link, 'load_objects',
                              return_value=([], [])), \
                 patch.object(hybrid_bisect.hybrid_link, 'read_tsv', return_value=[]), \
                 patch.object(hybrid_bisect.hybrid_link, 'load_exclusions', return_value={}), \
                 patch.object(hybrid_bisect.hybrid_link, 'link', return_value=(b'', {
                     'functions_redirected': 1, 'redirect_bytes': 5})) as link, \
                 patch.object(hybrid_bisect, 'decode_patch', return_value=[(1, b'')]), \
                 patch.object(hybrid_bisect.replay_suite, 'run_one', return_value=result), \
                 patch.object(hybrid_bisect.replay_suite, 'differences', return_value=[]), \
                 patch('sys.stderr'), patch('builtins.print'):
                # A passing replay deliberately terminates before reduction.
                with self.assertRaises(SystemExit) as stopped:
                    hybrid_bisect.main([
                        '--image', str(image), '--executable', str(image),
                        '--runner', str(image), '--game-dir', str(game),
                        '--manifest', str(manifest), '--abi-header', str(header),
                        '--output', str(root / 'output'), '--keep', '0x1'])
                self.assertEqual(stopped.exception.code, 2)
                self.assertEqual(link.call_count, 2)
                for call in link.call_args_list:
                    self.assertEqual(call.kwargs['variadic_names'], {'status'})

    def test_small_region_and_multiple_changed_channels_count_pixels(self):
        with tempfile.TemporaryDirectory() as directory:
            original = Path(directory) / 'original.ppm'
            patched = Path(directory) / 'patched.ppm'
            pixels = bytearray(100 * 3)
            original.write_bytes(b'P6\n10 10\n255\n' + pixels)
            patched.write_bytes(original.read_bytes())
            self.assertEqual(hybrid_bisect.frame_difference(original, patched), 0)
            pixels[30:33] = b'\x01\x02\x03'
            patched.write_bytes(b'P6\n10 10\n255\n' + pixels)
            difference = hybrid_bisect.frame_difference(original, patched)
            self.assertEqual(difference, 0.01)
            self.assertLess(difference, hybrid_bisect.FRAME_DIFF_LIMIT)
            self.assertGreater(difference, 0)
