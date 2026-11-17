# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Asset-free tests for the decompilation tracker; all inputs are synthetic."""

import io
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'tools'))
import decomp_status  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent


def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
    return path


GHIDRA_FUNCTIONS = (
    'entry\tname\tbody_bytes\tstatus\tfile\terror\n'
    '0x00000200\tFUN_00000200\t40\twritten\t\t\n'
    '0x00000100\tgpwExample\t12\twritten\t\t\n'
    '0x00000300\tcandidate_orphan_00000300\t8\twritten\t\t\n'
)
GHIDRA_CALLS = (
    'caller_entry\tcall_site\ttarget\tcallee_entry\tkind\n'
    '0x00000200\t0x00000210\t0x00000100\t0x00000100\tdirect\n'
    '0x00000200\t0x00000214\t0x00000300\t0x00000300\tdirect\n'
    '0x00000300\t0x00000304\t\t\tindirect_or_external\n'
)


class DecompStatusTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.tracker = self.root / 'decomp/symbols/functions.tsv'
        self.functions = write(self.root / 'functions.tsv', GHIDRA_FUNCTIONS)
        self.calls = write(self.root / 'calls.tsv', GHIDRA_CALLS)

    def tearDown(self):
        self.temporary.cleanup()

    def rows(self):
        return {row['addr']: row for row in decomp_status.read_tracker(self.tracker)}

    def test_seed_sorts_and_drops_placeholder_names(self):
        added, updated, missing = decomp_status.seed(self.tracker, self.functions)
        self.assertEqual((added, updated, missing), (3, 0, []))
        rows = decomp_status.read_tracker(self.tracker)
        self.assertEqual([row['addr'] for row in rows],
                         ['0x00000100', '0x00000200', '0x00000300'])
        self.assertEqual(rows[0]['name'], 'gpwExample')
        self.assertEqual(rows[0]['state'], 'named')
        self.assertEqual(rows[1]['name'], '')
        self.assertEqual(rows[1]['state'], 'unknown')
        self.assertEqual(rows[2]['name'], '')

    def test_reseed_keeps_manual_columns(self):
        decomp_status.seed(self.tracker, self.functions)
        rows = self.rows()
        rows['0x00000200'].update(name='draw_thing', module='views', state='named',
                                  notes='kept')
        decomp_status.write_tracker(self.tracker, rows.values())
        write(self.functions, GHIDRA_FUNCTIONS.replace('\t40\t', '\t44\t'))
        added, updated, _ = decomp_status.seed(self.tracker, self.functions)
        self.assertEqual((added, updated), (0, 1))
        row = self.rows()['0x00000200']
        self.assertEqual((row['name'], row['module'], row['notes'], row['size']),
                         ('draw_thing', 'views', 'kept', '44'))

    def test_seed_reports_rows_missing_from_ghidra(self):
        decomp_status.seed(self.tracker, self.functions)
        write(self.functions, GHIDRA_FUNCTIONS.replace(
            '0x00000300\tcandidate_orphan_00000300\t8\twritten\t\t\n', ''))
        _, _, missing = decomp_status.seed(self.tracker, self.functions)
        self.assertEqual(missing, [0x300])

    def test_next_requires_done_callees_and_skips_indirect(self):
        decomp_status.seed(self.tracker, self.functions)
        rows = decomp_status.read_tracker(self.tracker)
        callees, indirect = decomp_status.read_calls(self.calls)
        proposed = [row['addr'] for row in decomp_status.propose(rows, callees, indirect)]
        self.assertEqual(proposed, ['0x00000100'])
        with_indirect = decomp_status.propose(rows, callees, indirect, include_indirect=True)
        self.assertEqual([row['addr'] for row in with_indirect], ['0x00000300', '0x00000100'])
        for row in rows:
            if row['addr'] in ('0x00000100', '0x00000300'):
                row['kind'] = 'clib'
                row['state'] = 'library'
        proposed = [row['addr'] for row in decomp_status.propose(rows, callees, indirect)]
        self.assertEqual(proposed, ['0x00000200'])

    def test_check_accepts_seeded_tracker(self):
        decomp_status.seed(self.tracker, self.functions)
        self.assertEqual(decomp_status.check(decomp_status.read_tracker(self.tracker),
                                             self.root), [])

    def test_check_rejects_unsupported_claims(self):
        decomp_status.seed(self.tracker, self.functions)
        rows = decomp_status.read_tracker(self.tracker)
        rows[0].update(state='verified', module='hiscore', sig_status='confirmed',
                       test='decomp/tests/missing.json')
        rows[1].update(state='blocked')
        rows[2].update(state='library')
        errors = decomp_status.check(rows, self.root)
        joined = '\n'.join(errors)
        self.assertIn('missing decomp/src/hiscore.c', joined)
        self.assertIn('test file not found', joined)
        self.assertIn('requires notes', joined)
        self.assertIn('requires a library kind', joined)

    def test_check_rejects_unsorted_and_bad_values(self):
        rows = [decomp_status.new_row(0x200, 4, ''), decomp_status.new_row(0x100, 4, '')]
        rows[1]['kind'] = 'bogus'
        errors = decomp_status.check(rows, self.root)
        self.assertTrue(any('ascending' in error for error in errors))
        self.assertTrue(any('unknown kind' in error for error in errors))

    def test_summary_reports_byte_share(self):
        decomp_status.seed(self.tracker, self.functions)
        out = io.StringIO()
        decomp_status.summary(decomp_status.read_tracker(self.tracker), out)
        self.assertIn('3 functions, 60 body bytes', out.getvalue())

    def test_status_separates_compilation_from_verification(self):
        rows = []
        for index, state in enumerate(('compiles', 'verified', 'blocked', 'compiles-bad')):
            row = decomp_status.new_row(0x100 + index * 16, 10, '')
            row['state'] = state
            rows.append(row)
        out = io.StringIO()
        decomp_status.status(rows, out)
        lines = out.getvalue().splitlines()
        self.assertIn('Inventory: 4 functions; 40 original body bytes', lines)
        self.assertTrue(any('Compiles or later' in line and '50.0%' in line for line in lines))
        self.assertTrue(any('Verified equivalence' in line and '25.0%' in line for line in lines))
        self.assertIn('Byte-identical recompilation', out.getvalue())
        empty = io.StringIO()
        decomp_status.status([], empty)
        self.assertIn('0.0%', empty.getvalue())

    def test_translation_blockers_overlap_and_missing_inventory(self):
        rows = [decomp_status.new_row(address, 10, '')
                for address in (0x100, 0x200, 0x300, 0x400)]
        for row in rows:
            row['state'] = 'blocked'
        report = {'model': 'flat-x86-program-explicit-state', 'entries': [
            {'address': 0x100, 'state': 'generated-unverified'},
            {'address': 0x200, 'state': 'unsupported', 'root_blockers': [0x500, 0x500]},
            {'address': 0x300, 'state': 'unsupported', 'root_blockers': [0x500, 0x600]},
            {'address': 0x500, 'state': 'unsupported', 'reason': 'unsupported instruction: int'},
            {'address': 0x600, 'state': 'unsupported', 'reason': 'unsupported instruction: fsin'},
        ]}
        out = io.StringIO()
        decomp_status.translation_blockers(rows, report, out)
        lines = out.getvalue().splitlines()
        self.assertTrue(any('generated, unverified' in line and '1 functions' in line for line in lines))
        self.assertTrue(any('excluded by generator' in line and '2 functions' in line for line in lines))
        self.assertTrue(any('outside inventory' in line and '1 functions' in line for line in lines))
        self.assertTrue(any('excluded by generator' in line and line.endswith('20 body bytes')
                            for line in lines))
        self.assertTrue(any('0x00000500' in line and '2  unsupported' in line for line in lines))
        self.assertEqual([row['state'] for row in rows], ['blocked'] * 4)
        report['entries'].append(report['entries'][0])
        with self.assertRaises(decomp_status.TrackerError):
            decomp_status.translation_blockers(rows, report, io.StringIO())

    def test_service_dependency_counts_include_ports_and_preserve_unknowns(self):
        rows = [decomp_status.new_row(address, 10, '') for address in range(1, 7)]
        for row in rows:
            row['state'] = 'blocked'
        rows[5]['state'] = 'compiles'
        report = {'model': 'flat-x86-program-explicit-state', 'entries': [
            {'address': 1, 'state': 'generated-unverified', 'reachable_host_service_sites': [99]},
            {'address': 2, 'state': 'generated-unverified', 'reachable_host_service_sites': [],
             'reachable_host_port_service_sites': [98]},
            {'address': 3, 'state': 'generated-unverified', 'reachable_host_service_sites': [],
             'reachable_host_port_service_sites': []},
            {'address': 4, 'state': 'generated-unverified', 'reachable_host_service_sites': []},
            {'address': 5, 'state': 'generated-unverified'},
            {'address': 6, 'state': 'generated-unverified', 'reachable_host_service_sites': [99]},
        ]}
        out = io.StringIO()
        decomp_status.translation_blockers(rows, report, out)
        lines = dict((line.strip().rsplit(None, 2)[0], int(line.split()[-2]))
                     for line in out.getvalue().splitlines() if line.endswith(' functions'))
        self.assertEqual(lines, {'reachable sites': 2, 'no recorded reachable sites': 1,
                                 'incomplete reachability data': 2})
        self.assertIn('does not prove service support', out.getvalue())

    def test_committed_tracker_is_valid(self):
        tracker = ROOT / 'decomp/symbols/functions.tsv'
        if not tracker.exists():
            self.skipTest('no committed tracker')
        self.assertEqual(decomp_status.check(decomp_status.read_tracker(tracker), ROOT), [])


if __name__ == '__main__':
    unittest.main()
