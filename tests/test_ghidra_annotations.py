# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Asset-free symbol CLI tests and optional real Ghidra synthetic round trip."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from tools import ghidra_ef2000 as ghidra


class SymbolCommandTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.workspace = self.root / 'workspace'
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').touch()
        image = b'fictional fixture'
        (self.workspace / 'payload.bin').write_bytes(image)
        import hashlib
        (self.workspace / 'manifest.json').write_text(json.dumps({'protected_image': {
            'initialized_range': [0, ghidra.IMAGE_SIZE],
            'payload_sha256': hashlib.sha256(image).hexdigest()}}))
        self.symbols = self.root / 'symbols'
        self.symbols.mkdir()
        for name in ('functions.tsv', 'globals.tsv', 'signatures.tsv'):
            (self.symbols / name).write_text('header\n')
        self.launcher = self.root / 'analyzeHeadless'
        self.launcher.touch()

    def test_apply_requires_script_completion_marker(self):
        args = argparse.Namespace(workspace=self.workspace, analyze_headless=str(self.launcher),
                                  symbols=self.symbols, headers=self.root)
        with patch.object(ghidra.subprocess, 'run') as runner:
            with self.assertRaisesRegex(ValueError, 'did not complete'):
                ghidra.apply_symbols(args)
        command = runner.call_args.args[0]
        self.assertIn('EF2000ApplySymbols.java', command)
        self.assertEqual(command[-5], str(self.symbols / 'functions.tsv'))

    def test_export_refuses_canonical_tracker(self):
        args = argparse.Namespace(workspace=self.workspace, analyze_headless=str(self.launcher),
                                  symbols=self.symbols, output=self.symbols)
        with patch.object(ghidra.subprocess, 'run') as runner:
            with self.assertRaisesRegex(ValueError, 'refusing to overwrite'):
                ghidra.export_symbols(args)
        runner.assert_not_called()


@unittest.skipUnless(os.environ.get('GHIDRA_HOME'), 'set GHIDRA_HOME for live synthetic Ghidra test')
class SyntheticGhidraRoundtrip(unittest.TestCase):
    def test_roundtrip(self):
        with tempfile.TemporaryDirectory(prefix='ef2000-ghidra-symbol-test-') as temp:
            root = Path(temp)
            image = root / 'synthetic.bin'
            image.write_bytes(b'\xb8\x07\x00\x00\x00\xc3' + b'\x90' * 10 +
                              b'\xc3' + b'\x90' * 15 + b'\0' * 64)
            script_path = ghidra.SCRIPTS
            launcher = str(Path(os.environ['GHIDRA_HOME']) / 'support/analyzeHeadless')
            project = root / 'project'
            project.mkdir()
            def run(*args):
                return subprocess.run([launcher, str(project), 'Synthetic', '-scriptPath',
                                       str(script_path), *args], check=True,
                                      stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                      text=True, timeout=120).stdout
            run('-import', str(image), '-loader', 'BinaryLoader', '-loader-baseAddr', '0',
                '-processor', 'x86:LE:32:default', '-cspec', 'windows',
                '-preScript', 'EF2000SyntheticSeed.java')
            symbols = root / 'symbols'
            symbols.mkdir()
            (symbols / 'functions.tsv').write_text(
                'addr\tsize\tname\tmodule\tstate\tkind\tsig_status\ttest\tnotes\n'
                '0x00000000\t6\tfixture_recovered\tfixture\ttyped\tgame\tconfirmed\t\t\n'
                '0x00000010\t1\tfixture_void\tfixture\ttyped\tgame\tconfirmed\t\t\n')
            (symbols / 'globals.tsv').write_text(
                'addr\tsize\ttype\tname\tevidence\tnotes\n'
                '0x00000020\t4\tuint32_t\tfixture_value\tsynthetic fixture\t\n')
            (symbols / 'signatures.tsv').write_text(
                'addr\tname\tinputs\toutputs\tpreserved\tstack_bytes\tevidence\n'
                '0x00000000\tfixture_recovered\tEAX:ef_addr32\tEAX:uint32_t\t\t0\tsynthetic fixture\n'
                '0x00000010\tfixture_void\t\t\t\t0\tvoid fixture\n')
            headers = root / 'include'
            headers.mkdir()
            (headers / 'fixture.h').write_text(
                'struct fixture_record { unsigned int value; };\n'
                'typedef struct EF_PACKED fixture_packed {\n'
                '  char name[5]; unsigned int value;\n'
                '} fixture_packed;\n')
            marker = root / 'done'
            args = [str(symbols / name) for name in
                    ('functions.tsv', 'globals.tsv', 'signatures.tsv')]
            for attempt in range(2):
                marker.unlink(missing_ok=True)
                output = run('-process', image.name, '-noanalysis', '-postScript',
                             'EF2000ApplySymbols.java', *args, str(headers), str(marker))
                self.assertTrue(marker.is_file(), output)
            marker.unlink()
            output = run('-process', image.name, '-noanalysis', '-readOnly', '-postScript',
                         'EF2000VerifySynthetic.java', str(marker))
            self.assertEqual(marker.read_text() if marker.exists() else None,
                             'verified\n', output)
            review = root / 'review'
            output = run('-process', image.name, '-noanalysis', '-readOnly', '-postScript',
                         'EF2000ExportSymbols.java', *args, str(review))
            self.assertTrue((review / 'functions.tsv').is_file(), output)
            self.assertIn('fixture_recovered', (review / 'functions.tsv').read_text())
            self.assertIn('fixture_value', (review / 'globals.tsv').read_text())
            self.assertIn('EAX:uint32_t', (review / 'signatures.tsv').read_text())


if __name__ == '__main__':
    unittest.main()
