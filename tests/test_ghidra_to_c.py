# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Tests for the mechanical Ghidra-to-C ABI declarations."""

import unittest

from tools.ghidra_to_c import confirmed_alias_declarations


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


if __name__ == '__main__':
    unittest.main()
