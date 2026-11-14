# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

from tools import asm_inventory, asm_to_c


class InventoryTests(unittest.TestCase):
    def test_wait_grouping_preserves_raw_evidence_and_instruction_starts(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image = root/'payload.bin'
            image.write_bytes(bytes.fromhex('9b9bd97c24109bd96c2410c3'))
            rows = [dict(addr='0x0', size='12', name='wait_fixture', state='unknown', kind='asm', module='')]
            expected = [[0, 1], [1, 1], [2, 4], [6, 1], [7, 4], [11, 1]]
            for index, raw in enumerate(([[0, 1], [1, 5], [6, 1], [7, 4], [11, 1]],
                                         [[0, 6], [6, 5], [11, 1]], expected)):
                bodies = self.write_bodies(root, image, [dict(address=0, ranges=[[0, 12]], instruction_spans=raw)])
                output = root/str(index)
                manifest = asm_inventory.export_inventory(image, rows, output, body_ranges=bodies)
                _, records = asm_to_c.read_inventory(output, image)
                record = records[0]
                self.assertEqual(manifest['schema'], 4)
                self.assertEqual(manifest['instruction_normalization'], asm_inventory.NORMALIZATION)
                self.assertEqual(record['ghidra_instruction_spans'], raw)
                self.assertEqual(record['normalized_ghidra_instruction_spans'], expected)
                self.assertTrue(record['ghidra_instruction_layout_matches'])
                self.assertEqual([i['mnemonic'] for i in record['instructions']],
                                 ['fwait', 'fwait', 'fnstcw', 'fwait', 'fldcw', 'ret'])
                self.assertEqual(bytes.fromhex(''.join(i['bytes'] for i in record['instructions'])), image.read_bytes())
                self.assertEqual((output/'ghidra-body-ranges.json').read_bytes(), bodies.read_bytes())
                modified = dict(record, normalized_ghidra_instruction_spans=[[0, 12]])
                (output/'00000000.json').write_text(json.dumps(modified))
                with self.assertRaisesRegex(ValueError, 'normalized Ghidra'):
                    asm_to_c.read_inventory(output, image)
                (output/'00000000.json').write_text(json.dumps(record))
                evidence = output/'ghidra-body-ranges.json'
                evidence.write_text(evidence.read_text()+' ')
                with self.assertRaisesRegex(ValueError, 'normalized inventory provenance'):
                    asm_to_c.read_inventory(output, image)

    def test_wait_normalization_does_not_fill_gaps_or_merge_other_instructions(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for index, (encoded, layout) in enumerate((
                    ('9b90c3', [[0, 2], [2, 1]]),
                    ('9b9bd97c2410c3', [[0, 1], [2, 4], [6, 1]]),
                    ('9bd97c24109bc3', [[0, 6], [6, 1]]))):
                image = root/'payload.bin'
                image.write_bytes(bytes.fromhex(encoded))
                rows = [dict(addr='0x0', size=str(len(image.read_bytes())), name='fixture',
                             state='unknown', kind='asm', module='')]
                bodies = self.write_bodies(root, image, [dict(address=0, ranges=[[0, len(image.read_bytes())]],
                                                            instruction_spans=layout)])
                output = root/str(index)
                manifest = asm_inventory.export_inventory(image, rows, output, body_ranges=bodies)
                self.assertFalse(manifest['entries'][0]['ghidra_instruction_layout_matches'])
                record = json.loads((output/'00000000.json').read_text())
                with self.assertRaises(asm_to_c.Unsupported):
                    asm_to_c.translate(record)
                record['ghidra_instruction_layout_matches'] = True
                (output/'00000000.json').write_text(json.dumps(record))
                with self.assertRaisesRegex(ValueError, 'normalized Ghidra'):
                    asm_to_c.read_inventory(output, image)

    def fixture(self, root):
        image = root / 'payload.bin'
        image.write_bytes(bytes.fromhex('e80100000090c3'))
        rows = [dict(addr='0x0', size='7', name='fixture', state='drafted',
                     kind='game', module='fixture'),
                dict(addr='0x6', size='1', name='tail', state='unknown',
                     kind='asm', module='')]
        return image, rows

    def test_real_decoder_bytes_targets_overlap_and_reproducibility(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, rows = self.fixture(root)
            first = asm_inventory.export_inventory(image, rows, root / 'a')
            second = asm_inventory.export_inventory(image, rows, root / 'b')
            self.assertEqual(first, second)
            for path in (root / 'a').iterdir():
                self.assertEqual(path.read_bytes(), (root / 'b' / path.name).read_bytes())
            record = json.loads((root / 'a/00000000.json').read_text())
            self.assertEqual(record['overlapping_entries'], [6])
            self.assertEqual(record['instructions'][0]['direct_target'], 6)
            self.assertEqual(record['instructions'][0]['bytes'], 'e801000000')
            self.assertEqual(record['boundary'], 'unverified-linear-window')
            with self.assertRaises(FileExistsError):
                asm_inventory.export_inventory(image, rows, root / 'a')

    def test_control_flow_keeps_calls_and_conditional_fallthrough(self):
        def insn(address, mnemonic, target=None):
            result = dict(address=address, bytes='90', mnemonic=mnemonic, operands='')
            if target is not None:
                result['direct_target'] = target
            return result
        report = asm_inventory.control_flow_report([
            insn(0, 'call', 20), insn(1, 'jne', 0), insn(2, 'jmp'),
            insn(3, 'ret'), insn(4, 'int'), insn(5, '(bad)')])
        edges = report['edges']
        self.assertIn(dict(site=0, kind='fallthrough', target=1,
                           status='decoded-instruction'), edges)
        self.assertIn(dict(site=1, kind='fallthrough', target=2,
                           status='decoded-instruction'), edges)
        self.assertFalse(any(e['site'] in (2, 3) and e['kind'] == 'fallthrough'
                             for e in edges))
        self.assertEqual({e['status'] for e in report['review_edges']},
                         {'outside-decoded-starts', 'indirect-unresolved',
                          'system-transition', 'undecodable'})

    def test_target_inside_instruction_is_not_resolved(self):
        report = asm_inventory.control_flow_report([
            dict(address=0, bytes='ebff', mnemonic='jmp', operands='1', direct_target=1)])
        self.assertEqual(report['review_edges'][0]['target'], 1)
        self.assertEqual(report['review_edges'][0]['status'], 'outside-decoded-starts')

    def write_bodies(self, root, image, entries):
        path = root / 'bodies.json'
        path.write_text(json.dumps(dict(schema=1, boundary='ghidra-body-ranges',
            payload_sha256=hashlib.sha256(image.read_bytes()).hexdigest(), entries=entries)))
        return path

    def test_detached_ranges_exclude_gap_and_keep_cross_span_edge(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image = root / 'payload.bin'
            image.write_bytes(bytes.fromhex('eb02ffffc3'))
            rows = [dict(addr='0x0', size='3', name='fixture', state='drafted',
                         kind='game', module='fixture')]
            bodies = self.write_bodies(root, image,
                                      [dict(address=0, ranges=[[0, 2], [4, 5]],
                                            instruction_spans=[[0, 2], [4, 1]])])
            result = asm_inventory.export_inventory(image, rows, root / 'out', body_ranges=bodies)
            record = json.loads((root / 'out/00000000.json').read_text())
            self.assertEqual([i['address'] for i in record['instructions']], [0, 4])
            self.assertEqual(record['ranges'], [[0, 2], [4, 5]])
            self.assertTrue(record['ghidra_instruction_layout_matches'])
            self.assertEqual(record['control_flow']['review_edges'], [])
            self.assertEqual(result['boundary'], 'ghidra-body-ranges')
            self.assertEqual(result['body_ranges_sha256'], hashlib.sha256(bodies.read_bytes()).hexdigest())

    def test_data_in_body_is_not_promoted_to_instruction_agreement(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, rows = self.fixture(root)
            bodies = self.write_bodies(root, image, [dict(address=0, ranges=[[0, 7]],
                instruction_spans=[[0, 5], [6, 1]])])
            manifest = asm_inventory.export_inventory(image, rows[:1], root / 'out',
                                                      body_ranges=bodies)
            self.assertFalse(manifest['entries'][0]['ghidra_instruction_layout_matches'])

    def test_invalid_body_provenance_ranges_and_missing_entry_fail_closed(self):
        cases = [
            [dict(address=0, ranges=[[0, 8]])],
            [dict(address=0, ranges=[[0, 2], [1, 3]])],
            [dict(address=0, ranges=[[1, 2]])],
            [dict(address=0, ranges=[[0, 1]]), dict(address=0, ranges=[[0, 2]])],
            [dict(address=1, ranges=[[1, 2]])],
        ]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, rows = self.fixture(root)
            for entries in cases:
                with self.subTest(entries=entries):
                    bodies = self.write_bodies(root, image, entries)
                    with self.assertRaises(ValueError):
                        asm_inventory.export_inventory(image, rows[:1], root / 'out', body_ranges=bodies)
                    self.assertFalse((root / 'out').exists())
            bodies = self.write_bodies(root, image, [])
            image.write_bytes(b'changed')
            with self.assertRaisesRegex(ValueError, 'provenance'):
                asm_inventory.export_inventory(image, rows[:1], root / 'out', body_ranges=bodies)

    def test_body_ending_inside_instruction_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, rows = self.fixture(root)
            bodies = self.write_bodies(root, image, [dict(address=0, ranges=[[0, 2]])])
            def runner(argv, **kwargs):
                text = 'fake decoder' if '--version' in argv else '0: e8 01 00 00 00 call 6'
                return subprocess.CompletedProcess(argv, 0, text, '')
            with self.assertRaisesRegex(ValueError, 'byte/continuity mismatch'):
                asm_inventory.export_inventory(image, rows[:1], root / 'out',
                                               body_ranges=bodies, runner=runner)

    def test_reject_wrong_decoder_bytes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, rows = self.fixture(root)
            def runner(argv, **kwargs):
                text = 'fake decoder' if '--version' in argv else '0: c3 ret'
                return subprocess.CompletedProcess(argv, 0, text, '')
            with self.assertRaisesRegex(ValueError, 'byte/continuity mismatch'):
                asm_inventory.export_inventory(image, rows, root / 'out', runner=runner)

    def test_validate_extents_before_writing(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, rows = self.fixture(root)
            rows[0]['size'] = '100'
            with self.assertRaisesRegex(ValueError, 'invalid linear window'):
                asm_inventory.export_inventory(image, rows, root / 'out')
            self.assertFalse((root / 'out').exists())


if __name__ == '__main__':
    unittest.main()
