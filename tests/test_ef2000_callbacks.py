# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Asset-free checks for distinguishing function/name tables from data/name tables."""

import struct
import unittest

from tools.ef2000_callbacks import find_tables, select_function_tables


class CallbackTableTests(unittest.TestCase):
    def setUp(self):
        self.image = bytearray(0x170400)

    def add_name(self, offset, value):
        self.image[offset:offset + len(value) + 1] = value.encode('ascii') + b'\0'

    def add_record(self, slot, handler, name_address):
        struct.pack_into('<II', self.image, slot, handler, name_address)

    def test_finds_tables_and_retains_unknown_function_handlers(self):
        for index, name in enumerate(('ClickStart', 'ClickStop', 'ClickPause', 'ClickNext')):
            self.add_name(0x170100 + index * 24, name)
            self.add_record(0x120100 + index * 8, 0x3000 + index * 0x20,
                            0x170100 + index * 24)
        tables = select_function_tables(find_tables(self.image), {0x3000, 0x3020})
        self.assertEqual(len(tables), 1)
        self.assertEqual(tables[0][0], 0x120100)
        self.assertEqual([row[-1] for row in tables[0][1]],
                         ['ClickStart', 'ClickStop', 'ClickPause', 'ClickNext'])

    def test_rejects_data_and_enum_name_tables(self):
        for index, name in enumerate(('Maverick', 'Panther', 'Hornet', 'Raven')):
            self.add_name(0x170100 + index * 24, name)
            self.add_record(0x120100 + index * 8, 0x160000 + index * 0x20,
                            0x170100 + index * 24)
        tables = find_tables(self.image)
        self.assertEqual(len(tables), 1)
        self.assertEqual(select_function_tables(tables, {0x3000, 0x3020}), [])

    def test_rejects_nonidentifier_name_and_short_patterns(self):
        for index, name in enumerate(('No spaces', 'Okay', 'Ready')):
            self.add_name(0x170100 + index * 24, name)
            self.add_record(0x120100 + index * 8, 0x3000 + index * 0x20,
                            0x170100 + index * 24)
        self.assertEqual(find_tables(self.image), [])
        with self.assertRaisesRegex(ValueError, 'at least two'):
            find_tables(self.image, minimum=1)


if __name__ == '__main__':
    unittest.main()
