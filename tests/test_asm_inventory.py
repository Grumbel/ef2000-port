# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

import json
from pathlib import Path
import subprocess
import tempfile
import unittest

from tools import asm_inventory


class InventoryTests(unittest.TestCase):
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
