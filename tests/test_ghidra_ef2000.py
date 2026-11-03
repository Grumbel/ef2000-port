# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Asset-free tests of the headless Ghidra command and safety checks."""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

from tools import ghidra_ef2000 as ghidra


class GhidraWorkflowTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.workspace = Path(self.temp.name) / 'ghidra'
        self.executable = Path(self.temp.name) / 'game.exe'
        self.executable.write_bytes(b'fictional test fixture')
        self.image = b'\x90' * ghidra.IMAGE_SIZE
        self.manifest = {
            'protected_image': {
                'initialized_range': [0, ghidra.IMAGE_SIZE],
                'payload_sha256': hashlib.sha256(self.image).hexdigest(),
            }
        }
        # A Nix build sandbox does not provide /bin/true. The launcher is
        # mocked at subprocess.run, but the path still has to pass validation.
        launcher = Path(self.temp.name) / 'analyzeHeadless'
        launcher.write_text('#!/bin/sh\nexit 0\n')
        launcher.chmod(0o755)
        self.launcher = str(launcher)

    def import_args(self):
        return argparse.Namespace(executable=self.executable,
                                  workspace=self.workspace,
                                  analyze_headless=self.launcher)

    def test_environment_workspace_default_for_nix_workflow(self):
        with patch.dict(ghidra.os.environ, {'EF2000_GHIDRA_WORKSPACE': str(self.workspace)}), \
                patch.object(ghidra, 'audit_flow_gaps') as audit:
            ghidra.main(['audit-flow-gaps'])
            self.assertEqual(audit.call_args.args[0].workspace, self.workspace)
            explicit = self.workspace / 'other'
            ghidra.main(['audit-flow-gaps', '--workspace', str(explicit)])
            self.assertEqual(audit.call_args.args[0].workspace, explicit)

    def test_import_uses_protected_image_and_pinned_offsets(self):
        def fake_ghidra(command, check):
            self.assertTrue(check)
            self.assertIn('BinaryLoader', command)
            self.assertIn('x86:LE:32:default', command)
            self.assertIn('EF2000Seed.java', command)
            self.assertEqual(command[command.index('-loader-baseAddr') + 1], '0')
            (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')

        with patch.object(ghidra, 'extract', return_value=(self.manifest,
                     {ghidra.IMAGE_NAME: self.image})), \
                patch.object(ghidra.subprocess, 'run', side_effect=fake_ghidra):
            ghidra.import_image(self.import_args())
        self.assertEqual((self.workspace / 'payload.bin').read_bytes(), self.image)
        self.assertEqual(json.loads((self.workspace / 'manifest.json').read_text()),
                         self.manifest)

    def test_import_refuses_to_overwrite_existing_project(self):
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        with patch.object(ghidra, 'extract', return_value=(self.manifest,
                     {ghidra.IMAGE_NAME: self.image})), \
                patch.object(ghidra.subprocess, 'run') as run:
            with self.assertRaisesRegex(ValueError, 'already exists'):
                ghidra.import_image(self.import_args())
            run.assert_not_called()

    def test_import_refuses_mismatched_extracted_image(self):
        self.workspace.mkdir()
        (self.workspace / 'payload.bin').write_bytes(b'unrelated')
        with patch.object(ghidra, 'extract', return_value=(self.manifest,
                     {ghidra.IMAGE_NAME: self.image})), \
                patch.object(ghidra.subprocess, 'run') as run:
            with self.assertRaisesRegex(ValueError, 'different image'):
                ghidra.import_image(self.import_args())
            run.assert_not_called()

    def test_decompile_targets_existing_project_at_guest_addresses(self):
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        (self.workspace / 'payload.bin').write_bytes(self.image)
        (self.workspace / 'manifest.json').write_text(json.dumps(self.manifest))
        args = argparse.Namespace(workspace=self.workspace, analyze_headless=self.launcher,
                                  output=None, offsets=[0xe9a24, 0x8b274])

        def fake_ghidra(command, check):
            self.assertTrue(check)
            self.assertIn('-noanalysis', command)
            self.assertEqual(command[-2:], ['e9a24', '8b274'])
            self.assertIn('EF2000Decompile.java', command)
            output = self.workspace / 'decompiled'
            for address in args.offsets:
                (output / f'0x{address:08x}.c').write_text('pseudo C')

        with patch.object(ghidra.subprocess, 'run', side_effect=fake_ghidra):
            ghidra.decompile(args)

    def test_decompile_rejects_altered_payload(self):
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        (self.workspace / 'payload.bin').write_bytes(b'invalid')
        (self.workspace / 'manifest.json').write_text(json.dumps(self.manifest))
        args = argparse.Namespace(workspace=self.workspace, analyze_headless=self.launcher,
                                  output=None, offsets=[0xe9a24])
        with patch.object(ghidra.subprocess, 'run') as run:
            with self.assertRaisesRegex(ValueError, 'does not match'):
                ghidra.decompile(args)
            run.assert_not_called()

    def test_decompile_rejects_silent_headless_failure(self):
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        (self.workspace / 'payload.bin').write_bytes(self.image)
        (self.workspace / 'manifest.json').write_text(json.dumps(self.manifest))
        args = argparse.Namespace(workspace=self.workspace, analyze_headless=self.launcher,
                                  output=None, offsets=[0xe9a24])
        with patch.object(ghidra.subprocess, 'run'):
            with self.assertRaisesRegex(ValueError, 'without fresh decompilation'):
                ghidra.decompile(args)

    def test_copy_callers_uses_the_imported_project(self):
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        (self.workspace / 'payload.bin').write_bytes(self.image)
        (self.workspace / 'manifest.json').write_text(json.dumps(self.manifest))
        args = argparse.Namespace(workspace=self.workspace, analyze_headless=self.launcher,
                                  output=None)

        def fake_ghidra(command, check):
            self.assertTrue(check)
            self.assertIn('-noanalysis', command)
            self.assertIn('EF2000CopyCallers.java', command)
            (self.workspace / 'copy-callers.tsv').write_text('call_site\treturn_ip\n')

        with patch.object(ghidra.subprocess, 'run', side_effect=fake_ghidra):
            ghidra.copy_callers(args)

    def test_copy_callers_refuses_modified_image(self):
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        (self.workspace / 'payload.bin').write_bytes(b'invalid')
        (self.workspace / 'manifest.json').write_text(json.dumps(self.manifest))
        args = argparse.Namespace(workspace=self.workspace, analyze_headless=self.launcher,
                                  output=None)
        with patch.object(ghidra.subprocess, 'run') as run:
            with self.assertRaisesRegex(ValueError, 'does not match'):
                ghidra.copy_callers(args)
            run.assert_not_called()

    def test_inventory_produces_full_program_index_and_calls(self):
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        (self.workspace / 'payload.bin').write_bytes(self.image)
        (self.workspace / 'manifest.json').write_text(json.dumps(self.manifest))
        args = argparse.Namespace(command='inventory', workspace=self.workspace,
                                  analyze_headless=self.launcher, output=None)

        def fake_ghidra(command, check):
            self.assertTrue(check)
            self.assertIn('-noanalysis', command)
            self.assertEqual(command[-7:], ['EF2000ExportAll.java',
                                            str(self.workspace / 'full-export'),
                                            'inventory', '0', 'resume', 'none', 'none'])
            output = self.workspace / 'full-export'
            (output / 'functions.tsv').write_text('entry\tname\n')
            (output / 'calls.tsv').write_text('caller_entry\tcall_site\n')
            (output / 'candidate-entries.tsv').write_text('entry\treason\n')
            (output / 'orphan-ranges.tsv').write_text('start\tend_exclusive\n')
            (output / 'orphan-instructions.asm').write_text('; nothing unassigned\n')
            (output / 'report.json').write_text(json.dumps({
                'initialized_image_bytes': ghidra.IMAGE_SIZE,
                'discovered_functions': 42, 'decompilations_written': 0,
                'decompilations_reused': 0, 'decompilations_failed': 0,
                'decompilations_limited': 0,
            }))

        with patch.object(ghidra.subprocess, 'run', side_effect=fake_ghidra):
            ghidra.export_all(args)

    def test_custom_storage_fallback_is_independent_of_watcom_model(self):
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        (self.workspace / 'payload.bin').write_bytes(self.image)
        (self.workspace / 'manifest.json').write_text(json.dumps(self.manifest))
        args = argparse.Namespace(command='export-all', workspace=self.workspace,
                                  analyze_headless=self.launcher, output=None,
                                  limit=4, overwrite=True, watcom_fallback=True,
                                  custom_storage_fallback=True)
        calls = []

        def fake_ghidra(command, check):
            self.assertTrue(check)
            self.assertIn('-readOnly', command)
            calls.append(command[-5:])
            output = Path(command[-6])
            output.mkdir(parents=True, exist_ok=True)
            (output / 'functions').mkdir(exist_ok=True)
            (output / 'functions/0x00000086.c').write_text('staged C')
            (output / 'report.json').write_text(json.dumps({
                'initialized_image_bytes': ghidra.IMAGE_SIZE,
                'discovered_functions': 4203, 'decompilations_written': 4,
                'decompilations_reused': 0, 'decompilations_failed': 0,
                'decompilations_limited': 4199,
            }))
            (output / 'functions.tsv').write_text('entry\tstatus\n')
            (output / 'calls.tsv').write_text('call_site\ttarget\n')
            (output / 'candidate-entries.tsv').write_text('entry\treason\n')
            (output / 'orphan-ranges.tsv').write_text('start\tend_exclusive\n')
            (output / 'orphan-instructions.asm').write_text('; nothing unassigned\n')

        with patch.object(ghidra.subprocess, 'run', side_effect=fake_ghidra):
            ghidra.export_all(args)
        self.assertEqual(calls, [
            ['decompile', '4', 'resume', 'watcom', 'none'],
            ['decompile', '4', 'resume', 'watcom', 'storage'],
        ])
        self.assertEqual((self.workspace / 'full-export/functions/0x00000086.c').read_text(),
                         'staged C')

    def test_overwrite_storage_retry_publishes_only_fresh_staged_c(self):
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        (self.workspace / 'payload.bin').write_bytes(self.image)
        (self.workspace / 'manifest.json').write_text(json.dumps(self.manifest))
        args = argparse.Namespace(command='export-all', workspace=self.workspace,
                                  analyze_headless=self.launcher, output=None,
                                  limit=0, overwrite=True, custom_storage_fallback=True)
        output = self.workspace / 'full-export'
        failed_file = output / 'functions/000b/0x000b3084.c'
        good_file = output / 'functions/0000/0x000045a4.c'
        failed_file.parent.mkdir(parents=True)
        good_file.parent.mkdir(parents=True)
        failed_file.write_text('OLD custom signature C')
        good_file.write_text('keep this newly exported C')
        calls = []

        def fake_ghidra(command, check):
            calls.append(command[-2:])
            staged = Path(command[-6])
            self.assertNotEqual(staged, output)
            staged.mkdir(parents=True, exist_ok=True)
            (staged / 'functions/0000').mkdir(parents=True, exist_ok=True)
            (staged / 'functions/000b').mkdir(parents=True, exist_ok=True)
            if len(calls) == 1:
                self.assertEqual(command[-3], 'resume')
                self.assertEqual(failed_file.read_text(), 'OLD custom signature C')
                (staged / 'functions/0000/0x000045a4.c').write_text('NEW ordinary C')
                (staged / 'functions.tsv').write_text(
                    'entry\tstatus\n0x000045a4\twritten\n0x000b3084\tfailed\n')
            else:
                self.assertEqual(command[-3], 'resume')
                self.assertFalse((staged / 'functions/000b/0x000b3084.c').exists())
                self.assertEqual(good_file.read_text(), 'keep this newly exported C')
                (staged / 'functions/000b/0x000b3084.c').write_text('FRESH storage C')
                (staged / 'functions.tsv').write_text(
                    'entry\tstatus\n0x000045a4\treused\n'
                    '0x000b3084\twritten_storage_fallback\n')
            (staged / 'calls.tsv').write_text('caller_entry\tcall_site\n')
            (staged / 'candidate-entries.tsv').write_text('entry\treason\n')
            (staged / 'orphan-ranges.tsv').write_text('start\tend_exclusive\n')
            (staged / 'orphan-instructions.asm').write_text('; no orphan bytes\n')
            (staged / 'report.json').write_text(json.dumps({
                'initialized_image_bytes': ghidra.IMAGE_SIZE,
                'discovered_functions': 2, 'decompilations_written': 1,
                'decompilations_reused': 1, 'decompilations_failed': 0,
                'decompilations_limited': 0,
            }))

        with patch.object(ghidra.subprocess, 'run', side_effect=fake_ghidra):
            ghidra.export_all(args)
        self.assertEqual(len(calls), 2)
        self.assertEqual(failed_file.read_text(), 'FRESH storage C')
        self.assertEqual(good_file.read_text(), 'NEW ordinary C')

    def test_full_export_passes_limit_and_overwrite_to_ghidra(self):
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        (self.workspace / 'payload.bin').write_bytes(self.image)
        (self.workspace / 'manifest.json').write_text(json.dumps(self.manifest))
        args = argparse.Namespace(command='export-all', workspace=self.workspace,
                                  analyze_headless=self.launcher, output=None,
                                  limit=12, overwrite=True, fastcall_fallback=True)

        def fake_ghidra(command, check):
            self.assertTrue(check)
            self.assertEqual(command[-5:], ['decompile', '12', 'overwrite', 'fastcall', 'none'])
            output = self.workspace / 'full-export'
            (output / 'report.json').write_text(json.dumps({
                'initialized_image_bytes': ghidra.IMAGE_SIZE,
                'discovered_functions': 20, 'decompilations_written': 12,
                'decompilations_reused': 0, 'decompilations_failed': 0,
                'decompilations_limited': 8,
            }))
            (output / 'functions.tsv').write_text('entry\tstatus\n')
            (output / 'calls.tsv').write_text('call_site\ttarget\n')
            (output / 'candidate-entries.tsv').write_text('entry\treason\n')
            (output / 'orphan-ranges.tsv').write_text('start\tend_exclusive\n')
            (output / 'orphan-instructions.asm').write_text('; nothing unassigned\n')

        with patch.object(ghidra.subprocess, 'run', side_effect=fake_ghidra):
            ghidra.export_all(args)

    def test_full_export_rejects_missing_report_even_if_process_succeeds(self):
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        (self.workspace / 'payload.bin').write_bytes(self.image)
        (self.workspace / 'manifest.json').write_text(json.dumps(self.manifest))
        args = argparse.Namespace(command='export-all', workspace=self.workspace,
                                  analyze_headless=self.launcher, output=None,
                                  limit=0, overwrite=False)
        with patch.object(ghidra.subprocess, 'run'):
            with self.assertRaisesRegex(ValueError, 'without a fresh report.json'):
                ghidra.export_all(args)

    def test_full_export_rejects_conflicting_decompiler_models(self):
        args = argparse.Namespace(command='export-all', workspace=self.workspace,
                                  analyze_headless=self.launcher, output=None,
                                  limit=0, overwrite=True,
                                  fastcall_fallback=True, watcom_fallback=True)
        with patch.object(ghidra.subprocess, 'run') as run:
            with self.assertRaisesRegex(ValueError, 'only one decompiler fallback'):
                ghidra.export_all(args)
            run.assert_not_called()

    def test_recover_orphans_defaults_to_report_only(self):
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        (self.workspace / 'payload.bin').write_bytes(self.image)
        (self.workspace / 'manifest.json').write_text(json.dumps(self.manifest))
        args = argparse.Namespace(workspace=self.workspace,
                                  analyze_headless=self.launcher, output=None,
                                  apply=False, max=0, tier='strict')

        def fake_ghidra(command, check):
            self.assertTrue(check)
            self.assertIn('-noanalysis', command)
            self.assertEqual(command[-4:], [str(self.workspace / 'recovery-candidates.tsv'),
                                            'report', '0', 'strict'])
            (self.workspace / 'recovery-candidates.tsv').write_text(
                'entry\treason\tfirst_instruction\tjump_refs\tcall_refs\tdata_refs\t'
                'confidence\tresult\n'
                '0x00005630\trange_start\tPUSH EBX\t1\t0\t0\t'
                'strong_reference_and_prologue\tproposed\n')

        with patch.object(ghidra.subprocess, 'run', side_effect=fake_ghidra):
            ghidra.recover_orphans(args)

    def test_recover_orphans_requires_explicit_apply_for_mutation(self):
        args = argparse.Namespace(workspace=self.workspace,
                                  analyze_headless=self.launcher, output=None,
                                  apply=False, max=25, tier='strict')
        with patch.object(ghidra.subprocess, 'run') as run:
            with self.assertRaisesRegex(ValueError, '--max requires --apply'):
                ghidra.recover_orphans(args)
            run.assert_not_called()

    def test_recover_orphans_apply_is_explicit_and_bounded(self):
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        (self.workspace / 'payload.bin').write_bytes(self.image)
        (self.workspace / 'manifest.json').write_text(json.dumps(self.manifest))
        args = argparse.Namespace(workspace=self.workspace,
                                  analyze_headless=self.launcher, output=None,
                                  apply=True, max=25, tier='table')

        def fake_ghidra(command, check):
            self.assertTrue(check)
            self.assertEqual(command[-4:], [str(self.workspace / 'recovery-candidates.tsv'),
                                            'apply', '25', 'table'])
            (self.workspace / 'recovery-candidates.tsv').write_text(
                'entry\treason\tfirst_instruction\tjump_refs\tcall_refs\tdata_refs\t'
                'confidence\tresult\n'
                '0x00005630\trange_start\tPUSH EBX\t1\t0\t0\t'
                'strong_reference_and_prologue\tcreated\n')

        with patch.object(ghidra.subprocess, 'run', side_effect=fake_ghidra):
            ghidra.recover_orphans(args)

    def test_recover_orphans_rejects_missing_report(self):
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        (self.workspace / 'payload.bin').write_bytes(self.image)
        (self.workspace / 'manifest.json').write_text(json.dumps(self.manifest))
        args = argparse.Namespace(workspace=self.workspace,
                                  analyze_headless=self.launcher, output=None,
                                  apply=True, max=10, tier='strict')
        with patch.object(ghidra.subprocess, 'run') as run:
            with self.assertRaisesRegex(ValueError, 'without a fresh orphan-recovery report'):
                ghidra.recover_orphans(args)
            run.assert_called_once()

    def test_recover_calls_is_read_only_by_default(self):
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        (self.workspace / 'payload.bin').write_bytes(self.image)
        (self.workspace / 'manifest.json').write_text(json.dumps(self.manifest))
        args = argparse.Namespace(workspace=self.workspace, analyze_headless=self.launcher,
                                  output=None, apply=False, max=0, tier='padding')

        def fake_ghidra(command, check):
            self.assertTrue(check)
            self.assertIn('-noanalysis', command)
            self.assertIn('-readOnly', command)
            self.assertEqual(command[-4:], [str(self.workspace / 'call-targets.tsv'),
                                            'report', '0', 'padding'])
            self.assertIn('EF2000RecoverCalls.java', command)
            (self.workspace / 'call-targets.tsv').write_text(
                'entry\tcall_sites\tconfidence\tresult\n'
                '0x000045a4\t0x000047da\tcall_padding_and_prologue\tproposed\n')

        with patch.object(ghidra.subprocess, 'run', side_effect=fake_ghidra):
            ghidra.recover_calls(args)

    def test_recover_calls_requires_apply_for_bounded_changes(self):
        args = argparse.Namespace(workspace=self.workspace, analyze_headless=self.launcher,
                                  output=None, apply=False, max=2, tier='padding')
        with patch.object(ghidra.subprocess, 'run') as run:
            with self.assertRaisesRegex(ValueError, '--max requires --apply'):
                ghidra.recover_calls(args)
            run.assert_not_called()

    def test_recover_calls_apply_and_fresh_report(self):
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        (self.workspace / 'payload.bin').write_bytes(self.image)
        (self.workspace / 'manifest.json').write_text(json.dumps(self.manifest))
        args = argparse.Namespace(workspace=self.workspace, analyze_headless=self.launcher,
                                  output=None, apply=True, max=2, tier='adjacent-ret')

        def fake_ghidra(command, check):
            self.assertTrue(check)
            self.assertNotIn('-readOnly', command)
            self.assertEqual(command[-4:], [str(self.workspace / 'call-targets.tsv'),
                                            'apply', '2', 'adjacent-ret'])
            (self.workspace / 'call-targets.tsv').write_text(
                'entry\tcall_sites\tconfidence\tresult\n'
                '0x000045a4\t0x000047da\tcall_ret_and_prologue\tcreated\n')

        with patch.object(ghidra.subprocess, 'run', side_effect=fake_ghidra):
            ghidra.recover_calls(args)
        with patch.object(ghidra.subprocess, 'run') as run:
            with self.assertRaisesRegex(ValueError, 'without a fresh direct-call recovery report'):
                ghidra.recover_calls(args)
            run.assert_called_once()

    def test_recover_calls_rejects_invalid_report(self):
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        (self.workspace / 'payload.bin').write_bytes(self.image)
        (self.workspace / 'manifest.json').write_text(json.dumps(self.manifest))
        args = argparse.Namespace(workspace=self.workspace, analyze_headless=self.launcher,
                                  output=None, apply=True, max=0, tier='padding')

        def fake_ghidra(command, check):
            (self.workspace / 'call-targets.tsv').write_text('bad\tcolumns\nfoo\tbar\n')

        with patch.object(ghidra.subprocess, 'run', side_effect=fake_ghidra):
            with self.assertRaisesRegex(ValueError, 'invalid direct-call recovery report'):
                ghidra.recover_calls(args)

    def test_recover_calls_accepts_fully_resolved_call_graph(self):
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        (self.workspace / 'payload.bin').write_bytes(self.image)
        (self.workspace / 'manifest.json').write_text(json.dumps(self.manifest))
        args = argparse.Namespace(workspace=self.workspace, analyze_headless=self.launcher,
                                  output=None, apply=False, max=0, tier='padding')

        def fake_ghidra(command, check):
            (self.workspace / 'call-targets.tsv').write_text(
                'entry\tcall_sites\tconfidence\tresult\n')

        with patch.object(ghidra.subprocess, 'run', side_effect=fake_ghidra):
            ghidra.recover_calls(args)

    def test_audit_flow_gaps_is_read_only_and_accepts_no_candidates(self):
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        (self.workspace / 'payload.bin').write_bytes(self.image)
        (self.workspace / 'manifest.json').write_text(json.dumps(self.manifest))
        args = argparse.Namespace(workspace=self.workspace,
                                  analyze_headless=self.launcher, output=None)

        def fake_ghidra(command, check):
            self.assertTrue(check)
            self.assertIn('-noanalysis', command)
            self.assertIn('-readOnly', command)
            self.assertEqual(command[-2:], ['EF2000AuditFlowGaps.java',
                                            str(self.workspace / 'flow-gaps.tsv')])
            (self.workspace / 'flow-gaps.tsv').write_text(
                'function_entry\tfallthrough\tnext_orphan\tgap_bytes\n')

        with patch.object(ghidra.subprocess, 'run', side_effect=fake_ghidra):
            ghidra.audit_flow_gaps(args)

    def test_audit_flow_gaps_requires_fresh_valid_report(self):
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        (self.workspace / 'payload.bin').write_bytes(self.image)
        (self.workspace / 'manifest.json').write_text(json.dumps(self.manifest))
        args = argparse.Namespace(workspace=self.workspace,
                                  analyze_headless=self.launcher, output=None)
        with patch.object(ghidra.subprocess, 'run'):
            with self.assertRaisesRegex(ValueError, 'without a fresh flow-gap report'):
                ghidra.audit_flow_gaps(args)
        with patch.object(ghidra.subprocess, 'run',
                          side_effect=lambda command, check: (self.workspace / 'flow-gaps.tsv')
                              .write_text('bad\tcolumns\n')):
            with self.assertRaisesRegex(ValueError, 'invalid flow-gap report'):
                ghidra.audit_flow_gaps(args)

    def test_audit_body_holes_is_read_only_and_accepts_empty_report(self):
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        (self.workspace / 'payload.bin').write_bytes(self.image)
        (self.workspace / 'manifest.json').write_text(json.dumps(self.manifest))
        args = argparse.Namespace(workspace=self.workspace, analyze_headless=self.launcher,
                                  output=None, offsets=[0x9820])

        def fake_ghidra(command, check):
            self.assertTrue(check)
            self.assertIn('-readOnly', command)
            self.assertEqual(command[-3:], ['EF2000AuditBodyHoles.java',
                                            str(self.workspace / 'body-holes.tsv'), '9820'])
            (self.workspace / 'body-holes.tsv').write_text(
                'entry\tfrom\tthrough\tundefined_bytes\tdata_bytes\t'
                'other_function_bytes\treferences\n')

        with patch.object(ghidra.subprocess, 'run', side_effect=fake_ghidra):
            ghidra.audit_body_holes(args)

    def test_audit_body_holes_requires_fresh_valid_report(self):
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        (self.workspace / 'payload.bin').write_bytes(self.image)
        (self.workspace / 'manifest.json').write_text(json.dumps(self.manifest))
        args = argparse.Namespace(workspace=self.workspace, analyze_headless=self.launcher,
                                  output=None, offsets=[])
        with patch.object(ghidra.subprocess, 'run'):
            with self.assertRaisesRegex(ValueError, 'without a fresh body-hole report'):
                ghidra.audit_body_holes(args)
        with patch.object(ghidra.subprocess, 'run',
                          side_effect=lambda command, check: (self.workspace / 'body-holes.tsv')
                              .write_text('bad\tcolumns\n')):
            with self.assertRaisesRegex(ValueError, 'invalid body-hole report'):
                ghidra.audit_body_holes(args)

    def test_function_facts_read_only_and_checks_all_four_reports(self):
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        (self.workspace / 'payload.bin').write_bytes(self.image)
        (self.workspace / 'manifest.json').write_text(json.dumps(self.manifest))
        args = argparse.Namespace(workspace=self.workspace, analyze_headless=self.launcher,
                                  output=None)

        def fake_ghidra(command, check):
            self.assertTrue(check)
            self.assertIn('-readOnly', command)
            self.assertIn('-noanalysis', command)
            self.assertEqual(command[-2:], ['EF2000FunctionFacts.java',
                             str(self.workspace / 'subsystem-facts')])
            output = self.workspace / 'subsystem-facts'
            output.mkdir(exist_ok=True)
            for name, columns in (
                ('function-facts.tsv', ghidra.ghidra_subsystems.FACT_COLUMNS),
                ('data-references.tsv', ghidra.ghidra_subsystems.REF_COLUMNS),
                ('address-candidates.tsv', ghidra.ghidra_subsystems.CANDIDATE_COLUMNS),
                ('cross-function-branches.tsv', ghidra.ghidra_subsystems.BRANCH_COLUMNS),
            ):
                header = sorted(columns)
                values = {column: '0' for column in columns}
                values.update(entry='0x00001000', name='fixture')
                rows = ['\t'.join(header)]
                if name == 'function-facts.tsv':
                    rows.append('\t'.join(values[column] for column in header))
                (output / name).write_text('\n'.join(rows) + '\n')

        with patch.object(ghidra.subprocess, 'run', side_effect=fake_ghidra):
            ghidra.function_facts(args)
        (self.workspace / 'subsystem-facts/address-candidates.tsv').unlink()
        with patch.object(ghidra.subprocess, 'run'):
            with self.assertRaisesRegex(ValueError, 'without a fresh function-facts.tsv'):
                ghidra.function_facts(args)

    def test_function_facts_rejects_invalid_schema(self):
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        (self.workspace / 'payload.bin').write_bytes(self.image)
        (self.workspace / 'manifest.json').write_text(json.dumps(self.manifest))
        args = argparse.Namespace(workspace=self.workspace, analyze_headless=self.launcher,
                                  output=None)
        output = self.workspace / 'subsystem-facts'
        with patch.object(ghidra.subprocess, 'run', side_effect=lambda command, check: (
                output / 'function-facts.tsv').write_text('incorrect\tcolumns\n')):
            with self.assertRaisesRegex(ValueError, 'invalid columns'):
                ghidra.function_facts(args)

    def test_repair_boundaries_is_read_only_by_default(self):
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        (self.workspace / 'payload.bin').write_bytes(self.image)
        (self.workspace / 'manifest.json').write_text(json.dumps(self.manifest))
        args = argparse.Namespace(workspace=self.workspace,
                                  analyze_headless=self.launcher, output=None, apply=False)

        def fake_ghidra(command, check):
            self.assertTrue(check)
            self.assertIn('-readOnly', command)
            self.assertEqual(command[-3:], ['EF2000RepairBoundaries.java',
                                            str(self.workspace / 'boundary-repairs.tsv'), 'report'])
            (self.workspace / 'boundary-repairs.tsv').write_text(
                'entry\tbody_bytes_before\tbody_bytes_after\tresult\n'
                '0x00004050\t34\t34\tproposed\n'
                '0x0000433c\t9\t9\tproposed\n'
                '0x000045a4\t401\t401\talready_repaired\n'
                '0x00004738\t199\t199\tproposed\n'
                '0x00004aa0\t144\t144\tproposed\n'
                '0x00004bf8\t60\t60\tproposed\n'
                '0x00004e14\t233\t233\tproposed\n'
                '0x00009820\t1021\t1021\tproposed\n'
                '0x0000fecc\t217\t217\tproposed\n')

        with patch.object(ghidra.subprocess, 'run', side_effect=fake_ghidra):
            ghidra.repair_boundaries(args)

    def test_repair_boundaries_applies_explicitly_and_reports_failure(self):
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        (self.workspace / 'payload.bin').write_bytes(self.image)
        (self.workspace / 'manifest.json').write_text(json.dumps(self.manifest))
        args = argparse.Namespace(workspace=self.workspace,
                                  analyze_headless=self.launcher, output=None, apply=True)

        def fake_ghidra(command, check):
            self.assertTrue(check)
            self.assertNotIn('-readOnly', command)
            self.assertEqual(command[-1], 'apply')
            (self.workspace / 'boundary-repairs.tsv').write_text(
                'entry\tbody_bytes_before\tbody_bytes_after\tresult\terror\n'
                '0x00004050\t34\t138\trepaired\t\n'
                '0x0000433c\t9\t242\trepaired\t\n'
                '0x000045a4\t401\t401\talready_repaired\t\n'
                '0x00004738\t199\t321\trepaired\t\n'
                '0x00004aa0\t144\t299\trepaired\t\n'
                '0x00004bf8\t60\t87\trepaired\t\n'
                '0x00004e14\t233\t260\trepaired\t\n'
                '0x00009820\t1021\t1560\trepaired\t\n'
                '0x0000fecc\t217\t231\trepaired\t\n')

        with patch.object(ghidra.subprocess, 'run', side_effect=fake_ghidra):
            ghidra.repair_boundaries(args)

        def failed_ghidra(command, check):
            (self.workspace / 'boundary-repairs.tsv').write_text(
                'entry\tbody_bytes_before\tbody_bytes_after\tresult\terror\n'
                '0x00004050\t34\t34\trepair_failed\tconflicting code\n'
                '0x0000433c\t9\t9\tproposed\t\n'
                '0x000045a4\t401\t401\talready_repaired\t\n'
                '0x00004738\t199\t199\tproposed\t\n'
                '0x00004aa0\t144\t144\tproposed\t\n'
                '0x00004bf8\t60\t60\tproposed\t\n'
                '0x00004e14\t233\t233\tproposed\t\n'
                '0x00009820\t1021\t1021\tproposed\t\n'
                '0x0000fecc\t217\t217\tproposed\t\n')

        with patch.object(ghidra.subprocess, 'run', side_effect=failed_ghidra):
            with self.assertRaisesRegex(ValueError, 'conflicting code'):
                ghidra.repair_boundaries(args)

        def incomplete_ghidra(command, check):
            (self.workspace / 'boundary-repairs.tsv').write_text(
                'entry\tbody_bytes_before\tbody_bytes_after\tresult\n'
                '0x000045a4\t45\t401\trepaired\n')

        with patch.object(ghidra.subprocess, 'run', side_effect=incomplete_ghidra):
            with self.assertRaisesRegex(ValueError, 'unexpected number'):
                ghidra.repair_boundaries(args)

    def test_callback_tables_requires_inventory_and_writes_verified_names(self):
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        self.workspace.joinpath('payload.bin').write_bytes(self.image)
        self.workspace.joinpath('manifest.json').write_text(json.dumps(self.manifest))
        export = self.workspace / 'full-export'
        export.mkdir()
        (export / 'functions.tsv').write_text('entry\tname\n0x00003000\tFUN_00003000\n'
                                              '0x00003020\tFUN_00003020\n')
        # Unlike the real image, our synthetic fixture needs a string area.
        image = bytearray(self.image)
        for index, name in enumerate(('ClickStart', 'ClickStop', 'ClickPause', 'ClickNext')):
            address = 0x170100 + index * 24
            image[address:address + len(name) + 1] = name.encode() + b'\0'
            struct.pack_into('<II', image, 0x120100 + index * 8,
                             0x3000 + index * 0x20, address)
        self.workspace.joinpath('payload.bin').write_bytes(image)
        self.manifest['protected_image']['payload_sha256'] = hashlib.sha256(image).hexdigest()
        self.workspace.joinpath('manifest.json').write_text(json.dumps(self.manifest))
        args = argparse.Namespace(workspace=self.workspace, functions=None, output=None)
        ghidra.callback_tables(args)
        output = self.workspace.joinpath('callback-tables.tsv').read_text()
        self.assertIn('ClickStart\tyes', output)
        self.assertIn('ClickNext\tno', output)

    def test_name_callbacks_reports_by_default_and_checks_fresh_output(self):
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        self.workspace.joinpath('payload.bin').write_bytes(self.image)
        self.workspace.joinpath('manifest.json').write_text(json.dumps(self.manifest))
        (self.workspace / 'callback-tables.tsv').write_text('test evidence')
        args = argparse.Namespace(workspace=self.workspace, analyze_headless=self.launcher,
                                  output=None, apply=False, promote_thunks=False,
                                  promote_entries=False, max=0)

        def fake_ghidra(command, check):
            self.assertTrue(check)
            self.assertIn('EF2000NameCallbacks.java', command)
            self.assertEqual(command[-3:], ['report', 'names', '0'])
            (self.workspace / 'callback-names.tsv').write_text(
                'handler\tname\tresult\n0x00003000\tClickStart\tproposed\n')

        with patch.object(ghidra.subprocess, 'run', side_effect=fake_ghidra):
            ghidra.name_callbacks(args)
        args.apply = True
        with patch.object(ghidra.subprocess, 'run') as run:
            with self.assertRaisesRegex(ValueError, 'without a fresh callback-naming report'):
                ghidra.name_callbacks(args)
            self.assertEqual(run.call_args.args[0][-3:], ['apply', 'names', '0'])

    def test_name_callbacks_requires_apply_to_create_jump_stubs(self):
        args = argparse.Namespace(workspace=self.workspace, analyze_headless=self.launcher,
                                  output=None, apply=False, promote_thunks=True,
                                  promote_entries=False, max=0)
        with patch.object(ghidra.subprocess, 'run') as run:
            with self.assertRaisesRegex(ValueError, '--promote-thunks requires --apply'):
                ghidra.name_callbacks(args)
            run.assert_not_called()

    def test_name_callbacks_requires_apply_to_promote_entries_and_bounds(self):
        args = argparse.Namespace(workspace=self.workspace, analyze_headless=self.launcher,
                                  output=None, apply=False, promote_thunks=False,
                                  promote_entries=True, max=10)
        with patch.object(ghidra.subprocess, 'run') as run:
            with self.assertRaisesRegex(ValueError, '--promote-entries requires --apply'):
                ghidra.name_callbacks(args)
            run.assert_not_called()
        args.apply = True
        args.promote_thunks = True
        with self.assertRaisesRegex(ValueError, 'choose only one'):
            ghidra.name_callbacks(args)
        args.promote_thunks = False
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        self.workspace.joinpath('payload.bin').write_bytes(self.image)
        self.workspace.joinpath('manifest.json').write_text(json.dumps(self.manifest))
        (self.workspace / 'callback-tables.tsv').write_text('test evidence')

        def fake_ghidra(command, check):
            self.assertTrue(check)
            self.assertEqual(command[-3:], ['apply', 'entries', '10'])
            (self.workspace / 'callback-names.tsv').write_text(
                'handler\tname\tresult\n0x00003000\tClickStart\tcreated_named_entry\n')

        with patch.object(ghidra.subprocess, 'run', side_effect=fake_ghidra):
            ghidra.name_callbacks(args)

    def test_name_callbacks_guards_undisassembled_callback_recovery(self):
        args = argparse.Namespace(workspace=self.workspace, analyze_headless=self.launcher,
                                  output=None, apply=False, promote_thunks=False,
                                  promote_entries=False, promote_undisassembled=True, max=3)
        with patch.object(ghidra.subprocess, 'run') as run:
            with self.assertRaisesRegex(ValueError, '--promote-undisassembled requires --apply'):
                ghidra.name_callbacks(args)
            run.assert_not_called()
        args.apply = True
        args.promote_entries = True
        with self.assertRaisesRegex(ValueError, 'choose only one'):
            ghidra.name_callbacks(args)
        args.promote_entries = False
        (self.workspace / 'projects').mkdir(parents=True)
        (self.workspace / 'projects/EF2000.gpr').write_bytes(b'project')
        (self.workspace / 'payload.bin').write_bytes(self.image)
        (self.workspace / 'manifest.json').write_text(json.dumps(self.manifest))
        (self.workspace / 'callback-tables.tsv').write_text('test evidence')

        def fake_ghidra(command, check):
            self.assertTrue(check)
            self.assertEqual(command[-3:], ['apply', 'undisassembled', '3'])
            (self.workspace / 'callback-names.tsv').write_text(
                'handler\tname\tresult\n'
                '0x00004d4c\tgpwAcceptHiscore\tcreated_named_undisassembled\n')

        with patch.object(ghidra.subprocess, 'run', side_effect=fake_ghidra):
            ghidra.name_callbacks(args)

    def test_guest_offsets_are_within_initialized_image(self):
        self.assertEqual(ghidra.guest_offset('0xe9a24'), 0xe9a24)
        for text in ('-1', '0x195fb0', 'not-an-address'):
            with self.assertRaises(argparse.ArgumentTypeError):
                ghidra.guest_offset(text)

    def test_export_limit_is_nonnegative(self):
        self.assertEqual(ghidra.nonnegative_int('0'), 0)
        self.assertEqual(ghidra.nonnegative_int('42'), 42)
        for text in ('-1', 'ten'):
            with self.assertRaises(argparse.ArgumentTypeError):
                ghidra.nonnegative_int(text)

    def test_nix_ghidra_home_finds_headless_launcher(self):
        home = Path(self.temp.name) / 'nix-store-ghidra/lib/ghidra'
        launcher = home / 'support/analyzeHeadless'
        launcher.parent.mkdir(parents=True)
        launcher.write_text('#!/bin/sh\n')
        with patch.dict(ghidra.os.environ, {'GHIDRA_HOME': str(home)}):
            self.assertEqual(ghidra.headless_path(None), str(launcher))


if __name__ == '__main__':
    unittest.main()
