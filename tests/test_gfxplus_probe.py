# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Synthetic-only tests for read-only GFX+ executable triage."""

import struct
import unittest

from tools.gfxplus_probe import all_offsets, inspect, pe_sections, possible_mz_header


def dos_header(data, offset, *, pages=3, size=64):
    data[offset:offset + 2] = b'MZ'
    struct.pack_into('<4H', data, offset + 2, 0, pages, 0, size // 16)
    struct.pack_into('<H', data, offset + 0x18, 0x1c)


def fixture():
    data = bytearray(4096)
    dos_header(data, 0)
    struct.pack_into('<I', data, 0x3c, 0x80)
    data[0x80:0x84] = b'PE\0\0'
    struct.pack_into('<HH', data, 0x84, 0x014c, 1)
    struct.pack_into('<H', data, 0x94, 0xe0)
    struct.pack_into('<H', data, 0x98, 0x10b)
    struct.pack_into('<I', data, 0x98 + 28, 0x400000)
    data[0x178:0x180] = b'DATA\0\0\0\0'
    struct.pack_into('<I', data, 0x184, 0x32000)
    struct.pack_into('<II', data, 0x188, 0xd00, 0x200)
    dos_header(data, 0x400)
    data[0x900:0x900 + 21] = b'Glide(tm) Version 2.1'
    data[0xb00:0xb00 + 11] = b'VOODOO_FILE'
    return data


class GFXPlusProbeTests(unittest.TestCase):
    def test_pe_markers_and_mz_candidate_are_file_offsets(self):
        report = inspect(bytes(fixture()))
        self.assertEqual(report['pe_machine'], 0x14c)
        self.assertEqual(report['format'], 'PE (updater or installer)')
        self.assertEqual(report['sections'][0]['name'], 'DATA')
        self.assertEqual(report['pe_image_base'], 0x400000)
        self.assertEqual(report['anchors']['glide_2_1'],
                         [{'file_offset': 0x900, 'section': 'DATA',
                           'pe_virtual_address': 0x432700}])
        self.assertEqual(report['anchors']['voodoo_file'],
                         [{'file_offset': 0xb00, 'section': 'DATA',
                           'pe_virtual_address': 0x432900}])
        candidates = report['possible_embedded_mz_headers']
        self.assertEqual(len(candidates), 1)
        self.assertEqual(candidates[0]['file_offset'], 0x400)
        self.assertEqual(candidates[0]['declared_dos_size'], 1536)
        self.assertNotIn('executable', candidates[0])

    def test_dos_exe_without_glide_is_not_inferred_to_be_gfx(self):
        data = bytearray(1536)
        dos_header(data, 0)
        data[0x100:0x104] = b'B23X'
        report = inspect(bytes(data))
        self.assertIsNone(report['pe_machine'])
        self.assertEqual(report['anchors']['x32_loader'][0]['file_offset'], 0x100)
        self.assertEqual(report['anchors']['glide_2_1'], [])
        self.assertEqual(report['possible_embedded_mz_headers'], [])

    def test_rejects_bad_pe_section_bounds(self):
        data = fixture()
        struct.pack_into('<II', data, 0x188, 0xffffffff, 0x200)
        with self.assertRaisesRegex(ValueError, 'beyond file'):
            pe_sections(bytes(data))

    def test_rejects_invalid_input_and_false_mz(self):
        with self.assertRaisesRegex(ValueError, 'MZ'):
            inspect(b'not DOS')
        data = bytearray(128)
        data[0:2] = b'MZ'
        data[32:34] = b'MZ'
        self.assertIsNone(possible_mz_header(data, 32))

    def test_offsets_include_overlaps(self):
        self.assertEqual(all_offsets(b'aaaa', b'aa'), [0, 1, 2])
        with self.assertRaises(ValueError):
            all_offsets(b'a', b'')


if __name__ == '__main__':
    unittest.main()
