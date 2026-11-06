# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Conservative 32-bit OpenWatcom OMF inspector and offline EF2000 patch planner.

This does *not* apply patches or modify a game image. The generated EF patch
is deliberately not consumed by a runner yet. All supported relocations must
resolve; unfamiliar OMF constructs fail closed.
"""

import argparse
from dataclasses import dataclass, field as data_field
import csv
import hashlib
import json
from pathlib import Path

try:
    from .flirt_match import OMFError, records, field, index, name
except ImportError:  # python3 tools/omf_splice.py from the repository root
    from flirt_match import OMFError, records, field, index, name


@dataclass
class Segment:
    name: str
    cls: str
    size: int
    align: int
    use32: bool
    data: bytearray = data_field(init=False)
    written: bytearray = data_field(init=False)

    def __post_init__(self):
        self.data = bytearray(self.size)
        self.written = bytearray(self.size)


@dataclass(frozen=True)
class Public:
    symbol: str
    segment: int
    offset: int


@dataclass(frozen=True)
class Fixup:
    segment: int
    offset: int
    location: int
    mode: str
    frame_method: int
    frame_index: int | None
    target_method: int
    target_index: int
    displacement: int


@dataclass
class Module:
    name: str = ''
    segments: list = data_field(default_factory=lambda: [None])
    externs: list = data_field(default_factory=lambda: [''])
    publics: list = data_field(default_factory=list)
    groups: list = data_field(default_factory=lambda: [None])
    fixups: list = data_field(default_factory=list)


def read_module(blob):
    module = Module()
    names = ['']
    last_data = None
    frame_threads = [None] * 4
    target_threads = [None] * 4
    for kind, payload in records(blob):
        cursor = 0
        if kind == 0x80:  # THEADR
            module.name, cursor = name(payload, cursor)
        elif kind == 0x96:  # LNAMES
            while cursor < len(payload):
                item, cursor = name(payload, cursor)
                names.append(item)
        elif kind in (0x98, 0x99):  # SEGDEF
            attr, cursor = field(payload, cursor)
            align = (attr >> 5) & 7
            if align == 0 or align > 5:
                raise OMFError('absolute or unsupported SEGDEF alignment')
            if align == 5:
                # Some linkers use this for DWORD alignment; handle explicitly.
                alignment = 4
            else:
                alignment = (1, 1, 2, 16, 4096)[align]
            size, cursor = field(payload, cursor, 4 if kind == 0x99 else 2)
            if attr & 0x02:
                raise OMFError('large SEGDEF is unsupported')
            segname, cursor = index(payload, cursor)
            classname, cursor = index(payload, cursor)
            _, cursor = index(payload, cursor)
            if size > 4 * 1024 * 1024 or max(segname, classname) >= len(names):
                raise OMFError('invalid or oversized SEGDEF')
            module.segments.append(Segment(names[segname], names[classname], size,
                                           alignment, bool(attr & 1)))
        elif kind == 0x9a:  # GRPDEF
            groupname, cursor = index(payload, cursor)
            members = []
            while cursor < len(payload):
                descriptor, cursor = field(payload, cursor)
                if descriptor != 0xff:
                    raise OMFError('non-SEGDEF group component')
                member, cursor = index(payload, cursor)
                members.append(member)
            if groupname >= len(names):
                raise OMFError('undefined group name')
            module.groups.append({'name': names[groupname], 'segments': members})
        elif kind in (0x8c, 0xb4):  # EXTDEF/LEXTDEF
            while cursor < len(payload):
                symbol, cursor = name(payload, cursor)
                _, cursor = index(payload, cursor)
                module.externs.append(symbol)
        elif kind in (0x90, 0x91, 0xb6, 0xb7):  # PUBDEF/LPUBDEF
            _, cursor = index(payload, cursor)
            segment, cursor = index(payload, cursor)
            if not segment:
                raise OMFError('absolute PUBDEF is unsupported')
            while cursor < len(payload):
                symbol, cursor = name(payload, cursor)
                offset, cursor = field(payload, cursor, 4 if kind & 1 else 2)
                _, cursor = index(payload, cursor)
                module.publics.append(Public(symbol, segment, offset))
        elif kind in (0xa0, 0xa1):  # LEDATA
            segment, cursor = index(payload, cursor)
            offset, cursor = field(payload, cursor, 4 if kind & 1 else 2)
            if not 0 < segment < len(module.segments):
                raise OMFError('invalid LEDATA segment')
            seg = module.segments[segment]
            content = payload[cursor:]
            if offset + len(content) > seg.size:
                raise OMFError('LEDATA extends past segment')
            if any(seg.written[offset:offset + len(content)]):
                raise OMFError('overlapping LEDATA records')
            seg.data[offset:offset + len(content)] = content
            seg.written[offset:offset + len(content)] = b'\x01' * len(content)
            last_data = (segment, offset, len(content))
            cursor = len(payload)
        elif kind in (0x9c, 0x9d):  # FIXUPP
            while cursor < len(payload):
                first, cursor = field(payload, cursor)
                if first & 0x80:
                    second, cursor = field(payload, cursor)
                    location = (first >> 2) & 15
                    at = ((first & 3) << 8) | second
                    mode = 'segment' if first & 0x40 else 'self'
                    fixdat, cursor = field(payload, cursor)
                    fm = (fixdat >> 4) & 7
                    if fixdat & 0x80:
                        thread = frame_threads[fm & 3]
                        if thread is None:
                            raise OMFError('undefined frame thread')
                        fm, fi = thread
                    else:
                        fi = None
                        if fm <= 2:
                            fi, cursor = index(payload, cursor)
                    tm = fixdat & 3
                    if fixdat & 8:
                        thread = target_threads[tm]
                        if thread is None:
                            raise OMFError('undefined target thread')
                        tm, ti = thread
                    else:
                        ti, cursor = index(payload, cursor)
                    if not fixdat & 4:
                        disp, cursor = field(payload, cursor, 4 if kind == 0x9d else 2)
                    else:
                        disp = 0
                    if last_data is None:
                        raise OMFError('FIXUPP before LEDATA')
                    seg, base, length = last_data
                    widths = {1: 2, 9: 4}
                    if location not in widths:
                        raise OMFError(f'unsupported fixup location {location}')
                    if at + widths[location] > length:
                        raise OMFError('FIXUPP location exceeds LEDATA')
                    module.fixups.append(Fixup(seg, base + at, location, mode,
                                               fm, fi, tm, ti, disp))
                else:  # THREAD (persists across FIXUPP records)
                    method = (first >> 2) & 7
                    number = first & 3
                    datum = None
                    if method <= 2:
                        datum, cursor = index(payload, cursor)
                    if first & 0x40:
                        frame_threads[number] = (method, datum)
                    else:
                        target_threads[number] = (method & 3, datum)
        elif kind in (0xa2, 0xa3, 0xc2, 0xc3, 0xb2, 0xb3):
            raise OMFError(f'unsupported LIDATA/COMDAT/BAKPAT record 0x{kind:02x}')
        elif kind in (0x80, 0x88, 0x8a, 0x8b, 0x94, 0x95, 0x8e, 0x8f):
            pass  # metadata, end, or line numbers
        else:
            raise OMFError(f'unrecognized OMF record 0x{kind:02x}')
        if kind not in (0x9c, 0x9d) and kind not in (0x88, 0x94, 0x95):
            # FIXUPP must refer to the most recent LEDATA, not older data.
            if kind not in (0xa0, 0xa1):
                last_data = None
        if cursor != len(payload) and kind not in (0x80, 0x88, 0x8a, 0x8b,
                                                   0x94, 0x95, 0x8e, 0x8f):
            raise OMFError(f'unconsumed bytes in OMF record 0x{kind:02x}')
    for public in module.publics:
        if not 0 < public.segment < len(module.segments):
            raise OMFError(f'public {public.symbol} references undefined segment')
        if public.offset >= module.segments[public.segment].size:
            raise OMFError(f'public {public.symbol} outside segment')
    for group in module.groups[1:]:
        if any(not 0 < segment < len(module.segments) for segment in group['segments']):
            raise OMFError('GRPDEF references undefined segment')
    for fixup in module.fixups:
        tables = {0: module.segments, 1: module.groups, 2: module.externs}
        if fixup.target_method not in tables or not (0 < fixup.target_index <
                                                      len(tables[fixup.target_method])):
            raise OMFError('FIXUPP references invalid target index')
        if fixup.frame_method in tables:
            table = tables[fixup.frame_method]
            if fixup.frame_index is None or not 0 < fixup.frame_index < len(table):
                raise OMFError('FIXUPP references invalid frame index')
        elif fixup.frame_method not in (4, 5):
            raise OMFError('FIXUPP has unsupported frame method')
    return module


def inspection(module):
    return {'format': 'ef2000-omf-inspection-v1', 'module': module.name,
            'segments': [{'index': i, 'name': seg.name, 'class': seg.cls,
                          'size': seg.size, 'alignment': seg.align,
                          'use32': seg.use32, 'initialized': sum(seg.written)}
                         for i, seg in enumerate(module.segments) if i],
            'externals': module.externs[1:],
            'publics': [{'symbol': p.symbol, 'segment': p.segment, 'offset': p.offset}
                        for p in module.publics],
            'fixups': [vars(fixup) for fixup in module.fixups]}


def parse_symbols(path, address_key='addr'):
    with path.open(newline='') as stream:
        return list(csv.DictReader(stream, delimiter='\t'))


def resolve_symbol(symbol, functions, globals_):
    # Watcom C functions gain a trailing underscore and data externals gain a
    # leading underscore. Normalize either decoration only when it resolves
    # to exactly one canonical symbol.
    spellings = {symbol}
    if symbol.startswith('_'):
        spellings.add(symbol[1:])
    for spelling in tuple(spellings):
        if spelling.endswith('_'):
            spellings.add(spelling[:-1])
    matched = []
    for row in functions + globals_:
        if row['name'] in spellings and row['name']:
            matched.append(int(row['addr'], 16))
    if len(set(matched)) != 1:
        raise OMFError(f'external {symbol!r} has {len(set(matched))} canonical addresses')
    return matched[0]


def plan_patch(module, image, functions, globals_, arena_base, reviewed):
    """Build one self-contained patch, never writing to the source image."""
    code = [(i, seg) for i, seg in enumerate(module.segments) if i and
            (seg.cls.upper() == 'CODE' or seg.name.upper() in ('_TEXT', 'TEXT', '.TEXT'))]
    if len(code) != 1:
        raise OMFError('one CODE segment per object is currently required')
    code_index, code_segment = code[0]
    candidates = [p for p in module.publics if p.segment == code_index]
    if len(candidates) != 1 or candidates[0].offset:
        raise OMFError('one code PUBDEF at offset zero is currently required')
    symbol = candidates[0].symbol
    spellings = {symbol}
    if symbol.startswith('_'):
        spellings.add(symbol[1:])
    if symbol.endswith('_'):
        spellings.add(symbol[:-1])
    rows = [r for r in functions if r['name'] and r['name'] in spellings]
    if len(rows) != 1:
        raise OMFError('code public must uniquely match a canonical function name')
    row = rows[0]
    address = int(row['addr'], 16)
    old_size = int(row['size'])
    # Ghidra body_bytes can count disjoint blocks reached by tail branches.
    # Such a total is not a safe contiguous overwrite capacity. The tracker
    # must flag these until a separately proved contiguous extent exists.
    if 'noncontiguous' in row.get('notes', '').lower():
        raise OMFError('noncontiguous function body: reviewed contiguous overwrite extent required')
    if row['state'] not in ('compiles', 'spliced', 'verified') or row['sig_status'] != 'confirmed':
        raise OMFError('code public must have a confirmed ABI and compiles+ state')
    if address not in reviewed:
        raise OMFError(f'0x{address:08x} needs an explicitly reviewed control-flow boundary')
    if not 5 <= old_size or address + old_size > len(image):
        raise OMFError('replacement destination out of bounds or too small for redirect')
    if not code_segment.use32 or not all(code_segment.written):
        raise OMFError('CODE segment must be USE32 and fully initialized')
    if arena_base < len(image) or arena_base >= 0xffff0000:
        raise OMFError('patch arena must be an explicit offset above the original image')

    def align_at(value, alignment):
        return (value + alignment - 1) // alignment * alignment

    segment_addresses = {}
    cursor = arena_base
    inline = code_segment.size <= old_size
    if inline:
        segment_addresses[code_index] = address
    for i, seg in enumerate(module.segments):
        if not i or i == code_index and inline:
            continue
        if not seg.use32 or seg.size > 1024 * 1024:
            raise OMFError('patch arena requires bounded USE32 segments')
        cursor = align_at(cursor, seg.align)
        segment_addresses[i] = cursor
        cursor += seg.size
    if cursor > arena_base + 16 * 1024 * 1024:
        raise OMFError('patch arena exceeds 16 MiB offline planner limit')
    output = {i: bytearray(seg.data) for i, seg in enumerate(module.segments) if i}
    touched = set()
    for fixup in module.fixups:
        seg = module.segments[fixup.segment]
        width = 4 if fixup.location == 9 else 2
        if fixup.location != 9:
            raise OMFError('only 32-bit offset fixups are patchable')
        if fixup.target_method == 0:
            if fixup.target_index not in segment_addresses:
                raise OMFError('fixup references unknown segment')
            target = segment_addresses[fixup.target_index]
        elif fixup.target_method == 2:
            if not 0 < fixup.target_index < len(module.externs):
                raise OMFError('fixup references undefined external')
            target = resolve_symbol(module.externs[fixup.target_index], functions, globals_)
        else:
            raise OMFError('unsupported group or explicit-frame target fixup')
        # A FLAT frame has a proven zero base in this fixed image. It therefore
        # supports both self-relative external calls and absolute external
        # offsets; other frame-specific near offsets remain unsupported.
        flat_group_frame = (
            fixup.frame_method == 1 and fixup.frame_index is not None and
            0 < fixup.frame_index < len(module.groups) and
            module.groups[fixup.frame_index]['name'].upper() == 'FLAT'
        )
        if fixup.frame_method not in (4, 5) and not flat_group_frame:
            raise OMFError('frame-specific near offset relocation requires group-base proof')
        key = (fixup.segment, fixup.offset)
        if key in touched:
            raise OMFError('duplicate fixup at one location')
        touched.add(key)
        original = int.from_bytes(output[fixup.segment][fixup.offset:fixup.offset+width], 'little')
        value = original + target + fixup.displacement
        if fixup.mode == 'self':
            value -= segment_addresses[fixup.segment] + fixup.offset + width
        output[fixup.segment][fixup.offset:fixup.offset+width] = (value & 0xffffffff).to_bytes(4, 'little')

    writes = []
    if inline:
        patched = bytes(output[code_index]) + b'\xcc' * (old_size - code_segment.size)
    else:
        target = segment_addresses[code_index]
        rel = (target - address - 5) & 0xffffffff
        patched = b'\xe9' + rel.to_bytes(4, 'little') + b'\xcc' * (old_size - 5)
    writes.append({'offset': address, 'bytes': patched.hex(), 'symbol': symbol,
                   'kind': 'inline' if inline else 'redirect'})
    for i, seg in enumerate(module.segments):
        if not i or i == code_index and inline or not seg.size:
            continue
        writes.append({'offset': segment_addresses[i], 'bytes': bytes(output[i]).hex(),
                       'symbol': seg.name, 'kind': 'arena'})
    return {'format': 'ef2000-patch-v1', 'base_image_sha256': hashlib.sha256(image).hexdigest(),
            'arena_base': arena_base, 'arena_end': cursor, 'writes': writes,
            'limitations': 'offline plan only; no runtime loader/arena reservation yet'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('object', type=Path)
    parser.add_argument('-o', '--output', required=True, type=Path)
    parser.add_argument('--image', type=Path, help='original extracted application payload (build only)')
    parser.add_argument('--functions', type=Path, default=Path('decomp/symbols/functions.tsv'))
    parser.add_argument('--globals', dest='globals_', type=Path,
                        default=Path('decomp/symbols/globals.tsv'))
    parser.add_argument('--arena-base', type=lambda s: int(s, 0))
    parser.add_argument('--reviewed-flow', action='append', default=[],
                        type=lambda s: int(s, 0), metavar='ADDR')
    args = parser.parse_args()
    try:
        module = read_module(args.object.read_bytes())
        if args.image is None:
            report = inspection(module)
        else:
            if args.arena_base is None:
                parser.error('--arena-base required for offline patch plans')
            report = plan_patch(module, args.image.read_bytes(),
                                parse_symbols(args.functions), parse_symbols(args.globals_),
                                args.arena_base, set(args.reviewed_flow))
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + '\n')
        print(f'{report["format"]}: {args.output}')
    except (OMFError, ValueError) as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()
