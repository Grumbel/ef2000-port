# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Synthetic fixtures exercise the loader parser without distributing game code."""

import struct
import unittest

from tools.extract_image import decode_layout, extract, relocate_words


class LayoutTests(unittest.TestCase):
    def image(self):
        data = bytearray(0x600)
        data[:2] = b'MZ'
        struct.pack_into('<13H', data, 2, 0, 3, 0, 3, 0, 65535,
                         0, 0, 0, 0, 0, 28, 0)
        data[0x28:0x2c] = b'B23X'
        struct.pack_into('<HH', data, 0x30, 0x100, 0x200)
        struct.pack_into('<H', data, 0x38, 0x10)
        struct.pack_into('<H', data, 0x42, 4)
        struct.pack_into('<HHH', data, 0x100, 0, 6, 0xffff)
        struct.pack_into('<II', data, 0x144, 0x300, 0x500)
        return data

    def test_layout_boundaries(self):
        layout = decode_layout(self.image())
        self.assertEqual(layout['payload_file_offset'], 0x300)
        self.assertEqual(layout['initialized_size'], 0x300)
        self.assertEqual(layout['bss_end'], 0x500)
        self.assertEqual(layout['stage2_relocation_offsets'], [0, 6])
        self.assertEqual(layout['stage2_relocation_table_size'], 6)

    def test_rejects_payload_length_disagreement(self):
        data = self.image()
        struct.pack_into('<I', data, 0x144, 0x301)
        with self.assertRaisesRegex(ValueError, 'file length'):
            decode_layout(data)

    def test_rejects_invalid_bss(self):
        data = self.image()
        struct.pack_into('<I', data, 0x148, 0x100)
        with self.assertRaisesRegex(ValueError, 'BSS'):
            decode_layout(data)

    def test_rejects_invalid_relocation(self):
        data = self.image()
        struct.pack_into('<H', data, 0x100, 0x1bf)
        with self.assertRaisesRegex(ValueError, 'relocation target'):
            decode_layout(data)

    def test_rejects_unterminated_relocations(self):
        data = self.image()
        data[0x100:0x140] = bytes(0x40)
        with self.assertRaisesRegex(ValueError, 'unterminated'):
            decode_layout(data)

    def test_rejects_out_of_file_read(self):
        data = self.image()
        struct.pack_into('<H', data, 0x32, 0xffff)
        with self.assertRaisesRegex(ValueError, 'file range'):
            decode_layout(data)

    def test_cli_extraction_rejects_unknown_release(self):
        with self.assertRaisesRegex(ValueError, 'SHA-256'):
            extract(self.image())


class RelocationTests(unittest.TestCase):
    def test_segment_wraparound_and_unchanged_input(self):
        data = bytes.fromhex('ff ff 12 34 00 00')
        self.assertEqual(relocate_words(data, [0, 4], 2),
                         bytes.fromhex('01 00 12 34 02 00'))
        self.assertEqual(data, bytes.fromhex('ff ff 12 34 00 00'))

    def test_rejects_out_of_bounds_word(self):
        for offset in [-1, 1]:
            with self.assertRaises(ValueError):
                relocate_words(b'\0\0', [offset], 0)

    def test_rejects_invalid_segment(self):
        for segment in [-1, 65536]:
            with self.assertRaises(ValueError):
                relocate_words(b'\0\0', [0], segment)


if __name__ == '__main__':
    unittest.main()
