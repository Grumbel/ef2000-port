# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

import struct
import unittest

from tools.inspect_exe import inspect


class InspectTests(unittest.TestCase):
    def image(self):
        data = bytearray(512)
        data[:2] = b'MZ'
        struct.pack_into('<13H', data, 2, 0, 1, 0, 2, 0, 65535,
                         0, 0, 0, 0, 0, 28, 0)
        return data

    def test_overlay_is_not_part_of_dos_image(self):
        report = inspect(self.image() + b'payload')
        self.assertEqual(report['dos_image_end'], 512)
        self.assertEqual(report['trailing_bytes'], 7)
        self.assertEqual(report['dos_entry_file_offset'], 32)

    def test_relocation_coordinates(self):
        data = self.image()
        struct.pack_into('<H', data, 6, 1)
        struct.pack_into('<HH', data, 28, 3, 2)
        self.assertEqual(inspect(data)['relocations'][0]['file_offset'], 67)

    def test_rejects_truncated_image(self):
        with self.assertRaises(ValueError):
            inspect(self.image()[:-1])

    def test_rejects_invalid_entry(self):
        data = self.image()
        struct.pack_into('<H', data, 22, 32)
        with self.assertRaises(ValueError):
            inspect(data)

    def test_rejects_invalid_relocation_table(self):
        data = self.image()
        struct.pack_into('<H', data, 6, 2)
        with self.assertRaises(ValueError):
            inspect(data)


if __name__ == '__main__':
    unittest.main()
