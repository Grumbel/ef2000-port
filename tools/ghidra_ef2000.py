#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Import the pinned EF2000 X-32 image into Ghidra and decompile guest offsets.

All proprietary bytes and Ghidra project files stay in the ignored local/
directory by default. Ghidra is supplied by the user, not vendored here.
"""

import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

try:
    from .extract_image import extract
    from .ef2000_callbacks import find_tables, select_function_tables
    from . import ghidra_subsystems
except ImportError:
    from extract_image import extract
    from ef2000_callbacks import find_tables, select_function_tables
    import ghidra_subsystems


ROOT = Path(__file__).resolve().parent.parent
SCRIPTS = ROOT / 'tools' / 'ghidra'
PROJECT_NAME = 'EF2000'
IMAGE_NAME = 'payload.bin'
IMAGE_SIZE = 0x195fb0


def headless_path(option):
    if option:
        candidate = Path(option).expanduser()
    elif os.environ.get('GHIDRA_HOME'):
        candidate = Path(os.environ['GHIDRA_HOME']) / 'support' / 'analyzeHeadless'
    else:
        resolved = shutil.which('analyzeHeadless')
        if not resolved:
            raise ValueError('set --analyze-headless or GHIDRA_HOME (Ghidra is not on PATH)')
        candidate = Path(resolved)
    if candidate.is_dir():
        candidate /= 'support/analyzeHeadless'
    if not candidate.is_file():
        raise ValueError(f'Ghidra headless launcher not found: {candidate}')
    return str(candidate.resolve())


def guest_offset(text):
    try:
        value = int(text, 0)
    except ValueError as error:
        raise argparse.ArgumentTypeError(f'invalid guest offset: {text}') from error
    if not 0 <= value < IMAGE_SIZE:
        raise argparse.ArgumentTypeError(f'guest offset outside initialized image: {text}')
    return value


def nonnegative_int(text):
    try:
        value = int(text)
    except ValueError as error:
        raise argparse.ArgumentTypeError('expected a nonnegative function count') from error
    if value < 0:
        raise argparse.ArgumentTypeError('expected a nonnegative function count')
    return value


def project_args(workspace, launcher):
    return [launcher, str(workspace / 'projects'), PROJECT_NAME,
            '-scriptPath', str(SCRIPTS)]


def import_image(args):
    # The extractor checks the exact original executable fingerprint. Never
    # import the MZ stub as a flat 32-bit program: its offsets do not match the
    # addresses emitted by Unicorn's block profiler.
    manifest, images = extract(args.executable.read_bytes())
    payload = images[IMAGE_NAME]
    if len(payload) != IMAGE_SIZE:
        raise ValueError('unexpected EF2000 initialized image length')
    workspace = args.workspace.resolve()
    project_dir = workspace / 'projects'
    if (project_dir / (PROJECT_NAME + '.gpr')).exists():
        raise ValueError('Ghidra project already exists; use decompile to reuse it')
    image_path = workspace / IMAGE_NAME
    manifest_path = workspace / 'manifest.json'
    if image_path.exists() and image_path.read_bytes() != payload:
        raise ValueError(f'refusing to replace a different image: {image_path}')
    if manifest_path.exists() and json.loads(manifest_path.read_text()) != manifest:
        raise ValueError(f'refusing to replace a different manifest: {manifest_path}')
    launcher = headless_path(args.analyze_headless)
    project_dir.mkdir(parents=True, exist_ok=True)
    if not image_path.exists():
        image_path.write_bytes(payload)
    if not manifest_path.exists():
        manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
    command = project_args(workspace, launcher) + [
        '-import', str(image_path),
        '-loader', 'BinaryLoader', '-loader-baseAddr', '0',
        '-loader-blockName', 'X32_IMAGE',
        '-processor', 'x86:LE:32:default', '-cspec', 'windows',
        '-preScript', 'EF2000Seed.java',
    ]
    subprocess.run(command, check=True)
    project_path = project_dir / (PROJECT_NAME + '.gpr')
    if not project_path.is_file():
        raise ValueError(f'Ghidra returned without creating its project: {project_path}')
    print(f'Ghidra project: {project_path}')


def decompile(args):
    workspace = validated_workspace(args.workspace)
    launcher = headless_path(args.analyze_headless)
    output = (args.output or workspace / 'decompiled').resolve()
    output.mkdir(parents=True, exist_ok=True)
    previous_times = {
        offset: (output / f'0x{offset:08x}.c').stat().st_mtime_ns
        if (output / f'0x{offset:08x}.c').exists() else -1
        for offset in args.offsets
    }
    command = project_args(workspace, launcher) + [
        '-process', IMAGE_NAME, '-noanalysis',
        '-postScript', 'EF2000Decompile.java', str(output),
        *(f'{offset:x}' for offset in args.offsets),
    ]
    subprocess.run(command, check=True)
    missing = [offset for offset in args.offsets
               if not (output / f'0x{offset:08x}.c').is_file() or
               (output / f'0x{offset:08x}.c').stat().st_mtime_ns <= previous_times[offset]]
    if missing:
        raise ValueError('Ghidra returned without fresh decompilation for: ' +
                         ', '.join(f'0x{offset:x}' for offset in missing))
    print(f'Decompilation: {output}')


def validated_workspace(path):
    workspace = path.resolve()
    manifest_path = workspace / 'manifest.json'
    image_path = workspace / IMAGE_NAME
    project_path = workspace / 'projects' / (PROJECT_NAME + '.gpr')
    if not manifest_path.is_file() or not image_path.is_file() or not project_path.is_file():
        raise ValueError('no imported project; run the import command first')
    manifest = json.loads(manifest_path.read_text())
    if (manifest['protected_image']['initialized_range'] != [0, IMAGE_SIZE] or
            manifest['protected_image']['payload_sha256'] !=
            hashlib.sha256(image_path.read_bytes()).hexdigest()):
        raise ValueError('imported image does not match its extraction manifest')
    return workspace


def copy_callers(args):
    """Export direct copy callsites with their post-CALL guest return IPs."""
    workspace = validated_workspace(args.workspace)
    output = (args.output or workspace / 'copy-callers.tsv').resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    previous_time = output.stat().st_mtime_ns if output.exists() else -1
    command = project_args(workspace, headless_path(args.analyze_headless)) + [
        '-process', IMAGE_NAME, '-noanalysis',
        '-postScript', 'EF2000CopyCallers.java', str(output),
    ]
    subprocess.run(command, check=True)
    if not output.is_file() or output.stat().st_mtime_ns <= previous_time:
        raise ValueError('Ghidra returned without a fresh copy-caller map')
    print(f'Copy callers: {output}')


def export_all(args):
    """Inventory all recovered functions or export their complete pseudo-C corpus."""
    if getattr(args, 'fastcall_fallback', False) and getattr(args, 'watcom_fallback', False):
        raise ValueError('choose only one decompiler fallback model')
    workspace = validated_workspace(args.workspace)
    output = (args.output or workspace / 'full-export').resolve()
    output.mkdir(parents=True, exist_ok=True)
    files = ['report.json', 'functions.tsv', 'calls.tsv', 'candidate-entries.tsv',
             'orphan-ranges.tsv', 'orphan-instructions.asm']
    if getattr(args, 'custom_storage_fallback', False) and getattr(args, 'overwrite', False):
        # Run BOTH passes in an empty directory. Reusing the old export tree
        # can resurrect stale custom-storage C, making a "complete" corpus
        # silently mix old and new decompilations. Publish only once the
        # isolated export has passed its ordinary completeness checks.
        with tempfile.TemporaryDirectory(prefix='ef2000-ghidra-overwrite-') as temporary:
            staged_output = Path(temporary) / 'full-export'
            staged_args = argparse.Namespace(**vars(args))
            staged_args.output = staged_output
            staged_args.overwrite = False
            export_all(staged_args)
            shutil.copytree(staged_output / 'functions', output / 'functions',
                            dirs_exist_ok=True)
            for name in files:
                shutil.copy2(staged_output / name, output / name)
            # A failed fresh decompilation must not leave an older C file
            # masquerading as current output. The .asm remains available.
            with (staged_output / 'functions.tsv').open(newline='') as stream:
                for row in csv.DictReader(stream, delimiter='\t'):
                    if row['status'] != 'failed':
                        continue
                    entry = int(row['entry'], 0)
                    if not 0 <= entry < IMAGE_SIZE:
                        raise ValueError(f'invalid failed function entry: {row["entry"]}')
                    stale = output / 'functions' / f'{entry >> 16:04x}' / f'0x{entry:08x}.c'
                    stale.unlink(missing_ok=True)
        print(f'Ghidra overwrite: published fresh two-pass corpus to {output}')
        return
    previous_times = {name: (output / name).stat().st_mtime_ns
                      if (output / name).exists() else -1 for name in files}
    mode = 'inventory' if args.command == 'inventory' else 'decompile'
    command = project_args(workspace, headless_path(args.analyze_headless)) + [
        '-process', IMAGE_NAME, '-noanalysis',
        *(['-readOnly'] if getattr(args, 'custom_storage_fallback', False) else []),
        '-postScript', 'EF2000ExportAll.java', str(output), mode,
        str(getattr(args, 'limit', 0)),
        'overwrite' if getattr(args, 'overwrite', False) else 'resume',
        'watcom' if getattr(args, 'watcom_fallback', False) else
        'fastcall' if getattr(args, 'fastcall_fallback', False) else 'none',
        'storage' if getattr(args, 'custom_storage_fallback', False) else 'none',
    ]
    if getattr(args, 'custom_storage_fallback', False):
        # Temporary signature changes can affect how Ghidra renders callers
        # later in the SAME headless process, even after restoring the
        # function. Export ordinary C first, then retry only failures in a
        # separate read-only process while reusing the already-written C.
        first_pass = command.copy()
        first_pass[-1] = 'none'
        subprocess.run(first_pass, check=True)
        command[-3] = 'resume'
    subprocess.run(command, check=True)
    for name in files:
        file = output / name
        if not file.is_file() or file.stat().st_mtime_ns <= previous_times[name]:
            raise ValueError(f'Ghidra returned without a fresh {name}')
    report = json.loads((output / 'report.json').read_text())
    if report['initialized_image_bytes'] != IMAGE_SIZE or report['discovered_functions'] < 1:
        raise ValueError('Ghidra returned an invalid full-program report')
    print(f'Ghidra {mode}: {report["discovered_functions"]} functions; '
          f'{report["decompilations_written"]} written, '
          f'{report["decompilations_reused"]} reused, '
          f'{report["decompilations_failed"]} failed, '
          f'{report["decompilations_limited"]} limited; output: {output}')
    return report


def recover_orphans(args):
    """Review or explicitly promote referenced orphan prologues to functions."""
    if args.max and not args.apply:
        raise ValueError('--max requires --apply')
    workspace = validated_workspace(args.workspace)
    output = (args.output or workspace / 'recovery-candidates.tsv').resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    previous_time = output.stat().st_mtime_ns if output.exists() else -1
    command = project_args(workspace, headless_path(args.analyze_headless)) + [
        '-process', IMAGE_NAME, '-noanalysis',
        '-postScript', 'EF2000RecoverOrphans.java', str(output),
        'apply' if args.apply else 'report', str(args.max), args.tier,
    ]
    subprocess.run(command, check=True)
    if not output.is_file() or output.stat().st_mtime_ns <= previous_time:
        raise ValueError('Ghidra returned without a fresh orphan-recovery report')
    with output.open(newline='') as stream:
        rows = list(csv.DictReader(stream, delimiter='\t'))
    if not rows or not {'entry', 'confidence', 'result'} <= set(rows[0]):
        raise ValueError('Ghidra returned an invalid orphan-recovery report')
    strong = sum(row['confidence'] in ('strong_reference_and_prologue',
                                     'aligned_table_reference_and_push') for row in rows)
    created = sum(row['result'] == 'created' for row in rows)
    failed = sum(row['result'] == 'create_failed' for row in rows)
    print(f'Ghidra orphan recovery: {len(rows)} candidates, {strong} reference/prologue '
          f'matches, {created} created, {failed} failed; output: {output}')
    return created


def recover_calls(args):
    """Audit or narrowly disassemble missing direct CALL targets."""
    if args.max and not args.apply:
        raise ValueError('--max requires --apply')
    workspace = validated_workspace(args.workspace)
    output = (args.output or workspace / 'call-targets.tsv').resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    previous_time = output.stat().st_mtime_ns if output.exists() else -1
    command = project_args(workspace, headless_path(args.analyze_headless)) + [
        '-process', IMAGE_NAME, '-noanalysis',
        *([] if args.apply else ['-readOnly']),
        '-postScript', 'EF2000RecoverCalls.java', str(output),
        'apply' if args.apply else 'report', str(args.max), args.tier,
    ]
    subprocess.run(command, check=True)
    if not output.is_file() or output.stat().st_mtime_ns <= previous_time:
        raise ValueError('Ghidra returned without a fresh direct-call recovery report')
    with output.open(newline='') as stream:
        reader = csv.DictReader(stream, delimiter='\t')
        columns = set(reader.fieldnames or ())
        rows = list(reader)
    if not {'entry', 'call_sites', 'confidence', 'result'} <= columns:
        raise ValueError('Ghidra returned an invalid direct-call recovery report')
    candidates = sum(row['confidence'] in ('call_padding_and_prologue',
                                         'call_ret_and_prologue') for row in rows)
    created = sum(row['result'] == 'created' for row in rows)
    failed = sum(row['result'] in ('disassemble_failed', 'create_failed') for row in rows)
    print(f'Ghidra direct-call recovery: {len(rows)} unresolved targets, '
          f'{candidates} guarded candidates, {created} created, '
          f'{failed} failed; output: {output}')
    return created


def repair_boundaries(args):
    """Audit or explicitly repair known instruction overlaps blocking a function body."""
    workspace = validated_workspace(args.workspace)
    output = (args.output or workspace / 'boundary-repairs.tsv').resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    previous_time = output.stat().st_mtime_ns if output.exists() else -1
    command = project_args(workspace, headless_path(args.analyze_headless)) + [
        '-process', IMAGE_NAME, '-noanalysis',
        *([] if args.apply else ['-readOnly']),
        '-postScript', 'EF2000RepairBoundaries.java', str(output),
        'apply' if args.apply else 'report',
    ]
    subprocess.run(command, check=True)
    if not output.is_file() or output.stat().st_mtime_ns <= previous_time:
        raise ValueError('Ghidra returned without a fresh boundary-repair report')
    with output.open(newline='') as stream:
        reader = csv.DictReader(stream, delimiter='\t')
        if not {'entry', 'body_bytes_before', 'body_bytes_after', 'result'} <= \
                set(reader.fieldnames or ()):
            raise ValueError('Ghidra returned an invalid boundary-repair report')
        rows = list(reader)
    if len(rows) != 9 or {row['entry'] for row in rows} != {
            '0x00004050', '0x0000433c', '0x000045a4', '0x00004738',
            '0x00004aa0', '0x00004bf8', '0x00004e14', '0x00009820',
            '0x0000fecc'}:
        raise ValueError('Ghidra returned an unexpected number of boundary-repair rows')
    failed = [row for row in rows if row['result'] == 'repair_failed']
    if failed:
        raise ValueError('Ghidra boundary repair failed: ' + '; '.join(
            f'{row["entry"]}: {row.get("error", "unknown error")}' for row in failed))
    print('Ghidra boundary repair: ' + ', '.join(
        f'{row["entry"]} {row["result"]} '
        f'({row["body_bytes_before"]} -> {row["body_bytes_after"]} bytes)'
        for row in rows) + f'; output: {output}')
    return rows


def audit_flow_gaps(args):
    """Report short unresolved fall-through gaps without editing the Ghidra project."""
    workspace = validated_workspace(args.workspace)
    output = (args.output or workspace / 'flow-gaps.tsv').resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    previous_time = output.stat().st_mtime_ns if output.exists() else -1
    command = project_args(workspace, headless_path(args.analyze_headless)) + [
        '-process', IMAGE_NAME, '-noanalysis', '-readOnly',
        '-postScript', 'EF2000AuditFlowGaps.java', str(output),
    ]
    subprocess.run(command, check=True)
    if not output.is_file() or output.stat().st_mtime_ns <= previous_time:
        raise ValueError('Ghidra returned without a fresh flow-gap report')
    with output.open(newline='') as stream:
        reader = csv.DictReader(stream, delimiter='\t')
        if not {'function_entry', 'fallthrough', 'next_orphan', 'gap_bytes'} <= \
                set(reader.fieldnames or ()):
            raise ValueError('Ghidra returned an invalid flow-gap report')
        rows = list(reader)
    print(f'Ghidra flow-gap audit: {len(rows)} candidates; output: {output}')


def audit_body_holes(args):
    """Inventory unowned ranges within multi-span functions, including data and references."""
    workspace = validated_workspace(args.workspace)
    output = (args.output or workspace / 'body-holes.tsv').resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    previous_time = output.stat().st_mtime_ns if output.exists() else -1
    command = project_args(workspace, headless_path(args.analyze_headless)) + [
        '-process', IMAGE_NAME, '-noanalysis', '-readOnly',
        '-postScript', 'EF2000AuditBodyHoles.java', str(output),
        *(f'{offset:x}' for offset in args.offsets),
    ]
    subprocess.run(command, check=True)
    if not output.is_file() or output.stat().st_mtime_ns <= previous_time:
        raise ValueError('Ghidra returned without a fresh body-hole report')
    with output.open(newline='') as stream:
        reader = csv.DictReader(stream, delimiter='\t')
        if not {'entry', 'from', 'through', 'undefined_bytes', 'data_bytes',
                'other_function_bytes', 'references'} <= set(reader.fieldnames or ()):
            raise ValueError('Ghidra returned an invalid body-hole report')
        rows = list(reader)
    print(f'Ghidra body-hole audit: {len(rows)} holes; output: {output}')


def function_facts(args):
    """Export read-only instruction-level facts for cross-subsystem analysis."""
    workspace = validated_workspace(args.workspace)
    output = (args.output or workspace / 'subsystem-facts').resolve()
    output.mkdir(parents=True, exist_ok=True)
    required = {
        'function-facts.tsv': ghidra_subsystems.FACT_COLUMNS,
        'data-references.tsv': ghidra_subsystems.REF_COLUMNS,
        'address-candidates.tsv': ghidra_subsystems.CANDIDATE_COLUMNS,
        'cross-function-branches.tsv': ghidra_subsystems.BRANCH_COLUMNS,
    }
    previous_times = {name: (output / name).stat().st_mtime_ns
                      if (output / name).exists() else -1 for name in required}
    command = project_args(workspace, headless_path(args.analyze_headless)) + [
        '-process', IMAGE_NAME, '-noanalysis', '-readOnly',
        '-postScript', 'EF2000FunctionFacts.java', str(output),
    ]
    subprocess.run(command, check=True)
    counts = {}
    for name, columns in required.items():
        path = output / name
        if not path.is_file() or path.stat().st_mtime_ns <= previous_times[name]:
            raise ValueError(f'Ghidra returned without a fresh {name}')
        counts[name] = sum(1 for _ in ghidra_subsystems.rows(path, columns))
    if not counts['function-facts.tsv']:
        raise ValueError('Ghidra returned no function facts')
    report = workspace / 'full-export/report.json'
    if report.is_file():
        expected = json.loads(report.read_text())['discovered_functions']
        if counts['function-facts.tsv'] != expected:
            raise ValueError(f'Ghidra returned facts for {counts["function-facts.tsv"]} '
                             f'of {expected} recognized functions')
    print(f'Ghidra function facts: {counts["function-facts.tsv"]} functions, '
          f'{counts["data-references.tsv"]} data references, '
          f'{counts["address-candidates.tsv"]} literal address candidates, '
          f'{counts["cross-function-branches.tsv"]} cross-function branches; '
          f'output: {output}')


def subsystem_map(args):
    """Rank affinity hypotheses from cached facts, without starting Ghidra."""
    workspace = validated_workspace(args.workspace)
    if args.top < 1 or not 0 < args.group_score < float('inf'):
        raise ValueError('--top and --group-score must be positive and finite')
    output = (args.output or workspace / 'subsystem-map').resolve()
    count, groups = ghidra_subsystems.write_report(
        workspace, output, args.seed, args.top, args.group_score)
    print(f'EF2000 subsystem affinity: {count} functions, {groups} candidate groups; '
          f'output: {output}')


def callback_tables(args):
    """Extract only name tables backed by the Ghidra function inventory."""
    workspace = validated_workspace(args.workspace)
    functions_path = (args.functions or workspace / 'full-export/functions.tsv').resolve()
    with functions_path.open(newline='') as stream:
        inventory = list(csv.DictReader(stream, delimiter='\t'))
    if not inventory or not {'entry', 'name'} <= set(inventory[0]):
        raise ValueError('expected Ghidra functions.tsv; run inventory first')
    function_entries = {int(row['entry'], 16) for row in inventory}
    tables = select_function_tables(find_tables((workspace / IMAGE_NAME).read_bytes()),
                                    function_entries)
    if not tables:
        raise ValueError('no callback tables corroborated by the supplied function inventory')
    output = (args.output or workspace / 'callback-tables.tsv').resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('w', newline='') as stream:
        writer = csv.writer(stream, delimiter='\t', lineterminator='\n')
        writer.writerow(('table_start', 'slot', 'handler', 'string_ptr', 'name', 'known_function'))
        for start, rows in tables:
            for slot, handler, string_ptr, name in rows:
                writer.writerow((f'0x{start:08x}', f'0x{slot:08x}', f'0x{handler:08x}',
                                 f'0x{string_ptr:08x}', name,
                                 'yes' if handler in function_entries else 'no'))
    count = sum(len(rows) for _, rows in tables)
    print(f'Named callback tables: {len(tables)} corroborated tables, {count} named slots; '
          f'output: {output}')


def name_callbacks(args):
    """Audit or explicitly apply unique callback names to existing functions."""
    promote_undisassembled = getattr(args, 'promote_undisassembled', False)
    if args.promote_thunks and not args.apply:
        raise ValueError('--promote-thunks requires --apply')
    if args.promote_entries and not args.apply:
        raise ValueError('--promote-entries requires --apply')
    if promote_undisassembled and not args.apply:
        raise ValueError('--promote-undisassembled requires --apply')
    if sum((args.promote_entries, args.promote_thunks, promote_undisassembled)) > 1:
        raise ValueError('choose only one callback promotion mode')
    if args.max and not (args.apply and (args.promote_thunks or args.promote_entries or
                                    promote_undisassembled)):
        raise ValueError('--max requires --apply and a callback promotion mode')
    workspace = validated_workspace(args.workspace)
    input_path = (workspace / 'callback-tables.tsv').resolve()
    output = (args.output or workspace / 'callback-names.tsv').resolve()
    if not input_path.is_file():
        raise ValueError('no callback-tables.tsv; run callback-tables first')
    # The Ghidra script independently checks every pointer and name against
    # the pinned imported image before applying symbols.
    output.parent.mkdir(parents=True, exist_ok=True)
    previous_time = output.stat().st_mtime_ns if output.exists() else -1
    command = project_args(workspace, headless_path(args.analyze_headless)) + [
        '-process', IMAGE_NAME, '-noanalysis',
        '-postScript', 'EF2000NameCallbacks.java', str(input_path), str(output),
        'apply' if args.apply else 'report',
        'undisassembled' if promote_undisassembled else
        'entries' if args.promote_entries else 'thunks' if args.promote_thunks else 'names',
        str(args.max),
    ]
    subprocess.run(command, check=True)
    if not output.is_file() or output.stat().st_mtime_ns <= previous_time:
        raise ValueError('Ghidra returned without a fresh callback-naming report')
    with output.open(newline='') as stream:
        rows = list(csv.DictReader(stream, delimiter='\t'))
    if not rows or not {'handler', 'name', 'result'} <= set(rows[0]):
        raise ValueError('Ghidra returned an invalid callback-naming report')
    changed = sum(row['result'] in ('renamed', 'created_named_thunk', 'created_named_entry',
                                  'created_named_undisassembled')
                  for row in rows)
    created = sum(row['result'] in ('created_named_thunk', 'created_unnamed_thunk',
                                   'created_named_entry', 'created_unnamed_entry',
                                   'created_named_undisassembled',
                                   'created_unnamed_undisassembled')
                  for row in rows)
    print(f'Ghidra named callbacks: {len(rows)} slots, {changed} names applied, '
          f'{created} new functions; output: {output}')
    return created


# Recorded with Ghidra 11.4.2 across bundles #041-#056; see docs/ghidra.md.
DOCUMENTED_FUNCTIONS = 4277
# Upper bound for repeated direct-call recovery; each pass exposes only the
# calls inside functions created by the previous one.
MAX_CALL_PASSES = 10


def rebuild(args):
    """Recreate the complete recovered Ghidra project in one workspace.

    Runs, in their documented order, every applying recovery pass that
    produced the bundle #056 project. Records counts after every stage in
    workspace/rebuild-report.json. Never touches an existing project.
    """
    workspace = args.workspace.resolve()
    if (workspace / 'projects' / (PROJECT_NAME + '.gpr')).exists():
        raise ValueError(f'a Ghidra project already exists in {workspace}; choose a new '
                         '--workspace (or delete the old one yourself after checking it)')

    def namespace(command, **values):
        base = dict(workspace=workspace, analyze_headless=args.analyze_headless,
                    command=command, output=None, apply=True, max=0)
        base.update(values)
        return argparse.Namespace(**base)

    stages = []
    report_path = workspace / 'rebuild-report.json'

    def record(label, created=None):
        report = export_all(namespace('inventory', apply=False))
        stage = {'stage': label, 'functions': report['discovered_functions']}
        if created is not None:
            stage['created'] = created
        stages.append(stage)
        report_path.write_text(json.dumps({'stages': stages}, indent=2) + '\n')
        print(f'rebuild: {label}: {stage["functions"]} functions'
              + (f' ({created} created)' if created is not None else ''))
        return report

    import_image(argparse.Namespace(workspace=workspace, executable=args.executable,
                                    analyze_headless=args.analyze_headless))
    record('import')
    record('orphans-strict', recover_orphans(namespace('recover-orphans', tier='strict')))
    record('orphans-table', recover_orphans(namespace('recover-orphans', tier='table')))
    # Callback tables are corroborated by the current inventory, so refresh
    # them before each naming pass.
    for label, mode in (('callback-names', None), ('callback-thunks', 'promote_thunks'),
                        ('callback-entries', 'promote_entries'),
                        ('callback-undisassembled', 'promote_undisassembled')):
        callback_tables(namespace('callback-tables', functions=None, apply=False))
        flags = dict(promote_thunks=False, promote_entries=False,
                     promote_undisassembled=False)
        if mode:
            flags[mode] = True
        record(label, name_callbacks(namespace('name-callbacks', **flags)))
    # The adjacent-RET tier is weaker evidence; use it only once the
    # padding tier has converged, then let padding pick up exposed calls.
    for tier in ('padding', 'adjacent-ret', 'padding'):
        for number in range(1, MAX_CALL_PASSES + 1):
            created = recover_calls(namespace('recover-calls', tier=tier))
            record(f'calls-{tier}-{number}', created)
            if not created:
                break
        else:
            raise ValueError(f'direct-call recovery ({tier}) did not converge')
    repairs = repair_boundaries(namespace('repair-boundaries'))
    final = record('repair-boundaries')
    audit = repair_boundaries(namespace('repair-boundaries', apply=False))
    if any(row['result'] != 'already_repaired' for row in audit):
        raise ValueError('boundary recipes are not all applied: ' + ', '.join(
            f'{row["entry"]} {row["result"]}' for row in audit))
    summary = {'stages': stages, 'boundary_repairs': [
        {key: row[key] for key in ('entry', 'result', 'body_bytes_before', 'body_bytes_after')}
        for row in repairs], 'documented_functions': DOCUMENTED_FUNCTIONS,
        'functions': final['discovered_functions']}
    if not args.skip_export:
        exported = export_all(namespace('export-all', apply=False, limit=0, overwrite=False,
                                        fastcall_fallback=False, watcom_fallback=False,
                                        custom_storage_fallback=True))
        summary['decompilations_failed'] = exported['decompilations_failed']
        function_facts(namespace('function-facts', apply=False))
    report_path.write_text(json.dumps(summary, indent=2) + '\n')
    difference = final['discovered_functions'] - DOCUMENTED_FUNCTIONS
    print(f'rebuild: {final["discovered_functions"]} functions '
          f'({difference:+d} against the documented {DOCUMENTED_FUNCTIONS}); '
          f'report: {report_path}')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest='command', required=True)
    # The Nix workflow wrapper runs from a read-only store source tree, so
    # it supplies a writable default under the caller's current directory.
    # Direct source-tree invocations retain the original ignored local/ path.
    default_workspace = Path(os.environ.get('EF2000_GHIDRA_WORKSPACE',
                                            str(ROOT / 'local/ghidra'))).expanduser()
    for name in ('import', 'decompile', 'copy-callers', 'inventory', 'export-all',
                 'recover-orphans', 'recover-calls', 'repair-boundaries', 'audit-flow-gaps',
                 'audit-body-holes', 'function-facts', 'subsystem-map',
                 'callback-tables', 'name-callbacks', 'rebuild'):
        sub = subparsers.add_parser(name)
        sub.add_argument('--workspace', type=Path, default=default_workspace,
                         help='Ghidra project and image workspace (default: EF2000_GHIDRA_WORKSPACE or source local/ghidra)')
        sub.add_argument('--analyze-headless', help='Ghidra launcher or installation directory')
    imp = subparsers.choices['import']
    imp.add_argument('executable', type=Path, help='SHA-pinned original _EF2000.EXE')
    dec = subparsers.choices['decompile']
    dec.add_argument('offsets', nargs='+', type=guest_offset, help='guest offsets, e.g. 0xe9a24')
    dec.add_argument('--output', type=Path, help='decompiled C files (default: workspace/decompiled)')
    callers = subparsers.choices['copy-callers']
    callers.add_argument('--output', type=Path, help='TSV output (default: workspace/copy-callers.tsv)')
    for name in ('inventory', 'export-all'):
        command = subparsers.choices[name]
        command.add_argument('--output', type=Path,
                             help='output directory (default: workspace/full-export)')
    all_export = subparsers.choices['export-all']
    all_export.add_argument('--limit', type=nonnegative_int, default=0,
                            help='export at most N discovered functions (0 means all)')
    all_export.add_argument('--overwrite', action='store_true',
                            help='regenerate cached pseudo-C after changing Ghidra annotations')
    all_export.add_argument('--fastcall-fallback', action='store_true',
                            help='optionally retry failed pseudo-C with an unverified __fastcall evaluation model')
    all_export.add_argument('--watcom-fallback', action='store_true',
                            help='retry failed pseudo-C using the optional experimental EAX/EDX/EBX/ECX model')
    all_export.add_argument('--custom-storage-fallback', action='store_true',
                            help='last-resort decompilation using temporary, unverified register parameters')
    recovery = subparsers.choices['recover-orphans']
    recovery.add_argument('--output', type=Path,
                          help='TSV report (default: workspace/recovery-candidates.tsv)')
    recovery.add_argument('--apply', action='store_true',
                          help='modify the Ghidra project; default only reports candidates')
    recovery.add_argument('--max', type=nonnegative_int, default=0,
                          help='maximum promotions per run (0 means all; only for --apply)')
    recovery.add_argument('--tier', choices=('strict', 'table'), default='strict',
                          help='strict saved-register prologues or opt-in aligned-table volatile-register pushes')
    direct = subparsers.choices['recover-calls']
    direct.add_argument('--output', type=Path,
                        help='TSV report (default: workspace/call-targets.tsv)')
    direct.add_argument('--apply', action='store_true',
                        help='disassemble and create guarded direct-call targets on this project')
    direct.add_argument('--max', type=nonnegative_int, default=0,
                        help='maximum function creations (0 means all; only with --apply)')
    direct.add_argument('--tier', choices=('padding', 'adjacent-ret'), default='padding',
                        help='strong alignment-padding evidence, or opt-in immediate preceding RET byte')
    boundaries = subparsers.choices['repair-boundaries']
    boundaries.add_argument('--output', type=Path,
                            help='TSV audit (default: workspace/boundary-repairs.tsv)')
    boundaries.add_argument('--apply', action='store_true',
                            help='repair only nine pinned, byte-verified boundary recipes on a disposable project')
    gaps = subparsers.choices['audit-flow-gaps']
    gaps.add_argument('--output', type=Path,
                      help='TSV audit (default: workspace/flow-gaps.tsv)')
    holes = subparsers.choices['audit-body-holes']
    holes.add_argument('offsets', nargs='*', type=guest_offset,
                       help='optional function entries, e.g. 0x9820 (default: all multi-span functions)')
    holes.add_argument('--output', type=Path,
                       help='TSV audit (default: workspace/body-holes.tsv)')
    facts = subparsers.choices['function-facts']
    facts.add_argument('--output', type=Path,
                       help='TSV directory (default: workspace/subsystem-facts)')
    subsystem = subparsers.choices['subsystem-map']
    subsystem.add_argument('--output', type=Path,
                           help='ranked reports (default: workspace/subsystem-map)')
    subsystem.add_argument('--seed', type=ghidra_subsystems.guest_address,
                           action='append', default=[],
                           help='repeat for selected guest functions')
    subsystem.add_argument('--top', type=int, default=12,
                           help='maximum ranked neighbors per function (default: 12)')
    subsystem.add_argument('--group-score', type=float, default=8.0,
                           help='minimum pairwise score for candidate groups (default: 8)')
    callback = subparsers.choices['callback-tables']
    callback.add_argument('--functions', type=Path,
                          help='Ghidra functions.tsv (default: workspace/full-export/functions.tsv)')
    callback.add_argument('--output', type=Path,
                          help='output TSV (default: workspace/callback-tables.tsv)')
    names = subparsers.choices['name-callbacks']
    names.add_argument('--output', type=Path,
                       help='audit TSV (default: workspace/callback-names.tsv)')
    names.add_argument('--apply', action='store_true',
                       help='rename existing unambiguous auto-named functions')
    names.add_argument('--promote-thunks', action='store_true',
                       help='with --apply, also create missing 5-byte direct-JMP callback stubs')
    names.add_argument('--promote-entries', action='store_true',
                       help='with --apply, also recover aligned disassembled table-backed entries')
    names.add_argument('--promote-undisassembled', action='store_true',
                       help='with --apply, narrowly decode named table-backed entries lacking instructions')
    names.add_argument('--max', type=nonnegative_int, default=0,
                       help='maximum function creations (0 means all; only with an applying promotion mode)')
    rebuild_parser = subparsers.choices['rebuild']
    rebuild_parser.add_argument('executable', type=Path, help='SHA-pinned original _EF2000.EXE')
    rebuild_parser.add_argument('--skip-export', action='store_true',
                                help='stop after boundary repair; skip pseudo-C and facts export')
    args = parser.parse_args(argv)
    try:
        {'import': import_image, 'decompile': decompile, 'copy-callers': copy_callers,
         'inventory': export_all, 'export-all': export_all,
         'recover-orphans': recover_orphans, 'recover-calls': recover_calls,
         'repair-boundaries': repair_boundaries,
         'audit-flow-gaps': audit_flow_gaps,
         'audit-body-holes': audit_body_holes,
         'function-facts': function_facts, 'subsystem-map': subsystem_map,
         'callback-tables': callback_tables,
         'name-callbacks': name_callbacks, 'rebuild': rebuild}[args.command](args)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'ghidra_ef2000: {error}\n')


if __name__ == '__main__':
    main()
