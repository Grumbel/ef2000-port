# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Asset-free harness tests and opt-in original-vs-native high-score replay."""

import os
from pathlib import Path
import shutil
import struct
import tempfile
import unittest

from tools import highscores_differential as reference


class DifferentialFixtureTests(unittest.TestCase):
    def test_fixtures_are_bounded_guest_layouts_and_reproducible(self):
        cases = reference.scenarios(12)
        self.assertEqual(len(cases), 20)
        self.assertEqual(cases, reference.scenarios(12))
        self.assertNotEqual(cases, reference.scenarios(12, seed=3))
        for title, state, name, difficulty in cases:
            self.assertTrue(title)
            self.assertEqual(len(state), reference.STATE_BYTES)
            self.assertEqual(len(name), reference.NAME_BYTES)
            self.assertIn(0, name)
            self.assertIn(difficulty, (1, 2, 3))
        first = cases[0][1]
        self.assertEqual(struct.unpack_from('<i', first)[0], 1200)
        self.assertEqual(struct.unpack_from('<i', first, 4 + 21)[0], 1000)

    def test_rejects_unsupported_image_and_bad_names(self):
        with tempfile.TemporaryDirectory() as directory:
            fake = Path(directory) / 'payload.bin'
            fake.write_bytes(b'invalid original')
            with self.assertRaisesRegex(ValueError, 'SHA-pinned'):
                reference.load_image(payload_path=fake)
        with self.assertRaisesRegex(ValueError, 'exactly one'):
            reference.load_image()
        with self.assertRaisesRegex(ValueError, 'NUL-terminated'):
            reference.player_name('n' * 20)
        with self.assertRaisesRegex(ValueError, 'ten guest records'):
            reference.make_state(0, [1])


@unittest.skipUnless(os.environ.get('EF2000_EXE'),
                     'set EF2000_EXE to run original Unicorn/C++ differential checks')
class OriginalHighscoreDifferentialTests(unittest.TestCase):
    def test_original_guest_and_native_match(self):
        if not shutil.which('g++'):
            self.skipTest('requires a native C++ compiler')
        try:
            import unicorn  # noqa: F401
        except ImportError:
            self.skipTest('requires Python Unicorn')
        payload = reference.load_image(executable=Path(os.environ['EF2000_EXE']))
        self.assertEqual(reference.run(payload, 'g++', random_count=80), 88)


if __name__ == '__main__':
    unittest.main()
