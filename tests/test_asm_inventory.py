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
    def test_trace_gaps_group_runs_and_ignore_out_of_image_markers(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image = root/'payload.bin'
            payload = bytes.fromhex('9bd9e8c390909090')
            image.write_bytes(payload)
            bodies = self.write_bodies(root, image, [dict(address=0, ranges=[[0, 4]],
                instruction_spans=[[0, 3], [3, 1]])])
            trace = root/'trace.json'
            document = dict(payload_sha256=hashlib.sha256(payload).hexdigest(), entries=[
                dict(root=0, trace=[[0, 1], [1, 2], [4, 1], [5, 1], [3, 1], [99, 1]]),
                dict(root=1, trace=[1, 4, 6, 99, 7]),
            ])
            trace.write_text(json.dumps(document))
            result = asm_inventory.audit_trace_gaps(payload, bodies, trace)
            self.assertEqual(result['entries'], [
                dict(source=1, target=4, hits=2, roots=[0, 1], observed_pcs=[4, 5, 6]),
                dict(source=None, target=7, hits=1, roots=[1], observed_pcs=[7]),
            ])
            document['payload_sha256'] = 'invalid'
            trace.write_text(json.dumps(document))
            with self.assertRaisesRegex(ValueError, 'trace provenance'):
                asm_inventory.audit_trace_gaps(payload, bodies, trace)

    def test_trace_table_keeps_aliases_and_reports_unmatched_targets(self):
        payload = bytes.fromhex('0000000009000000ffff09000000ffff08000000')
        gaps = [dict(source=1, target=9), dict(source=1, target=10), dict(source=2, target=8)]
        report = asm_inventory.audit_trace_table(payload, gaps, 1, 4, len(payload), 6)
        self.assertEqual(report['matches'], [dict(target=9, slots=[4, 10])])
        self.assertEqual(report['unmatched_targets'], [10])
        with self.assertRaisesRegex(ValueError, 'bounds or stride'):
            asm_inventory.audit_trace_table(payload, gaps, 1, 4, len(payload), 2)

    def test_candidate_body_checks_both_branch_paths_and_call_ownership(self):
        instructions = [
            dict(address=0, bytes='7405', mnemonic='je', direct_target=7),
            dict(address=2, bytes='e809000000', mnemonic='call', direct_target=16),
            dict(address=7, bytes='c3', mnemonic='ret'),
        ]
        owned = bytearray(17)
        report = asm_inventory.audit_candidate_body(instructions, 0, 8, owned, {16})
        self.assertEqual(report['end_exclusive'], 8)
        self.assertEqual(report['return_sites'], [7])
        self.assertEqual(report['blockers'], [])
        report = asm_inventory.audit_candidate_body(instructions, 0, 8, owned, set())
        self.assertEqual(report['blockers'][0]['kind'], 'call')
        owned[3] = 1
        report = asm_inventory.audit_candidate_body(instructions, 0, 8, owned, {16})
        self.assertIn('owned boundary', report['blockers'][0]['reason'])

    def test_candidate_audit_rejects_bypassed_or_invalid_jump_table_guards(self):
        with tempfile.TemporaryDirectory() as temporary:
            image = Path(temporary) / 'payload.bin'
            # Two table slots precede the body at 8. The JA and second
            # table slot both reach the second RET.
            table = bytes.fromhex('1d000000 1e000000')
            code = bytes.fromhex('3c01 0f870e000000 25ff000000 2eff248500000000 c3c3')

            def audit(data):
                image.write_bytes(data)
                instructions = asm_inventory.decode_window(image, 8, len(data) - 8)
                return asm_inventory.audit_candidate_body(
                    instructions, 8, len(data), bytearray(len(data)), set(), payload=data)

            valid = audit(table + code)
            self.assertEqual(valid['blockers'], [])
            self.assertEqual(valid['guarded_jump_tables'][0]['targets'], [29, 30])
            bypass = table + code[:4] + bytes.fromhex('05000000') + code[8:]
            self.assertTrue(audit(bypass)['blockers'])
            invalid_slot = bytes.fromhex('1c000000') + table[4:] + code
            self.assertTrue(audit(invalid_slot)['blockers'])

    def test_table_audit_requires_a_resolved_direct_call_chain(self):
        with tempfile.TemporaryDirectory() as temporary:
            image = Path(temporary) / 'payload.bin'
            # Both calls target the next six-byte body.
            payload = bytes.fromhex('e801000000c3 e801000000c3 c3')
            tables = [dict(matches=[dict(target=0)])]

            def audit(data, limit):
                image.write_bytes(data)
                return asm_inventory.audit_table_bodies(
                    image, data, {}, tables, max_bytes=32, max_callees=limit,
                    objdump='objdump')

            limited = audit(payload, 1)
            self.assertEqual([len(body['blockers']) for body in limited], [1, 1])
            complete = audit(payload, 2)
            self.assertEqual([body['candidate_callees'] for body in complete], [[6], [12], []])
            self.assertEqual([body['blockers'] for body in complete], [[], [], []])
            observed = asm_inventory.audit_table_bodies(
                image, payload, {}, [], max_bytes=32, max_callees=2,
                objdump='objdump', observed_entries=[0])
            self.assertEqual(observed, [dict(complete[0], origin='observed-gap'), *complete[1:]])
            base = self.write_bodies(image.parent, image, [])
            overlay = image.parent / 'overlay.json'
            evidence = dict(candidate_bodies=complete, trace_sha256='trace',
                            payload_sha256=hashlib.sha256(payload).hexdigest(),
                            body_ranges_sha256=hashlib.sha256(base.read_bytes()).hexdigest())
            count = asm_inventory.candidate_overlay(image, base, evidence, overlay, objdump='objdump')
            self.assertEqual(count, 3)
            bodies, _ = asm_inventory.load_body_ranges(overlay, payload)
            self.assertEqual(bodies[6]['instruction_spans'], [[6, 5], [11, 1]])
            rejected = audit(payload[:-1] + bytes.fromhex('ffe0'), 2)
            self.assertTrue(all(body['blockers'] for body in rejected))

    def test_branch_targeted_test_immediate_mov_is_preserved_and_rechecked(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image = root/'payload.bin'
            image.write_bytes(bytes.fromhex('740266a989cfc3'))
            rows = [dict(addr='0x0', size='7', name='overlap', state='unknown',
                         kind='asm', module='')]
            bodies = self.write_bodies(root, image, [dict(address=0, ranges=[[0, 7]],
                instruction_spans=[[0, 2], [2, 4], [6, 1]])])
            output = root/'inventory'
            asm_inventory.export_inventory(image, rows, output, body_ranges=bodies)
            _, records = asm_to_c.read_inventory(output, image)
            record = records[0]
            self.assertEqual([(i['address'], i['mnemonic']) for i in record['instructions']],
                             [(0, 'je'), (2, 'test'), (4, 'mov'), (6, 'ret')])
            self.assertEqual(record['size'], 7)
            self.assertIn('case 0x4u:', asm_to_c.translate(record))
            record['instructions'][2]['operands'] = 'esi,ecx'
            (output/'00000000.json').write_text(json.dumps(record))
            with self.assertRaisesRegex(ValueError, 'instruction decode mismatch'):
                asm_to_c.read_inventory(output, image)

            # Historical v1 inventories retain their original linear decoding.
            record['instructions'].pop(2)
            (output/'00000000.json').write_text(json.dumps(record))
            manifest_path = output/'manifest.json'
            manifest = json.loads(manifest_path.read_text())
            manifest['instruction_normalization'] = asm_inventory.PREVIOUS_NORMALIZATION
            manifest_path.write_text(json.dumps(manifest))
            _, previous = asm_to_c.read_inventory(output, image)
            with self.assertRaisesRegex(asm_to_c.Unsupported, 'unresolved or external branch'):
                asm_to_c.translate(previous[0])

            # Without an explicit branch to the embedded MOV, keep linear decoding.
            linear = asm_inventory.decode_window(image, 2, 5)
            self.assertEqual(asm_inventory.overlapping_test_moves(image, linear), linear)
            for encoded in ('740166a989cfc3', '740266a9890fc3', '740266a990cfc3'):
                image.write_bytes(bytes.fromhex(encoded))
                linear = asm_inventory.decode_window(image, 0, 7)
                self.assertEqual(asm_inventory.overlapping_test_moves(image, linear), linear)

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
