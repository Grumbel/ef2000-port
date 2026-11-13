#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Link OpenWatcom OMF modules into one EF2000 hybrid guest image.

Hand-written ``compiles+`` bodies take precedence over mechanically generated
``ok`` bodies. Other guest entries keep their original bytes. Compiled module
segments live in a reserved guest arena and eligible entries redirect there.
The proprietary input image and generated output belong under local/.
"""

import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

try:
    from .flirt_match import OMFError
    from .omf_splice import read_module
except ImportError:  # python3 tools/hybrid_link.py from the repository root
    from flirt_match import OMFError
    from omf_splice import read_module


ROOT = Path(__file__).resolve().parent.parent
MAGIC = b'EF2000P1'
HEADER = struct.Struct('<8s32sI')
ENTRY = struct.Struct('<II')
GUEST_SIZE = 0x4000000
DEFAULT_ARENA = 0x3000000
STACK_TOP = 0x3FFF000
BSS_END = 0x4DE2F0
IMAGE_SHA256 = '3fd26b189efe289feb9e6ac701d592a5d48d7c1e152527aa00c01611e87e0fdb'
ABI_HEADER = ROOT / 'decomp/include/ef_abi.h'
MAX_PATCH_BYTES = 16 * 1024 * 1024
MAX_PATCH_WRITES = 4096


def read_tsv(path):
    with path.open(newline='') as stream:
        return list(csv.DictReader(stream, delimiter='\t'))


def load_exclusions(path, image_size):
    """Load persistent guest-entry exclusions from an optional TSV file."""
    if not path.is_file():
        return {}
    lines = [line for line in path.read_text().splitlines()
             if line.strip() and not line.lstrip().startswith('#')]
    reader = csv.DictReader(lines, delimiter='\t')
    if reader.fieldnames != ['addr', 'reason']:
        raise ValueError('hybrid exclude list requires addr and reason columns')
    exclusions = {}
    for row in reader:
        address = int(row['addr'], 0)
        if not 0 <= address < image_size:
            raise ValueError('hybrid exclusion address outside initialized payload')
        if not row['reason'].strip():
            raise ValueError('hybrid exclusion requires a reason')
        reason = row['reason'].strip()
        if address in exclusions:
            existing = exclusions[address]
            if reason not in existing.split('; '):
                exclusions[address] = existing + '; ' + reason
        else:
            exclusions[address] = reason
    return exclusions


def symbol_spellings(value):
    values = {value}
    if value.startswith('_'):
        values.add(value[1:])
    if value.endswith('_'):
        values.add(value[:-1])
    for spelling in tuple(values):
        if spelling.startswith('_') and spelling.endswith('_'):
            values.add(spelling[1:-1])
    return values


def parse_guest_entry(symbol):
    for spelling in symbol_spellings(symbol):
        match = re.fullmatch(r'(?:ef_fn_|guest_fn_)([0-9a-fA-F]{1,8})(?:_.*)?', spelling)
        if match:
            return int(match.group(1), 16)
    return None


def load_abi_aliases(path):
    aliases = {}
    declaration = re.compile(
        r'(?m)^[ \t]*[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w*)\s*'
        r'\([^;]*\)\s*;\s*/\*([^*]*(?:\*(?!/)[^*]*)*)\*/')
    for match in declaration.finditer(path.read_text()):
        # The address annotation is an alias only when it leads the comment.
        # Other hexadecimal values may describe an indirect function pointer,
        # global, or helper used by the declaration rather than its entry.
        address = re.match(r'\s*0x([0-9a-fA-F]{1,8})\b', match.group(2))
        if address:
            aliases[match.group(1)] = int(address.group(1), 16)
    return aliases


def align(value, boundary):
    return (value + boundary - 1) // boundary * boundary


def load_objects(hand_dirs, mechanical_dir, report_path):
    """Return (objects, selected-address->kind), excluding non-ok drafts."""
    selected = {}
    for row in read_tsv(report_path):
        if row['state'] == 'ok':
            selected[int(row['addr'], 16)] = 'mechanical'

    objects = []
    paths_seen = set()
    for directory in hand_dirs:
        for path in sorted(directory.glob('*.obj')):
            resolved = path.resolve()
            if resolved not in paths_seen:
                paths_seen.add(resolved)
                objects.append((path, 'hand'))
    for address in selected:
        path = mechanical_dir / 'obj' / ('ef_fn_%08x.obj' % address)
        if path.is_file():
            resolved = path.resolve()
            if resolved not in paths_seen:
                paths_seen.add(resolved)
                objects.append((path, 'mechanical'))
    return objects, selected


def link(objects, functions, globals_, mechanical_selected, image,
         abi_aliases, arena_base=DEFAULT_ARENA, explicit_excludes=None,
         signatures=None):
    explicit_excludes = explicit_excludes or {}
    function_by_addr = {int(row['addr'], 16): row for row in functions}
    signature_by_addr = {int(row['addr'], 16): row for row in signatures or []}
    function_by_name = {}
    for row in functions:
        if row['name']:
            for spelling in symbol_spellings(row['name']):
                function_by_name.setdefault(spelling, []).append(row)
    global_by_name = {}
    for row in globals_:
        for spelling in symbol_spellings(row['name']):
            global_by_name.setdefault(spelling, []).append(row)

    modules = []
    hand_entry_candidates = {}
    mech_entry_candidates = {}
    public_definitions = {}
    for path, origin in objects:
        module = read_module(path.read_bytes())
        code_segments = [index for index, segment in enumerate(module.segments)
                         if index and segment.cls.upper() == 'CODE']
        if len(code_segments) != 1:
            raise OMFError('%s: expected one CODE segment, got %d' %
                           (path.name, len(code_segments)))
        record = {'path': path, 'origin': origin, 'module': module,
                  'code': code_segments[0], 'addresses': {}, 'excluded': None}
        modules.append(record)
        for public in module.publics:
            if public.local:
                continue
            for spelling in symbol_spellings(public.symbol):
                public_definitions.setdefault(spelling, []).append((record, public))
            address = parse_guest_entry(public.symbol)
            if address is None:
                spellings = symbol_spellings(public.symbol)
                candidates = {int(row['addr'], 16)
                              for spelling in spellings
                              for row in function_by_name.get(spelling, [])}
                if len(candidates) == 1:
                    address = next(iter(candidates))
            if address is None or address not in function_by_addr:
                continue
            row = function_by_addr[address]
            eligible = (row['state'] in ('compiles', 'spliced', 'verified') and
                        row['sig_status'] == 'confirmed')
            if address in explicit_excludes:
                continue
            if origin == 'hand' and eligible:
                hand_entry_candidates.setdefault(address, []).append((record, public))
            elif origin == 'mechanical' and address in mechanical_selected:
                mech_entry_candidates.setdefault(address, []).append((record, public))

    selected_entries = dict(hand_entry_candidates)
    for address, definitions in mech_entry_candidates.items():
        selected_entries.setdefault(address, definitions)

    # Exclude short entries before layout. These must remain original because
    # a near jump is five bytes and no generic safe trampoline island exists.
    excluded = []
    excluded.extend((address, reason) for address, reason in explicit_excludes.items())
    eligible_entries = {}
    for row in functions:
        address = int(row['addr'], 16)
        if (row['state'] in ('compiles', 'spliced', 'verified') and
                row['sig_status'] == 'confirmed' and address not in selected_entries):
            excluded.append((address, 'no compiled hand-written public definition found'))
    for address in mechanical_selected:
        if address not in mech_entry_candidates and address not in hand_entry_candidates:
            excluded.append((address, 'mechanical ok object has no public definition'))
    for address, definitions in selected_entries.items():
        row = function_by_addr[address]
        if int(row['size']) < 5:
            excluded.append((address, 'entry slot is shorter than the five-byte rel32 jump'))
            continue
        unique = {(id(record), public.segment, public.offset)
                  for record, public in definitions}
        if len(unique) != 1:
            raise OMFError('0x%08x has %d compiled entry definitions' %
                           (address, len(unique)))
        eligible_entries[address] = definitions[0]

    if not modules:
        raise OMFError('no OMF objects supplied')
    if arena_base <= BSS_END or arena_base >= STACK_TOP:
        raise OMFError('arena must be above cleared BSS and below the guest stack')

    # Each module keeps its segment topology, but every segment receives a
    # distinct flat guest address. This resolves both code and static-data
    # relocations without moving original globals or functions.
    cursor = arena_base
    for record in modules:
        module = record['module']
        for index, segment in enumerate(module.segments):
            if not index:
                continue
            if not segment.use32:
                record['excluded'] = 'contains a non-USE32 segment'
                break
            cursor = align(cursor, segment.align)
            record['addresses'][index] = cursor
            cursor += segment.size
    code_arena_end = cursor

    # C-generated bodies use OpenWatcom's normal callee-save set. Original
    # EF2000 callers also expect several functions to preserve ECX/EDX (and
    # occasionally other GPRs). Route every eligible external call and entry
    # through a small ABI trampoline that saves exactly those additional
    # registers. The trampoline calls the compiled body, so register inputs
    # and the declared return register pass through unchanged.
    trampoline_targets = {}
    trampoline_records = []
    trampoline_cursor = code_arena_end
    register_codes = {'eax': 0, 'ecx': 1, 'edx': 2, 'ebx': 3,
                      'ebp': 5, 'esi': 6, 'edi': 7}
    signature_skip_reasons = {}
    for address, (record, public) in sorted(eligible_entries.items()):
        signature = signature_by_addr.get(address)
        if not signature:
            signature_skip_reasons[address] = 'confirmed signature row missing'
            continue
        if signature['stack_bytes'] not in ('', '0'):
            signature_skip_reasons[address] = 'stack arguments need a stack-aware trampoline'
            continue
        preserved = {item.strip().lower() for item in signature['preserved'].split(',')
                     if item.strip()}
        if not preserved <= set(register_codes):
            signature_skip_reasons[address] = 'unsupported preserved-register set'
            continue
        outputs = [item.strip() for item in signature['outputs'].split(',') if item.strip()]
        if len(outputs) > 1:
            signature_skip_reasons[address] = 'multiple return registers need a custom trampoline'
            continue
        if outputs:
            output_reg = outputs[0].split(':', 1)[0].lower()
            output_reg = {'al': 'eax', 'ax': 'eax', 'bl': 'ebx', 'bx': 'ebx',
                          'cl': 'ecx', 'cx': 'ecx', 'dl': 'edx', 'dx': 'edx'}.get(
                              output_reg, output_reg)
            preserved.discard(output_reg)
        regs = [reg for reg in register_codes if reg in preserved]
        body = record['addresses'][public.segment] + public.offset
        # Caller-clean stack arguments ([ESP+4]..): the compiled body must
        # see them at the same offsets, so re-push them after the saves. With
        # nothing to save, a direct jump to the body is already correct.
        stack_arg_count = len(re.findall(r'\[ESP\+\d+\]', signature['inputs'], re.I))
        if stack_arg_count and not regs:
            continue
        start = trampoline_cursor
        code = bytearray(0x50 + register_codes[reg] for reg in regs)
        if stack_arg_count:
            offset = 4 * len(regs) + 4 * stack_arg_count
            for _ in range(stack_arg_count):
                code.extend(b'\xff\x74\x24' + bytes([offset]))
        call_next = start + len(code) + 5
        displacement = body - call_next
        if not -(1 << 31) <= displacement < (1 << 31):
            signature_skip_reasons[address] = 'trampoline call target is out of range'
            continue
        code.extend(b'\xe8' + (displacement & 0xffffffff).to_bytes(4, 'little'))
        if stack_arg_count:
            code.extend(b'\x83\xc4' + bytes([4 * stack_arg_count]))
        code.extend(0x58 + register_codes[reg] for reg in reversed(regs))
        code.append(0xc3)
        trampoline_records.append((start, bytes(code), address))
        trampoline_targets[address] = start
        trampoline_cursor += len(code)

    arena_end = trampoline_cursor
    if arena_end >= STACK_TOP:
        raise OMFError('linked arena overlaps the guest stack region')
    arena = bytearray(arena_end - arena_base)
    for record in modules:
        module = record['module']
        for index, segment in enumerate(module.segments):
            if not index or not segment.size:
                continue
            offset = record['addresses'][index] - arena_base
            arena[offset:offset + segment.size] = segment.data
    for address, code, _guest_address in trampoline_records:
        offset = address - arena_base
        arena[offset:offset + len(code)] = code

    def original_symbol(symbol):
        guest_entry = parse_guest_entry(symbol)
        if guest_entry is not None and guest_entry in function_by_addr:
            return guest_entry
        aliases = {address for spelling in symbol_spellings(symbol)
                   for name, address in abi_aliases.items()
                   if spelling in symbol_spellings(name)}
        if len(aliases) == 1:
            return next(iter(aliases))
        matches = {int(row['addr'], 16)
                   for spelling in symbol_spellings(symbol)
                   for row in function_by_name.get(spelling, [])}
        if len(matches) == 1:
            return next(iter(matches))
        global_matches = {int(row['addr'], 16)
                          for spelling in symbol_spellings(symbol)
                          for row in global_by_name.get(spelling, [])}
        if len(global_matches) == 1:
            return next(iter(global_matches))
        return None

    def resolve_external(symbol, current_record):
        address = original_symbol(symbol)
        if address is not None:
            if address in eligible_entries:
                record, public = eligible_entries[address]
                if record in active_modules:
                    return trampoline_targets.get(
                        address, record['addresses'][public.segment] + public.offset)
            return address
        local_definitions = {
            (public.segment, public.offset): public
            for public in current_record['module'].publics
            if public.local and
            symbol_spellings(public.symbol) & symbol_spellings(symbol)
        }
        if len(local_definitions) == 1:
            public = next(iter(local_definitions.values()))
            return current_record['addresses'][public.segment] + public.offset
        definitions = { (id(record), public.segment, public.offset): (record, public)
                        for spelling in symbol_spellings(symbol)
                        for record, public in public_definitions.get(spelling, []) }
        if len(definitions) == 1:
            record, public = next(iter(definitions.values()))
            if record['excluded']:
                raise OMFError('external %r belongs to excluded module %s' %
                               (symbol, record['path'].name))
            return record['addresses'][public.segment] + public.offset
        if len(definitions) > 1:
            raise OMFError('external %r has multiple compiled definitions' % symbol)
        raise OMFError('unresolved external %r' % symbol)

    # Module-level failures are isolated: leave its guest functions original,
    # then retry callers so their unresolved references can fall back to an
    # original tracked address where possible.
    active_modules = list(modules)
    for record in modules:
        if record['excluded']:
            active_modules.remove(record)
    module_errors = {}
    while True:
        newly_bad = {}
        for record in active_modules:
            module = record['module']
            try:
                for fixup in module.fixups:
                    if fixup.location != 9:
                        raise OMFError('unsupported fixup width %d' % fixup.location)
                    if fixup.frame_method == 1:
                        if fixup.frame_index is None:
                            raise OMFError('fixup group frame is missing')
                        group = module.groups[fixup.frame_index]
                        group_name = group['name'].upper()
                        if group_name == 'DGROUP':
                            if any(module.segments[index].cls.upper() not in
                                   ('DATA', 'BSS')
                                   for index in group['segments']):
                                raise OMFError('DGROUP contains a non-data segment')
                        elif group_name != 'FLAT':
                            raise OMFError('unsupported fixup group %r' % group['name'])
                    elif fixup.frame_method not in (4, 5):
                        raise OMFError('unsupported fixup frame method %d' %
                                       fixup.frame_method)
                    if fixup.target_method == 0:
                        target = record['addresses'][fixup.target_index]
                    elif fixup.target_method == 2:
                        target = resolve_external(module.externs[fixup.target_index], record)
                    else:
                        raise OMFError('unsupported fixup target method %d' %
                                       fixup.target_method)
                    segment = module.segments[fixup.segment]
                    source_address = record['addresses'][fixup.segment] + fixup.offset
                    original = int.from_bytes(segment.data[fixup.offset:fixup.offset + 4], 'little')
                    value = original + target + fixup.displacement
                    if fixup.mode == 'self':
                        value -= source_address + 4
                    arena_offset = source_address - arena_base
                    arena[arena_offset:arena_offset + 4] = (value & 0xffffffff).to_bytes(4, 'little')
            except (OMFError, IndexError, KeyError) as error:
                newly_bad[id(record)] = str(error)
        if not newly_bad:
            break
        module_errors.update(newly_bad)
        active_modules = [record for record in active_modules if id(record) not in newly_bad]
        for record in modules:
            if id(record) in newly_bad:
                record['excluded'] = newly_bad[id(record)]
        # Remove exports of failed modules; their game functions then resolve
        # to their original addresses on the next pass.
        public_definitions = {}
        for record in active_modules:
            for public in record['module'].publics:
                if public.local:
                    continue
                for spelling in symbol_spellings(public.symbol):
                    public_definitions.setdefault(spelling, []).append((record, public))

    patch_writes = []
    redirect_count = 0
    for address, (record, public) in sorted(eligible_entries.items()):
        if record not in active_modules:
            excluded.append((address, 'module link failed: ' + record['excluded']))
            continue
        target = trampoline_targets.get(
            address, record['addresses'][public.segment] + public.offset)
        displacement = target - (address + 5)
        if not -(1 << 31) <= displacement < (1 << 31):
            excluded.append((address, 'rel32 target is out of range'))
            continue
        patch_writes.append((address, b'\xe9' + (displacement & 0xffffffff).to_bytes(4, 'little')))
        redirect_count += 1

    patch_writes.append((arena_base, bytes(arena)))
    if len(patch_writes) > MAX_PATCH_WRITES:
        raise OMFError('hybrid needs %d patch writes; limit is %d' %
                       (len(patch_writes), MAX_PATCH_WRITES))
    total_patch_bytes = sum(len(data) for _, data in patch_writes)
    if total_patch_bytes > MAX_PATCH_BYTES:
        raise OMFError('hybrid patch exceeds 16 MiB')
    intervals = sorted((offset, offset + len(data)) for offset, data in patch_writes)
    for first, second in zip(intervals, intervals[1:]):
        if second[0] < first[1]:
            raise OMFError('hybrid writes overlap at 0x%x' % second[0])

    payload_size = len(image)
    if payload_size > BSS_END:
        # On the pinned target the initialized payload ends at 0x195fb0,
        # below BSS, so an arena above BSS is disjoint from source bytes.
        raise OMFError('unexpected initialized payload size 0x%x' % payload_size)
    digest = hashlib.sha256(image).digest()
    if digest.hex() != IMAGE_SHA256:
        raise OMFError('input payload SHA-256 is not the pinned EF2000 image')
    blob = bytearray(HEADER.pack(MAGIC, digest, len(patch_writes)))
    for offset, data in patch_writes:
        blob.extend(ENTRY.pack(offset, len(data)))
        blob.extend(data)

    active_addresses = {address for address, (record, _) in eligible_entries.items()
                        if record in active_modules}
    for record in modules:
        if record['excluded']:
            for address, (entry_record, _) in eligible_entries.items():
                if entry_record is record:
                    excluded.append((address, 'module link failed: ' + record['excluded']))
    report = {
        'format': 'ef2000-hybrid-link-v1',
        'source_sha256': digest.hex(),
        'payload_bytes': payload_size,
        'arena_base': arena_base,
        'arena_end': arena_end,
        'arena_bytes': len(arena),
        'compiled_code_bytes': code_arena_end - arena_base,
        'abi_trampolines': len(trampoline_records),
        'abi_trampoline_bytes': sum(len(code) for _, code, _ in trampoline_records),
        'object_modules': len(modules),
        'linked_modules': len(active_modules),
        'functions_redirected': redirect_count,
        'redirect_bytes': redirect_count * 5,
        'bytes_patched_total': total_patch_bytes,
        'mechanical_ok_available': len(mechanical_selected),
        'mechanical_ok_patched': sum(
            1 for address, (record, _) in eligible_entries.items()
            if record['origin'] == 'mechanical' and address in active_addresses),
        'handwritten_patched': sum(
            1 for address, (record, _) in eligible_entries.items()
            if record['origin'] == 'hand' and address in active_addresses),
        'excluded_functions': len(excluded),
        'excluded': [{'addr': '0x%08x' % address, 'reason': reason}
                     for address, reason in sorted(set(excluded))],
        'unwrapped_abi': [{'addr': '0x%08x' % address, 'reason': reason}
                          for address, reason in sorted(signature_skip_reasons.items())],
        'module_exclusions': [{'module': record['path'].name,
                               'reason': record['excluded']}
                              for record in modules if record['excluded']],
    }
    return bytes(blob), report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image', type=Path, required=True,
                        help='SHA-pinned initialized EF2000 payload.bin')
    parser.add_argument('--hand-objects', type=Path, action='append',
                        default=None,
                        help='directory containing OpenWatcom hand-written objects')
    parser.add_argument('--mechanical-dir', type=Path,
                        default=ROOT / 'local/mechanical')
    parser.add_argument('--mechanical-report', type=Path,
                        help='default: --mechanical-dir/report.tsv')
    parser.add_argument('--functions', type=Path,
                        default=ROOT / 'decomp/symbols/functions.tsv')
    parser.add_argument('--globals', type=Path,
                        default=ROOT / 'decomp/symbols/globals.tsv')
    parser.add_argument('--signatures', type=Path,
                        default=ROOT / 'decomp/symbols/signatures.tsv')
    parser.add_argument('--abi-header', type=Path, default=ABI_HEADER)
    parser.add_argument('--arena-base', type=lambda value: int(value, 0),
                        default=DEFAULT_ARENA)
    parser.add_argument('--exclude-address', action='append', default=[],
                        help='leave this recovered entry original (repeatable, ADDR[:REASON])')
    parser.add_argument('--exclude-list', type=Path,
                        default=ROOT / 'decomp/symbols/hybrid-exclude.tsv',
                        help='TSV of guest entries that must keep their original bytes')
    parser.add_argument('--only-address', action='append', default=[],
                        help='redirect only these guest entries (repeatable); every other '
                             'tracked function keeps its original bytes (isolation runs)')
    parser.add_argument('-o', '--output', type=Path, required=True,
                        help='ignored local EFPATCH1 hybrid patch file')
    parser.add_argument('--report', type=Path,
                        help='JSON link report (defaults next to output)')
    parser.add_argument('--excludes', type=Path,
                        help='TSV exclusions (defaults next to output)')
    args = parser.parse_args(argv)
    report_path = args.mechanical_report or args.mechanical_dir / 'report.tsv'
    try:
        source = args.image.read_bytes()
        hand_object_dirs = args.hand_objects or [ROOT / 'local/hybrid/obj']
        objects, selected = load_objects(hand_object_dirs, args.mechanical_dir,
                                         report_path)
        explicit_excludes = load_exclusions(args.exclude_list, len(source))
        for value in args.exclude_address:
            address_text, separator, reason = value.partition(':')
            explicit_excludes[int(address_text, 0)] = (
                reason if separator else 'explicitly demoted after hybrid replay failure')
        if args.only_address:
            keep = {int(value, 0) for value in args.only_address}
            for row in read_tsv(args.functions):
                address = int(row['addr'], 16)
                if address not in keep:
                    explicit_excludes.setdefault(address, 'isolation run: not selected')
        blob, report = link(objects, read_tsv(args.functions), read_tsv(args.globals),
                            selected, source,
                            load_abi_aliases(args.abi_header), args.arena_base,
                            explicit_excludes, read_tsv(args.signatures))
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open('xb') as stream:
            stream.write(blob)
        report_file = args.report or args.output.with_suffix('.json')
        report_file.write_text(json.dumps(report, indent=2) + '\n')
        excludes_file = args.excludes or args.output.with_name('hybrid-exclude.tsv')
        with excludes_file.open('w', newline='') as stream:
            writer = csv.writer(stream, delimiter='\t')
            writer.writerow(['addr', 'reason'])
            writer.writerows((entry['addr'], entry['reason']) for entry in report['excluded'])
        print('hybrid: %d functions, %d redirect bytes, %d arena bytes, %d/%d modules' %
              (report['functions_redirected'], report['redirect_bytes'],
               report['arena_bytes'], report['linked_modules'], report['object_modules']))
        print('hybrid: %d exclusions; report=%s' %
              (report['excluded_functions'], report_file))
    except (OSError, OMFError, ValueError, csv.Error) as error:
        parser.error(str(error))


if __name__ == '__main__':
    sys.exit(main())
