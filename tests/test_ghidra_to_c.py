# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Tests for the mechanical Ghidra-to-C ABI declarations."""

import unittest

from tools.ghidra_to_c import (confirmed_alias_declarations, convert,
                               normalize_type)


class RecoveredAliasTests(unittest.TestCase):
    def test_no_register_input_alias_uses_eax_carrier_without_confirmed_return(self):
        self.assertEqual(
            confirmed_alias_declarations(0x18118, ([], [])),
            ['#pragma aux ef_fn_00018118 parm caller []',
             'uint32_t ef_fn_00018118();'])

    def test_stack_only_alias_preserves_confirmed_return_type(self):
        signature = ([('[ESP+4]', 'uint32_t')], [('EAX', 'uint16_t')])
        self.assertEqual(
            confirmed_alias_declarations(0x1000, signature),
            ['#pragma aux ef_fn_00001000 parm caller [] value [eax]',
             'uint16_t ef_fn_00001000();'])

    def test_three_byte_unsigned_carrier(self):
        self.assertEqual(normalize_type('uint3'), 'uint32_t')

    def test_subfield_of_array_element_uses_byte_offset(self):
        signature, body, flags, _ = convert(
            'void FUN_00000001(void)\n{\n'
            '  uint16_t local_24[1];\n'
            '  uint32_t result;\n'
            '  result = local_24[0]._1_1_;\n'
            '  return;\n}', 1, {}, {})
        self.assertEqual(signature, 'void ef_fn_00000001(void)')
        self.assertIn('((char *)&local_24[0] + 1)', body)
        self.assertEqual(flags, set())


if __name__ == '__main__':
    unittest.main()
