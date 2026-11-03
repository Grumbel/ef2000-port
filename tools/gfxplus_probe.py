#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Read-only GFX+ updater / DOS executable triage, without extraction or execution.

The GFX+ Tactpat.exe updater contains patch data and recognizable 3Dfx/Glide
strings. File offsets in this report are *not* executable guest addresses.
Every input must be provided by the user; never commit original game files.
"""

import argparse
import hashlib
import json
from pathlib import Path
import struct


MAX_FILE_SIZE = 128 * 1024 * 1024
MAX_MATCHES = 8192

# Text anchors, not function symbols or proof of runnable embedded code.
ANCHORS = {
    '3dfx_upgrade': b'Tactcom 2.0 to 3DFX update',
    'rendition_upgrade': b'Tactcom 2.0 to Rendition update',
    'glide_2_1': b'Glide(tm) Version 2.1',
    'grSstQueryHardware': b'grSstQueryHardware',
    'grSstOpen': b'grSstOpen',
    'grTexDownloadMipMap': b'grTexDownloadMipMap',
    'sst1InitMapBoard': b'sst1InitMapBoard',
    'voodoo_file': b'VOODOO_FILE',
    'glide_no_splash': b'FX_GLIDE_NO_SPLASH',
    'gfxplus_font': b'GFXPLUSCDrawBitChr',
    'fxpci_dos': b'fxPCI for DOS',
    'x32_loader': b'B23X',
    'separate_glide_overlay': b'GLIDE2X.OVL',
}


def all_offsets(data, needle):
    if not needle:
        raise ValueError('empty search needle')
    results = []
    cursor = 0
    while True:
        cursor = data.find(needle, cursor)
        if cursor == -1:
            return results
        results.append(cursor)
        if len(results) > MAX_MATCHES:
            raise ValueError('too many signature occurrences')
        cursor += 1


def u16(data, offset):
    return struct.unpack_from('<H', data, offset)[0]


def u32(data, offset):
    return struct.unpack_from('<I', data, offset)[0]


def pe_sections(data):
    """Return validated on-disk section ranges, if this is a PE file."""
    if len(data) < 0x40 or data[:2] != b'MZ':
        raise ValueError('input is not an MZ executable')
    header_offset = u32(data, 0x3c)
    if header_offset < 0x40 or header_offset + 24 > len(data):
        return None
    if data[header_offset:header_offset + 4] != b'PE\0\0':
        return None
    machine, count = struct.unpack_from('<HH', data, header_offset + 4)
    optional_size = u16(data, header_offset + 20)
    optional_start = header_offset + 24
    image_base = None
    if optional_size >= 32 and optional_start + 32 <= len(data):
        if u16(data, optional_start) == 0x10b:  # PE32 (not PE32+)
            image_base = u32(data, optional_start + 28)
    if not 1 <= count <= 96:
        raise ValueError('implausible PE section count')
    start = header_offset + 24 + optional_size
    if start + count * 40 > len(data):
        raise ValueError('truncated PE section table')
    result = []
    for index in range(count):
        record = start + index * 40
        name = data[record:record + 8].split(b'\0', 1)[0].decode('ascii', 'replace')
        rva = u32(data, record + 12)
        size = u32(data, record + 16)
        offset = u32(data, record + 20)
        if size and (not offset or offset + size > len(data)):
            raise ValueError(f'PE section {name!r} extends beyond file')
        result.append({'name': name, 'file_offset': offset, 'size': size,
                       'rva': rva})
    return {'machine': machine, 'image_base': image_base, 'sections': result}


def section_at(offset, sections):
    for section in sections:
        if section['size'] and section['file_offset'] <= offset < section['file_offset'] + section['size']:
            return section
    return None


def possible_mz_header(data, offset):
    """Validate a *possible* embedded DOS header, without asserting a file end.

    An updater can embed incomplete binary fragments and coincidental MZ bytes;
    neither a passing header nor nearby Glide text proves a complete EXE.
    """
    if offset + 28 > len(data) or data[offset:offset + 2] != b'MZ':
        return None
    last, pages, relocs, paragraphs = struct.unpack_from('<4H', data, offset + 2)
    header_bytes = paragraphs * 16
    length = (pages - 1) * 512 + (last or 512)
    if not (pages and last <= 512 and 28 <= header_bytes <= 4096 and
            header_bytes <= length <= len(data) - offset):
        return None
    ip = u16(data, offset + 0x14)
    cs = u16(data, offset + 0x16)
    reloc_table = u16(data, offset + 0x18)
    if relocs and (reloc_table < 28 or reloc_table + 4 * relocs > header_bytes):
        return None
    if header_bytes + cs * 16 + ip >= length:
        return None
    result = {'file_offset': offset, 'declared_dos_size': length,
              'header_size': header_bytes, 'relocations': relocs}
    if offset + 0x40 <= len(data):
        pointer = u32(data, offset + 0x3c)
        if pointer and pointer < len(data) - offset - 4:
            magic = data[offset + pointer:offset + pointer + 4]
            if magic[:2] in (b'LE', b'LX', b'NE') or magic == b'PE\0\0':
                result['extended_header'] = magic[:2].decode('ascii')
    return result


def inspect(data):
    if len(data) > MAX_FILE_SIZE:
        raise ValueError('input is too large for bounded GFX+ triage')
    pe = pe_sections(data)
    sections = pe['sections'] if pe else []
    image_base = pe['image_base'] if pe else None

    def location(offset):
        section = section_at(offset, sections)
        result = {'file_offset': offset, 'section': section['name'] if section else None}
        if section and image_base is not None:
            result['pe_virtual_address'] = (image_base + section['rva'] +
                                            offset - section['file_offset'])
        return result

    markers = {key: [location(offset) for offset in all_offsets(data, needle)]
               for key, needle in ANCHORS.items()}
    candidates = []
    for offset in all_offsets(data, b'MZ'):
        if offset == 0:
            continue
        candidate = possible_mz_header(data, offset)
        if candidate:
            candidate['section'] = location(offset)['section']
            candidates.append(candidate)
    return {
        'format': 'PE (updater or installer)' if pe else 'DOS MZ / other MZ executable',
        'size': len(data),
        'sha256': hashlib.sha256(data).hexdigest(),
        'pe_machine': pe['machine'] if pe else None,
        'pe_image_base': image_base,
        'sections': sections,
        'anchors': markers,
        'possible_embedded_mz_headers': candidates,
        'warning': ('Text anchors are not Ghidra functions, guest addresses, '
                    'or proof that any complete executable can be carved. '
                    'No patch was applied and no game code was executed.'),
    }


def summarize(report):
    lines = [f"{report['format']}: {report['size']:,} bytes, SHA-256 {report['sha256']}"]
    for section in report['sections']:
        lines.append(f"  section {section['name']}: "
                     f"file 0x{section['file_offset']:x}, size 0x{section['size']:x}, "
                     f"RVA 0x{section['rva']:x}")
    for name, matches in report['anchors'].items():
        if matches:
            locations = ', '.join(f"0x{match['file_offset']:x}"
                                  for match in matches[:12])
            if len(matches) > 12:
                locations += f", … ({len(matches)} total)"
            lines.append(f'  {name}: {locations}')
    candidates = report['possible_embedded_mz_headers']
    lines.append(f'  plausible embedded MZ headers: {len(candidates)} (not extracted)')
    for candidate in candidates:
        lines.append(f"    0x{candidate['file_offset']:x}: DOS size "
                     f"{candidate['declared_dos_size']:,}, "
                     f"section {candidate['section'] or '(none)'}")
    lines.append(report['warning'])
    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path,
                        help='external GFX+ Tactpat.exe, or a user-supplied DOS executable')
    parser.add_argument('--json', action='store_true', help='emit structured report')
    args = parser.parse_args()
    try:
        if args.executable.stat().st_size > MAX_FILE_SIZE:
            raise ValueError('input is too large')
        report = inspect(args.executable.read_bytes())
    except (OSError, ValueError, struct.error) as error:
        parser.exit(1, f'{error}\n')
    print(json.dumps(report, indent=2) if args.json else summarize(report))


if __name__ == '__main__':
    main()
