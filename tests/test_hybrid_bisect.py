# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Pixel comparisons used to isolate small hybrid drawing defects."""

from pathlib import Path
import tempfile
import unittest

from tools import hybrid_bisect


class FrameDifferenceTest(unittest.TestCase):
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
