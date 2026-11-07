# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Asset-free tests for hybrid-link ABI entry aliases."""

import tempfile
import unittest
from pathlib import Path

from tools import hybrid_link


class AbiAliasTests(unittest.TestCase):
    def test_only_leading_address_annotation_is_an_entry_alias(self):
        with tempfile.TemporaryDirectory() as directory:
            header = Path(directory) / 'abi.h'
            header.write_text(
                'void direct_entry(void); /* 0x1234 EAX */\n'
                'void indirect_entry(void); /* calls through 0x5678 */\n'
                'void data_reference(void); /* [0x9abc] -> EAX */\n')

            self.assertEqual(hybrid_link.load_abi_aliases(header),
                             {'direct_entry': 0x1234})

    def test_real_header_does_not_alias_helpers_to_referenced_data_or_stubs(self):
        aliases = hybrid_link.load_abi_aliases(hybrid_link.ABI_HEADER)

        self.assertNotIn('guest_dos_int_x', aliases)
        self.assertNotIn('guest_allocate_parser_text', aliases)
        self.assertEqual(aliases['emit_command_packet'], 0xa3f74)


if __name__ == '__main__':
    unittest.main()
