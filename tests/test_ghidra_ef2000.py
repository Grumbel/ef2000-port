# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later

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
        self.launcher = '/bin/true'

    def import_args(self):
        return argparse.Namespace(executable=self.executable,
                                  workspace=self.workspace,
                                  analyze_headless=self.launcher)

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
            output = self.workspace / 'full-export'
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
            ['decompile', '4', 'overwrite', 'watcom', 'none'],
            ['decompile', '4', 'resume', 'watcom', 'storage'],
        ])

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
