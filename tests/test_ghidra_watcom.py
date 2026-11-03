# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Asset-free tests for isolated, non-destructive Ghidra ABI-model setup."""

from pathlib import Path
import tempfile
import unittest
import xml.etree.ElementTree as ET

from tools import ghidra_watcom


class WatcomSpecTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / 'source'
        cspec = self.source / ghidra_watcom.CSPEC
        cspec.parent.mkdir(parents=True)
        self.original = '<compiler_spec>\n' + ghidra_watcom.SENTINEL + '\n  </resolveprototype>\n</compiler_spec>\n'
        cspec.write_text(self.original)
        launcher = self.source / 'support/analyzeHeadless'
        launcher.parent.mkdir()
        launcher.write_text('#!/bin/sh\n')
        self.target = self.root / 'watcom-copy'

    def test_prepare_copies_and_modifies_only_destination(self):
        ghidra_watcom.prepare(self.source, self.target)
        self.assertEqual((self.source / ghidra_watcom.CSPEC).read_text(), self.original)
        result = (self.target / ghidra_watcom.CSPEC).read_text()
        models = ET.fromstring(result).findall('prototype')
        self.assertEqual(len(models), 1)
        self.assertEqual(models[0].attrib['name'], ghidra_watcom.MODEL)
        self.assertEqual([x.find('register').attrib['name']
                          for x in models[0].find('input').findall('pentry')[:4]],
                         ['EAX', 'EDX', 'EBX', 'ECX'])
        with self.assertRaisesRegex(ValueError, 'already exists'):
            ghidra_watcom.prepare(self.source, self.target)

    def test_rejects_invalid_specs_before_copying(self):
        with self.assertRaisesRegex(ValueError, 'unsupported'):
            ghidra_watcom.inject_model('<compiler_spec/>', ghidra_watcom.TEMPLATE.read_text())
        with self.assertRaisesRegex(ValueError, 'separate'):
            ghidra_watcom.prepare(self.source, self.source / 'nested')
        self.assertFalse(self.target.exists())


if __name__ == '__main__':
    unittest.main()
