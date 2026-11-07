#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Compile one recovered C module to 32-bit OpenWatcom OMF.

The conservative bootstrap flags are provisional until the ABI audit in
decompilation-plan §3.5. No executable or proprietary material is linked.
"""

import argparse
import csv
import os
from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / 'decomp/src'
# -s suppresses the default __CHK stack-probe call. Until we can resolve
# the original runtime symbols, generated game code must not add one.
# Initial functional-compatibility profile, not a claim about the game's
# original compiler switches. Avoid numerically unstable FP optimizations.
FLAGS = ('-bt=dos', '-3r', '-zp1', '-fpi87', '-fp3', '-ob', '-ot', '-s', '-wx', '-za99')

GPRS = ('eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp')
PARAM_REGS = {
    'EAX': 'eax', 'AX': 'eax', 'AL': 'eax',
    'EBX': 'ebx', 'BX': 'ebx', 'BL': 'ebx',
    'ECX': 'ecx', 'CX': 'ecx', 'CL': 'ecx',
    'EDX': 'edx', 'DX': 'edx', 'DL': 'edx',
    'ESI': 'esi', 'EDI': 'edi', 'EBP': 'ebp',
}
VALUE_REGS = {
    **{reg.upper(): reg for reg in GPRS},
    'AX': 'ax', 'AL': 'al', 'BX': 'bx', 'BL': 'bl',
    'CX': 'cx', 'CL': 'cl', 'DX': 'dx', 'DL': 'dl',
}


def confirmed_abi_pragmas():
    """Render confirmed register ABIs for OpenWatcom's compiled C bodies."""
    signatures = ROOT / 'decomp/symbols/signatures.tsv'
    abi_header = ROOT / 'decomp/include/ef_abi.h'
    existing = set(re.findall(
        r'(?m)^\s*#pragma\s+aux\s+([A-Za-z_]\w*)',
        abi_header.read_text()))
    abi_text = abi_header.read_text()
    aliases = {}
    declared_types = {}
    declaration = re.compile(
        r'(?m)^[ \t]*([A-Za-z_][\w \t\*]*?)\s+([A-Za-z_]\w*)\s*'
        r'\([^;]*\)\s*;[ \t]*(?:/\*([^*]*(?:\*(?!/)[^*]*)*)\*/)?')
    for match in declaration.finditer(abi_text):
        declared_types[match.group(2)] = match.group(1).strip()
        comment = match.group(3) or ''
        if 'indirect' in comment.lower():
            continue
        address = re.search(r'\b0x([0-9a-fA-F]{1,8})\b', comment)
        if address:
            aliases.setdefault(int(address.group(1), 16), {})[match.group(2)] = match.group(1).strip()
    lines = ['/* Generated from confirmed signatures.tsv by ef2000_wcc.py. */']
    with signatures.open(newline='') as stream:
        for row in csv.DictReader(stream, delimiter='\t'):
            declarations = aliases.get(int(row['addr'], 16), {})
            declarations = dict(declarations)
            if row['name'] in declared_types:
                declarations.setdefault(row['name'], declared_types[row['name']])
            names = set(declarations)
            names -= existing
            if not names or row['stack_bytes'] not in ('', '0'):
                continue
            inputs = [item.strip() for item in row['inputs'].split(',') if item.strip()]
            outputs = [item.strip() for item in row['outputs'].split(',') if item.strip()]
            try:
                regs = [PARAM_REGS[item.split(':', 1)[0].upper()] for item in inputs]
            except KeyError:
                continue
            if len(outputs) > 1:
                continue
            preserved = {reg.strip().lower() for reg in row['preserved'].split(',')
                         if reg.strip()}
            if not preserved <= set(GPRS):
                continue
            for name in sorted(names):
                value = None
                output_reg = None
                if outputs and declarations[name] != 'void':
                    output_name, _, output_type = outputs[0].partition(':')
                    result_type = declarations[name].replace(' ', '')
                    widths = {'int8_t': 8, 'uint8_t': 8, 'char': 8,
                              'int16_t': 16, 'uint16_t': 16, 'short': 16,
                              'int32_t': 32, 'uint32_t': 32, 'int': 32,
                              'unsigned': 32, 'long': 32, 'ef_addr32': 32}
                    result_width = widths.get(result_type)
                    output_width = widths.get(output_type.replace(' ', ''))
                    if result_width and output_width and result_width == output_width:
                        try:
                            value = VALUE_REGS[output_name.upper()]
                        except KeyError:
                            continue
                        if result_width == 8:
                            value = 'al' if value in ('eax', 'al') else 'bl'
                        elif result_width == 16:
                            value = {'eax': 'ax', 'ebx': 'bx', 'ecx': 'cx',
                                     'edx': 'dx'}.get(value, value)
                        output_reg = output_name.upper()
                parts = ['#pragma aux', name,
                         'parm ' + ('[' + '] ['.join(regs) + ']' if regs else 'caller []')]
                if value:
                    parts.extend(('value', '[' + value + ']'))
                result_gpr = output_reg
                result_gpr = {'AL': 'EAX', 'AX': 'EAX', 'BL': 'EBX', 'BX': 'EBX',
                              'CL': 'ECX', 'CX': 'ECX', 'DL': 'EDX', 'DX': 'EDX'}.get(
                                  result_gpr, result_gpr)
                modified = [reg for reg in GPRS
                            if reg not in preserved and reg.upper() != result_gpr]
                if modified:
                    parts.extend(('modify', '[' + ' '.join(modified) + ']'))
                lines.append(' '.join(parts))
    return '\n'.join(lines) + '\n'


def compile_module(source, *, compiler='wcc386', output=None, execute=subprocess.run):
    source = source.resolve()
    if source.suffix != '.c' or not source.is_file():
        raise ValueError('expected an existing .c source file')
    # Fixtures and recovered source are the only supported inputs. Do not
    # permit arbitrary host code to masquerade as derived game source.
    # The wrapper itself lives in the read-only Nix store, but the user's
    # checked-out source and build outputs must stay in their working tree.
    work_root = Path(os.environ.get('EF2000_SOURCE', Path.cwd())).resolve()
    root = next((base for base in (work_root, ROOT) if
                 source.is_relative_to(base / 'decomp/src') or
                 source.is_relative_to(base / 'decomp/tests')), None)
    if root is None:
        raise ValueError('source must be under decomp/src or decomp/tests')
    destination = (output or work_root / 'decomp/build' / (source.stem + '.obj')).resolve()
    if destination.suffix != '.obj':
        raise ValueError('output must have a .obj extension')
    destination.parent.mkdir(parents=True, exist_ok=True)
    abi_header = destination.parent / 'ef_confirmed_abi.h'
    abi_header.write_text(confirmed_abi_pragmas())
    command = [compiler, *FLAGS, '-i=' + str(root / 'decomp/include'),
               '-fi=' + str(abi_header), '-fo=' + str(destination), str(source)]
    execute(command, check=True)
    if not destination.is_file() or destination.stat().st_size == 0:
        raise ValueError('compiler returned without creating a nonempty OMF object')
    return destination


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='one decomp/src/*.c source')
    parser.add_argument('--compiler', default='wcc386', help='OpenWatcom wcc386 executable')
    parser.add_argument('--output', type=Path, help='OMF output (default: decomp/build/<stem>.obj)')
    args = parser.parse_args(argv)
    try:
        print(compile_module(args.source, compiler=args.compiler, output=args.output))
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'ef2000-wcc: {error}\n')


if __name__ == '__main__':
    main()
