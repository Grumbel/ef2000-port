# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

import struct
import unittest

from tools.read_x32_probe import decode


class ReferenceRecordTests(unittest.TestCase):
    def record(self):
        record = bytearray(0x100)
        record[:8] = b'X32R\x04\0\0\0'
        record[0xfc:] = b'DONE'
        return record

    def test_decodes_packed_selectors_and_full_registers(self):
        record = self.record()
        struct.pack_into('<HHHHHH', record, 0xc, 0x2b, 0x33, 0x33, 0x40, 0x48, 0x3b)
        struct.pack_into('<I', record, 0x1c, 0x7f)
        struct.pack_into('<I', record, 0x48, 0x260)
        struct.pack_into('<I', record, 0x7c, 0x55660000)
        result = decode(record)
        self.assertEqual(result['entry']['ds'], 0x33)
        self.assertEqual(result['entry']['ss'], 0x3b)
        self.assertEqual(result['entry']['ds_limit'], 0x7f)
        self.assertEqual(result['3505']['ds_limit'], 0x260)
        self.assertEqual(result['dos_version']['ecx'], 0x55660000)

    def test_rejects_truncated_record_or_missing_completion(self):
        record = self.record()
        for bad in (record[:-1], record + b'\0', bytes(0x100)):
            with self.assertRaises(ValueError):
                decode(bad)


if __name__ == '__main__':
    unittest.main()
