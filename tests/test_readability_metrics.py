# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

import unittest

from tools.readability_metrics import count_file


class ReadabilityMetricsTests(unittest.TestCase):
    def test_counts_literal_computed_and_unnamed(self):
        text = '''
            EF_U32(0x0019f34cu) = eax;
            EF_U8( 0x15ee9a ) = 1u;
            x = EF_U8(EF_U32(0x004191d0u) + 4u);
            guest_fn_35de0_x(0u, 0u);
            guest_fn_ab324(1u, 2u);
            g_difficulty = 3;
        '''
        self.assertEqual(count_file(text),
                         {'literal': 3, 'computed': 1, 'unnamed': 2})


if __name__ == '__main__':
    unittest.main()
