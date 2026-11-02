# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Asset-free fixtures for extracting original X-32 enum pointer arrays."""

import struct
import unittest

from tools.mission_enum_probe import (Table, TABLES, MOBILE_CLASS_ALIASES,
                                      decode_table, inspect_payload, read_c_string)


class MissionEnumProbeTests(unittest.TestCase):
    def test_pointer_order_and_latin1_byte_decoding(self):
        payload = bytearray(48)
        payload[16:22] = b'FIRST\0'
        payload[22:29] = b'SECOND\0'
        struct.pack_into('<II', payload, 0, 22, 16)
        result = decode_table(bytes(payload), Table('fixture', 0, 2, 0x1234))
        self.assertEqual(result['guest_reader'], '0x00001234')
        self.assertEqual(result['pointer_table'], '0x00000000')
        self.assertEqual(result['entries'], [
            {'index': 0, 'label': 'SECOND'}, {'index': 1, 'label': 'FIRST'}])

    def test_rejects_bad_pointers_and_unterminated_names(self):
        payload = struct.pack('<I', 0xffffffff) + b'A\0'
        with self.assertRaisesRegex(ValueError, 'outside the payload'):
            decode_table(payload, Table('bad', 0, 1, 0))
        payload = struct.pack('<I', 4) + b'A' * 270
        with self.assertRaisesRegex(ValueError, 'unterminated string'):
            decode_table(payload, Table('bad', 0, 1, 0))
        with self.assertRaisesRegex(ValueError, 'outside the payload'):
            decode_table(payload, Table('bad', 270, 2, 0))

    def test_stable_table_dimensions_and_aliases(self):
        self.assertEqual(len(TABLES), 11)
        self.assertEqual(sum(table.count for table in TABLES), 313)
        attack = next(table for table in TABLES if table.name == 'attack_node_actions')
        self.assertEqual((attack.base, attack.count, attack.reader),
                         (0x132cc4, 3, 0x0000ef98))
        self.assertEqual([index for index, _ in MOBILE_CLASS_ALIASES],
                         [0, 1, 2, 3, 0, 1, 2, 3])

    def test_rejects_unpinned_payload(self):
        with self.assertRaisesRegex(ValueError, 'SHA-pinned'):
            inspect_payload(b'not-original', selected=['waypoint_actions'])
        self.assertEqual(read_c_string(b'\xe9\0', 0), '\xe9')


if __name__ == '__main__':
    unittest.main()
