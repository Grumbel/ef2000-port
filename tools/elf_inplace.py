#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Diagnostic ELF32-i386 C-object -> SHA-bound in-place EFPATCH1.

This is deliberately *not* the Watcom/OMF verification pipeline. It makes
isolated guest differential tests possible with a host GCC -m32 compiler.
Accept exactly one reviewed, same-size C function, absolute 32-bit external
relocations, and no allocatable private data or other executable sections.
"""

import argparse
import hashlib
import struct
from pathlib import Path

try:
    from .efpatch import pack
    from .highscores_differential import load_image
    from .omf_splice import parse_symbols, resolve_symbol
except ImportError:
    from efpatch import pack
    from highscores_differential import load_image
    from omf_splice import parse_symbols, resolve_symbol


ROOT = Path(__file__).resolve().parents[1]
HEADER = struct.Struct('<16sHHIIIIIHHHHHH')
SECTION = struct.Struct('<IIIIIIIIII')
SYMBOL = struct.Struct('<IIIBBH')
RELOC = struct.Struct('<II')
SHF_ALLOC = 2
SHF_EXECINSTR = 4
SHT_PROGBITS = 1
SHT_SYMTAB = 2
SHT_STRTAB = 3
SHT_RELA = 4
SHT_NOBITS = 8
SHT_REL = 9
R_386_32 = 1
STT_FUNC = 2


class ElfPatchError(ValueError):
    pass


def get_string(blob, start, label):
    if not 0 <= start < len(blob):
        raise ElfPatchError(f'invalid {label} string offset')
    end = blob.find(b'\0', start)
    if end < 0:
        raise ElfPatchError(f'unterminated {label} string')
    try:
        return blob[start:end].decode('ascii')
    except UnicodeDecodeError as error:
        raise ElfPatchError(f'non-ASCII {label} string') from error


def read_elf32(blob):
    """Parse only the ELF structures needed by one plain i386 C function."""
    if len(blob) < HEADER.size:
        raise ElfPatchError('truncated ELF header')
    (ident, kind, machine, version, _, _, shoff, _, ehsize, _, _,
     shentsize, shnum, shstridx) = HEADER.unpack_from(blob)
    if (ident[:7] != b'\x7fELF\x01\x01\x01' or kind != 1 or machine != 3 or
            version != 1 or ehsize != HEADER.size or shentsize != SECTION.size or
            not 1 < shnum <= 64 or shstridx >= shnum or
            shoff > len(blob) or shnum * SECTION.size > len(blob) - shoff):
        raise ElfPatchError('expected bounded little-endian ELF32 i386 ET_REL')
    sections = [SECTION.unpack_from(blob, shoff + i * SECTION.size)
                for i in range(shnum)]

    def section_bytes(index):
        if index >= shnum:
            raise ElfPatchError('invalid section reference')
        _, kind, _, _, offset, size, _, _, _, _ = sections[index]
        if kind == SHT_NOBITS:
            return b''
        if offset > len(blob) or size > len(blob) - offset:
            raise ElfPatchError('section extends beyond ELF file')
        return blob[offset:offset + size]

    if sections[shstridx][1] != SHT_STRTAB:
        raise ElfPatchError('invalid section-name string table')
    section_names = section_bytes(shstridx)
    names = [get_string(section_names, sh[0], 'section') for sh in sections]
    if names.count('.text') != 1:
        raise ElfPatchError('exactly one .text section required')
    code_index = names.index('.text')
    code_section = sections[code_index]
    if (code_section[1] != SHT_PROGBITS or
            code_section[2] & (SHF_ALLOC | SHF_EXECINSTR) !=
            SHF_ALLOC | SHF_EXECINSTR or not 0 < code_section[5] <= 65536):
        raise ElfPatchError('invalid executable .text section')
    for i, sh in enumerate(sections):
        _, kind, flags, _, _, size, _, info, _, _ = sh
        if i != code_index and flags & SHF_EXECINSTR and size:
            raise ElfPatchError(f'extra executable section {names[i]!r}')
        if i != code_index and flags & SHF_ALLOC and size and names[i] != '.eh_frame':
            raise ElfPatchError(f'private allocatable data {names[i]!r} is unsupported')
        if kind in (SHT_REL, SHT_RELA) and (info >= shnum or
                (info != code_index and names[info] != '.eh_frame')):
            raise ElfPatchError('relocations outside code/unwind metadata are unsupported')
    tables = [(i, sh) for i, sh in enumerate(sections) if sh[1] == SHT_SYMTAB]
    if len(tables) != 1:
        raise ElfPatchError('exactly one symbol table required')
    _, symtab = tables[0]
    if (symtab[9] != SYMBOL.size or symtab[5] % SYMBOL.size or
            symtab[6] >= shnum or sections[symtab[6]][1] != SHT_STRTAB):
        raise ElfPatchError('invalid ELF32 symbol table')
    symbols_data = section_bytes(tables[0][0])
    strings = section_bytes(symtab[6])
    symbols = []
    for offset in range(0, len(symbols_data), SYMBOL.size):
        name, value, size, info, _, index = SYMBOL.unpack_from(symbols_data, offset)
        symbols.append((get_string(strings, name, 'symbol'), value, size, info & 15, index))
    functions = [s for s in symbols if s[3] == STT_FUNC and s[4] == code_index]
    if len(functions) != 1 or functions[0][1:3] != (0, code_section[5]):
        raise ElfPatchError('one function must fill the entire .text section')
    relocs = []
    for i, sh in enumerate(sections):
        if sh[1] == SHT_RELA and sh[7] == code_index:
            raise ElfPatchError('RELA code relocations are unsupported')
        if sh[1] != SHT_REL or sh[7] != code_index:
            continue
        if sh[6] != tables[0][0] or sh[9] != RELOC.size or sh[5] % RELOC.size:
            raise ElfPatchError('invalid code relocation table')
        for offset in range(0, sh[5], RELOC.size):
            where, info = RELOC.unpack_from(section_bytes(i), offset)
            if where + 4 > code_section[5] or info >> 8 >= len(symbols):
                raise ElfPatchError('relocation extends outside code or symbol table')
            relocs.append((where, info & 255, symbols[info >> 8]))
    if len({where for where, _, _ in relocs}) != len(relocs):
        raise ElfPatchError('duplicate relocation offsets')
    ranges = sorted((where, where + 4) for where, _, _ in relocs)
    if any(a[1] > b[0] for a, b in zip(ranges, ranges[1:])):
        raise ElfPatchError('overlapping relocations')
    return functions[0][0], bytearray(section_bytes(code_index)), relocs


def build(object_bytes, image, functions, globals_, reviewed):
    name, code, relocs = read_elf32(object_bytes)
    matches = [row for row in functions if row.get('name') == name]
    if len(matches) != 1:
        raise ElfPatchError(f'function {name!r} has no unique tracked address')
    row = matches[0]
    address = int(row['addr'], 16)
    if (address != reviewed or row['sig_status'] != 'confirmed' or
            row['state'] not in ('compiles', 'spliced', 'verified') or
            'noncontiguous' in row.get('notes', '').lower()):
        raise ElfPatchError('function needs explicitly reviewed contiguous flow and ABI')
    if len(code) != int(row['size']) or address + len(code) > len(image):
        raise ElfPatchError('GCC C body must match the reviewed original extent exactly')
    for where, kind, symbol in relocs:
        name, value, _, _, index = symbol
        if kind != R_386_32 or index != 0 or not name:
            raise ElfPatchError('only absolute external R_386_32 code relocations supported')
        target = resolve_symbol(name, functions, globals_)
        addend = struct.unpack_from('<I', code, where)[0]
        struct.pack_into('<I', code, where, (target + addend) & 0xffffffff)
    plan = {'format': 'ef2000-patch-v1',
            'base_image_sha256': hashlib.sha256(image).hexdigest(),
            'writes': [{'kind': 'inline', 'offset': address, 'bytes': code.hex()}]}
    return pack(plan, image), code


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('object', type=Path, help='GCC -m32 ELF32 i386 .o (diagnostic only)')
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--executable', type=Path)
    source.add_argument('--payload', type=Path)
    parser.add_argument('--reviewed-flow', required=True, type=lambda value: int(value, 0))
    parser.add_argument('--functions', type=Path, default=ROOT / 'decomp/symbols/functions.tsv')
    parser.add_argument('--globals', type=Path, default=ROOT / 'decomp/symbols/globals.tsv')
    parser.add_argument('-o', '--output', required=True, type=Path)
    args = parser.parse_args(argv)
    try:
        original = load_image(executable=args.executable, payload_path=args.payload)
        blob, body = build(args.object.read_bytes(), original,
                           parse_symbols(args.functions), parse_symbols(args.globals),
                           args.reviewed_flow)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open('xb') as stream:
            stream.write(blob)
        print(f'diagnostic ELF32 C splice: {len(body)} bytes at '
              f'0x{args.reviewed_flow:x} -> {args.output}')
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()
