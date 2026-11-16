# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Asset-free checks of strict relocation matching and evidence invalidation."""

import io
from pathlib import Path
import json
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import recompilation_status as matching
import decomp_status
from omf_splice import Module, Segment, Public, Fixup, OMFError


class RecompilationTests(unittest.TestCase):
    def fixture(self):
        row = dict(addr='0x00000010', name='candidate', size='6', module='candidate')
        module = Module()
        segment = Segment('_TEXT', 'CODE', 10, 1, True)
        # A public starts inside the module, not at offset zero.
        segment.data[:] = b'\x90\x90\xe8\0\0\0\0\xc3\x90\x90'
        segment.written[:] = b'\1' * 10
        module.segments.append(segment)
        module.publics.append(Public('candidate_', 1, 2))
        module.externs.append('callee_')
        module.fixups.append(Fixup(1, 3, 9, 'self', 5, None, 2, 1, 0))
        image = bytearray(64)
        image[16:22] = b'\xe8\x0b\0\0\0\xc3'  # call 0x20 from 0x10
        functions = [row, dict(addr='0x00000020', name='callee')]
        return module, row, image, functions

    def test_external_relocation_exact_without_padding(self):
        module, row, image, functions = self.fixture()
        result = matching.compare(module, row, 6, image, functions, [])
        self.assertEqual(result['state'], 'exact')
        self.assertEqual(result['original_sha256'], result['linked_sha256'])
        self.assertEqual(result['compiled_bytes'], 6)
        # Including compiler padding is a different byte stream.
        self.assertEqual(matching.compare(module, row, 8, image, functions, [])['state'], 'different')

    def test_changed_instruction_and_unresolved_external(self):
        module, row, image, functions = self.fixture()
        image[21] = 0x90
        self.assertEqual(matching.compare(module, row, 6, image, functions, [])['state'], 'different')
        with self.assertRaises(OMFError):
            matching.compare(module, row, 6, image, [row], [])

    def test_internal_relocation_and_crossed_public_rejected(self):
        module, row, image, functions = self.fixture()
        module.fixups[0] = Fixup(1, 3, 9, 'self', 5, None, 0, 1, 0)
        with self.assertRaises(OMFError):
            matching.compare(module, row, 6, image, functions, [])
        module.fixups.clear()
        module.publics.append(Public('other_', 1, 7))
        with self.assertRaises(OMFError):
            matching.compare(module, row, 6, image, functions, [])

    def test_source_change_is_stale_in_status(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / 'decomp/src').mkdir(parents=True)
            source = root / 'decomp/src/candidate.c'
            source.write_text('void candidate(void) {}\n')
            row = decomp_status.new_row(16, 6, 'candidate')
            row.update(module='candidate', state='compiles')
            record = dict(addr=row['addr'], state='exact',
                          function={k: row[k] for k in ('addr', 'name', 'size', 'module')},
                          inputs=matching.inputs(root, row), flags=list(matching.FLAGS))
            self.assertTrue(matching.current(record, row, root))
            source.write_text('void candidate(void) { /* changed */ }\n')
            self.assertFalse(matching.current(record, row, root))
            path = root / 'report.json'
            path.write_text(json.dumps(dict(format=matching.FORMAT, records={row['addr']: record})))
            out = io.StringIO()
            decomp_status.status([row], out, recompilation=path, root=root)
            self.assertRegex(out.getvalue(), r'exact\s+0 functions')
            self.assertRegex(out.getvalue(), r'stale\s+1 functions')


if __name__ == '__main__':
    unittest.main()
