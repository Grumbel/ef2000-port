# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Synthetic, asset-free regression tests of Ghidra subsystem affinity leads."""

import csv
import json
from pathlib import Path
import tempfile
import unittest

from tools import ghidra_subsystems as maps


class SubsystemMappingTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.workspace = Path(temporary.name)
        (self.workspace / 'subsystem-facts').mkdir()
        (self.workspace / 'full-export').mkdir()
        entries = [0x1000, 0x2000, 0x3000, 0x4000]
        self.write('subsystem-facts/function-facts.tsv', sorted(maps.FACT_COLUMNS), [
            dict({key: '0' for key in maps.FACT_COLUMNS},
                 entry=f'0x{entry:08x}', name=f'FUN_{entry:08x}') for entry in entries
        ])
        self.write('subsystem-facts/data-references.tsv', sorted(maps.REF_COLUMNS), [
            dict(entry=f'0x{entry:08x}', site=f'0x{entry:08x}',
                 target='0x00120100', ref_type='READ', source='ANALYSIS')
            for entry in entries[:2]
        ])
        self.write('subsystem-facts/address-candidates.tsv', sorted(maps.CANDIDATE_COLUMNS), [
            dict(entry='0x00003000', site='0x00003005', target='0x00120a00',
                 operand='1', form='scalar', region='runtime_candidate')
        ])
        self.write('subsystem-facts/cross-function-branches.tsv', sorted(maps.BRANCH_COLUMNS), [
            dict(entry='0x00003000', site='0x00003004', target='0x00004000',
                 target_entry='0x00004000', kind='cross_function_jump')
        ])
        self.write('full-export/calls.tsv', sorted(maps.CALL_COLUMNS), [
            dict(caller_entry='0x00002000', callee_entry='0x00003000', kind='direct')
        ])
        (self.workspace / 'full-export/report.json').write_text(json.dumps({
            'discovered_functions': 4,
        }))

    def write(self, name, columns, entries):
        with (self.workspace / name).open('w', newline='') as handle:
            writer = csv.DictWriter(handle, fieldnames=columns, delimiter='\t')
            writer.writeheader()
            writer.writerows(entries)

    def test_ranks_strong_shared_data_and_distinguishes_branch_evidence(self):
        count, group_count = maps.write_report(self.workspace, self.workspace / 'result',
                                               [0x1000, 0x3000], top=4, threshold=4)
        self.assertEqual(count, 4)
        self.assertEqual(group_count, 1)
        with (self.workspace / 'result/function-affinities.tsv').open() as stream:
            rankings = list(csv.DictReader(stream, delimiter='\t'))
        first = next(row for row in rankings if row['entry'] == '0x00001000')
        self.assertEqual(first['neighbor'], '0x00002000')
        self.assertIn('exact:0x120100', first['reasons'])
        jump = next(row for row in rankings if row['entry'] == '0x00003000' and
                    row['neighbor'] == '0x00004000')
        self.assertEqual(jump['reasons'], 'cross_function_jump')
        self.assertEqual(json.loads((self.workspace / 'result/summary.json').read_text())[
            'cross_function_jump_pairs'], 1)

    def test_complete_linkage_does_not_merge_unrelated_endpoints(self):
        functions = {entry: {} for entry in [1, 2, 3, 4]}
        edges = {(1, 2): 10.0, (2, 3): 9.0, (3, 4): 8.0}
        groups = maps.groups(functions, edges, threshold=8)
        self.assertEqual(groups, [[1, 2], [3, 4]])

    def test_invalid_reference_and_missing_branch_file_fail_closed(self):
        (self.workspace / 'subsystem-facts/cross-function-branches.tsv').unlink()
        with self.assertRaises(FileNotFoundError):
            maps.load_facts(self.workspace)
        self.write('subsystem-facts/cross-function-branches.tsv', sorted(maps.BRANCH_COLUMNS), [
            dict(entry='0x00003000', site='0x00003004', target='0x00004000',
                 target_entry='0x00004000', kind='invented_jump')
        ])
        with self.assertRaisesRegex(ValueError, 'unknown cross-function branch kind'):
            maps.load_facts(self.workspace)

    def test_different_inventory_fails_closed(self):
        (self.workspace / 'full-export/report.json').write_text(json.dumps({
            'discovered_functions': 100,
        }))
        with self.assertRaisesRegex(ValueError, 'cover 4 of 100'):
            maps.load_facts(self.workspace)


if __name__ == '__main__':
    unittest.main()
