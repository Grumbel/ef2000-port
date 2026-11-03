# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

import unittest

from tools.abi_samples import preserve_review_status, sample


class AbiSamplesTests(unittest.TestCase):
    def test_diverse_unique_sorted_output(self):
        image = bytearray(b'\x90' * 10000)
        rows = []
        for entry in range(100, 9100, 50):
            if entry % 200 == 100:
                image[entry:entry + 2] = b'\xf3\xa4'
            rows.append(dict(entry=f'0x{entry:x}', body_bytes='320',
                             instructions='30', direct_calls=str((entry // 50) % 4),
                             indirect_calls=str((entry // 100) % 2),
                             conditional_branches=str(entry % 9),
                             pcode_loads='20', pcode_stores='5'))
        chosen = sample(rows, bytes(image))
        self.assertEqual(len(chosen), 40)
        self.assertEqual(len({x['entry'] for x in chosen}), 40)
        self.assertEqual([x['entry'] for x in chosen],
                         sorted(x['entry'] for x in chosen))
        self.assertEqual({x['region'] for x in chosen}, set(map(str, range(5))))

    def test_prefers_two_known_callers(self):
        image = b'\x90' * 10000
        rows = [dict(entry=f'0x{entry:x}', body_bytes='12', instructions='5',
                     direct_calls='0', indirect_calls='0', conditional_branches='0',
                     pcode_loads='0', pcode_stores='0')
                for entry in range(100, 9100, 50)]
        counts = {entry: 2 for entry in range(100, 9100, 100)}
        chosen = sample(rows, image, caller_counts=counts)
        self.assertTrue(all(int(row['direct_callers']) >= 2 for row in chosen))

    def test_refuses_inadequate_inventory(self):
        with self.assertRaisesRegex(ValueError, 'at least 40'):
            sample([], b'\x90' * 500)

    def test_preserves_review_only_for_same_candidate_and_shape(self):
        rows = [dict(entry='0x00000010', shape='leaf', status='unreviewed'),
                dict(entry='0x00000020', shape='few_calls', status='unreviewed')]
        previous = [dict(entry='0x00000010', shape='leaf', status='confirmed'),
                    dict(entry='0x00000020', shape='leaf', status='partial_abi')]
        self.assertEqual([row['status'] for row in preserve_review_status(rows, previous)],
                         ['confirmed', 'unreviewed'])


if __name__ == '__main__':
    unittest.main()
