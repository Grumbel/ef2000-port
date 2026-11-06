#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Mechanically turn exported Ghidra pseudo-C into compilable draft C.

Input is the local, git-ignored Ghidra export (``local/ghidra/full-export``).
Output goes ONLY under ``local/mechanical/`` (git-ignored): the result is a
derived work of Ghidra output and must never be committed (plan rule 2).
Hand-written, reviewed bodies in ``decomp/src`` always win over the output
(see ``--skip-recovered``).

Pipeline (docs/decompilation-plan-rev3.md, M0):
  1. normalise Ghidra types and names (FUN_x -> ef_fn_x, DAT_x -> EF_U32(..));
  2. apply confirmed signatures from decomp/symbols/signatures.tsv as
     ``#pragma aux`` register conventions in one shared header;
  3. emit one .c per function plus ``ef_gen_abi.h``;
  4. optionally compile each with OpenWatcom (``--compile``) and write
     ``report.tsv`` (state: ok / compile-error / manual).

Functions that use constructs we cannot translate mechanically (software
interrupts, ``in_*``/``unaff_*`` registers, CONCAT/SUB pieces, inline asm
markers) are flagged ``manual`` and not emitted for compilation.
"""

import argparse
import csv
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parent.parent
EXPORT = ROOT / 'local/ghidra/full-export'
OUT = ROOT / 'local/mechanical'
SIGNATURES = ROOT / 'decomp/symbols/signatures.tsv'
FUNCTIONS = ROOT / 'decomp/symbols/functions.tsv'
GLOBALS = ROOT / 'decomp/symbols/globals.tsv'
FLAGS = ('-bt=dos', '-3r', '-zp1', '-fpi87', '-fp3', '-ob', '-ot', '-za99', '-w1')

TYPES = [
    (r'\bundefined8\b', 'uint64_t'), (r'\bundefined4\b', 'uint32_t'),
    (r'\bundefined3\b', 'uint32_t'), (r'\bundefined2\b', 'uint16_t'),
    (r'\bundefined1\b', 'uint8_t'), (r'\bundefined\b', 'uint8_t'),
    (r'\bulonglong\b', 'uint64_t'), (r'\blonglong\b', 'int64_t'),
    (r'\bfloat10\b', 'long double'),
    (r'\bdword\b', 'uint32_t'),
    (r'\bcode\b', 'ef_gen_code'),
    (r'\bushort\b', 'uint16_t'), (r'\buint\b', 'uint32_t'),
    (r'\bulong\b', 'uint32_t'), (r'\bbyte\b', 'uint8_t'),
    (r'\bbool\b', 'int'), (r'\bint3\b', 'int32_t'), (r'\bint5\b', 'int32_t'),
    (r'\b__(?:fastcall|stdcall|cdecl|thiscall)\b', ''),
]
# Anything matching these cannot be translated mechanically.
MANUAL = re.compile(r'\b(?:swi|in_[A-Za-z]+|unaff_[A-Za-z0-9]+|extraout_[A-Za-z0-9]+|'
                    r'CONCAT\d+|SUB\d+|ZEXT\d+|SEXT\d+|halt_baddata|halt_unimplemented|'
                    r'WARNING|bad_instruction|CARRY\d|SBORROW\d|__halt)\b')
HEX = r'([0-9a-fA-F]{8})'
DEF = re.compile(r'^(?P<ret>[A-Za-z_][\w\s\*]*?)\s*\b(?P<name>\w+)\((?P<params>[^()]*)\)\s*$')


def read_tsv(path):
    with open(path, newline='') as handle:
        return list(csv.DictReader(handle, delimiter='\t'))


def normalize_type(ctype):
    for pattern, replacement in TYPES:
        ctype = re.sub(pattern, replacement, ctype)
    return ctype


def recovered_addresses():
    """Addresses that already have hand-written bodies (state compiles+)."""
    skip = {'unknown', 'named', 'typed', 'drafted', 'blocked', 'compiles-bad', ''}
    return {int(row['addr'], 16) for row in read_tsv(FUNCTIONS)
            if row['state'] not in skip}


def load_signatures():
    sigs = {}
    for row in read_tsv(SIGNATURES):
        ins = [(reg, normalize_type(ctype))
               for item in row['inputs'].split(',') if ':' in item
               for reg, ctype in [item.split(':', 1)]]
        outs = [(reg, normalize_type(ctype))
                for item in row['outputs'].split(',') if ':' in item
                for reg, ctype in [item.split(':', 1)]]
        sigs[int(row['addr'], 16)] = (ins, outs)
    return sigs


def load_globals():
    return {int(row['addr'], 16): row for row in read_tsv(GLOBALS)}


def dat_expression(address, globals_):
    row = globals_.get(address)
    ctype = normalize_type(row['type']) if row else ''
    raw = '0x%08x' % address
    access = {
        'uint8_t': 'EF_U8(%s)' % raw,
        'int8_t': '(*(int8_t *)ef_guest_memory_ptr(%s))' % raw,
        'uint16_t': 'EF_U16(%s)' % raw,
        'int16_t': 'EF_S16(%s)' % raw,
        'uint32_t': 'EF_U32(%s)' % raw,
        'ef_addr32': 'EF_U32(%s)' % raw,
        'int32_t': 'EF_S32(%s)' % raw,
        'float': '(*(float *)ef_guest_memory_ptr(%s))' % raw,
        'double': '(*(double *)ef_guest_memory_ptr(%s))' % raw,
        'uint64_t': '(*(uint64_t *)ef_guest_memory_ptr(%s))' % raw,
        'int64_t': '(*(int64_t *)ef_guest_memory_ptr(%s))' % raw,
    }
    return access.get(ctype, 'EF_U32(%s)' % raw)


REG32 = {'EAX': 'eax', 'EDX': 'edx', 'EBX': 'ebx', 'ECX': 'ecx', 'ESI': 'esi',
         'EDI': 'edi', 'AL': 'al', 'AX': 'ax', 'DL': 'dl', 'DX': 'dx',
         'BL': 'bl', 'BX': 'bx', 'CL': 'cl', 'CX': 'cx'}


def pragma_for(name, sig):
    ins, outs = sig
    regs = [REG32.get(reg.upper()) for reg, _ in ins
            if not re.fullmatch(r'\[?ESP\+\d+\]?', reg.upper())]
    if any(reg is None for reg in regs) or len(outs) > 1:
        return None
    text = '#pragma aux %s parm [%s]' % (name, '] ['.join(regs)) if regs else \
           '#pragma aux %s parm []' % name
    if outs:
        out = REG32.get(outs[0][0].upper())
        if out is None:
            return None
        text += ' value [%s]' % out
    return text


def convert(text, entry, names, sig_in=None, globals_=None):
    """Return (definition_signature, body_c, flags) for one pseudo-C file."""
    lines = text.split('\n')
    lines = [line for line in lines if not line.startswith(('/*', ' *'))]
    body = '\n'.join(lines).strip('\n')
    flags = set()
    extra = []
    inferred = []
    if sig_in:
        # Watcom register arguments: Ghidra shows them as in_<REG>.
        for reg, ctype in sig_in:
            stack = re.fullmatch(r'\[?ESP\+(\d+)\]?', reg.upper())
            if stack:
                extra.append('%s stack_%s' % (ctype if ctype else 'uint32_t', stack.group(1)))
                continue
            name = 'in_' + reg.upper()
            if re.search(r'\b' + name + r'\b', body):
                body = re.sub(r'\b' + name + r'\b', 'r_' + reg.lower(), body)
            extra.append('%s r_%s' % (ctype if ctype else 'uint32_t', reg.lower()))
    elif sig_in is None:
        # No confirmed signature: infer register inputs from in_<REG> uses.
        # HYPOTHESIS ONLY (plan rule 4); the pragma is marked inferred.
        fam = {'EAX': 'eax', 'AX': 'eax', 'AL': 'eax', 'AH': 'eax',
               'EDX': 'edx', 'DX': 'edx', 'DL': 'edx', 'DH': 'edx',
               'EBX': 'ebx', 'BX': 'ebx', 'BL': 'ebx', 'BH': 'ebx',
               'ECX': 'ecx', 'CX': 'ecx', 'CL': 'ecx', 'CH': 'ecx'}
        cast = {'E': 'uint32_t', 'X': 'uint16_t', 'L': 'uint8_t', 'H': 'uint8_t'}
        used = []
        for reg, base in fam.items():
            name = 'in_' + reg
            if re.search(r'\b' + name + r'\b', body):
                # Ghidra's declaration is only notation for a live-in
                # register. A write cannot be represented by an input-only
                # ABI parameter, so leave that function for manual handling.
                if re.search(r'\b' + name + r'\s*(?:=|\+\+|--|\+=|-=)', body):
                    flags.add('manual')
                body = re.sub(r'^\s*[A-Za-z_]\w*(?:\s+|\s*\*)+%s\s*;\s*$' % name,
                              '', body, flags=re.MULTILINE)
                body = re.sub(r'\b' + name + r'\b',
                              'r_' + base if reg.startswith('E') else
                              '((%s)r_%s)' % (cast[reg[-1]], base), body)
                if base not in used:
                    used.append(base)
        order = ['eax', 'edx', 'ebx', 'ecx']
        for base in sorted(used, key=order.index):
            extra.append('uint32_t r_' + base)
            inferred.append(base)
    if MANUAL.search(body) or re.search(r'\._\d_\d_', body):
        flags.add('manual')
    for pattern, repl in TYPES:
        body = re.sub(pattern, repl, body)
    body = re.sub(r'\btrue\b', '1', body)
    body = re.sub(r'\bfalse\b', '0', body)
    # Rename callees, referenced by auto name or by recovered name.
    body = re.sub(r'\b(?:thunk_)?FUN_' + HEX + r'\b',
                  lambda m: 'ef_fn_' + m.group(1).lower(), body)
    for old, new in names.items():
        body = re.sub(r'\b' + re.escape(old) + r'\b', new, body)
    globals_ = globals_ or {}
    body = re.sub(r'(?<![A-Za-z0-9_])_?DAT_' + HEX + r'\b',
                  lambda m: dat_expression(int(m.group(1), 16), globals_), body)
    body = re.sub(r'\b(?:PTR_|s_|u_|BYTE_|WORD_|DWORD_)\w*?_?' + HEX + r'\b(?:\s*\+\s*\d+)?',
                  lambda m: '((char *)ef_guest_memory_ptr(0x%s))' % m.group(1).lower(), body)
    body = re.sub(r'\bLAB_' + HEX + r'\b', lambda m: 'lab_' + m.group(1).lower(), body)
    body = re.sub(r'\bfunc_0x' + HEX + r'\b', lambda m: 'ef_fn_' + m.group(1).lower(), body)
    first = next((line for line in body.split('\n') if line.strip()), '')
    match = DEF.match(first)
    if not match:
        flags.add('manual')
        return None, body, flags, inferred
    ret = ' '.join(match.group('ret').split()) or 'int'
    params = match.group('params').strip() or 'void'
    if sig_in and extra:
        # Confirmed signatures replace Ghidra's generic prototype.
        params = ', '.join(extra)
    elif inferred:
        # Keep Ghidra's non-register parameters; place inferred register
        # inputs first so the pragma and the C declaration have the same ABI.
        params = ', '.join(extra + ([] if params == 'void' else [params]))
    sig = '%s ef_fn_%08x(%s)' % (ret, entry, params)
    rest = body.split('\n', 1)[1] if '\n' in body else ''
    return sig, rest, flags, inferred


def callees(body):
    return sorted(set(re.findall(r'\bef_fn_[0-9a-f]{8}\b', body)))


PREAMBLE = '''/* Mechanical draft derived from Ghidra output. NEVER COMMIT. */
#include "ef_abi.h"
#include "ef_mem.h"
#include "ef_gen_abi.h"
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--export', type=Path, default=EXPORT)
    parser.add_argument('--out', type=Path, default=OUT)
    parser.add_argument('--skip-recovered', action='store_true',
                        help='do not generate functions that already have hand-written bodies')
    parser.add_argument('--only', help='comma-separated hex addresses')
    parser.add_argument('--compile', action='store_true', help='compile with wcc386')
    parser.add_argument('--compiler', default=os.environ.get('WCC386', 'wcc386'))
    parser.add_argument('--limit', type=int)
    args = parser.parse_args()

    rows = read_tsv(args.export / 'functions.tsv')
    only = {int(a, 16) for a in args.only.split(',')} if args.only else None
    skip = recovered_addresses() if args.skip_recovered else set()
    sigs = load_signatures()
    globals_ = load_globals()
    names = {}
    for row in rows:
        entry = int(row['entry'], 16)
        if not re.fullmatch(r'(?:thunk_)?FUN_[0-9a-f]+', row['name']):
            names[row['name']] = 'ef_fn_%08x' % entry

    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / 'src').mkdir(exist_ok=True)
    header, report = [], []
    count = 0
    for row in rows:
        entry = int(row['entry'], 16)
        if (only and entry not in only) or entry in skip:
            continue
        if args.limit is not None and count >= args.limit:
            break
        path = args.export / row['file']
        if not path.is_file():
            continue
        sig, body, flags, inferred = convert(path.read_text(errors='replace'), entry, names,
                                   sigs[entry][0] if entry in sigs else None, globals_)
        state = 'manual' if flags else 'generated'
        if sig and entry in sigs:
            pragma = pragma_for('ef_fn_%08x' % entry, sigs[entry])
            if pragma:
                header.append(pragma)
        if sig and inferred and entry not in sigs:
            header.append('#pragma aux ef_fn_%08x parm [%s] /* inferred */' %
                          (entry, '] ['.join(inferred)))
        if sig:
            header.append(sig + ';')
        if state == 'generated':
            labels = sorted(set(re.findall(r'\blab_[0-9a-f]{8}\b', body)))
            (args.out / 'src' / ('ef_fn_%08x.c' % entry)).write_text(
                PREAMBLE + '\n' + sig + '\n' + body + '\n')
        report.append([row['entry'], row['name'], row['body_bytes'], state, ''])
        count += 1
    (args.out / 'ef_gen_abi.h').write_text(
        '/* Generated; derived from Ghidra output. NEVER COMMIT. */\n'
        '#include "ef_types.h"\n'
        'typedef void ef_gen_code(void);\n' + '\n'.join(dict.fromkeys(header)) + '\n')

    if args.compile:
        include = ['-i=' + str(ROOT / 'decomp/include'), '-i=' + str(args.out)]
        (args.out / 'obj').mkdir(exist_ok=True)
        for item in report:
            if item[3] != 'generated':
                continue
            source = args.out / 'src' / ('ef_fn_%08x.c' % int(item[0], 16))
            obj = args.out / 'obj' / (source.stem + '.obj')
            result = subprocess.run([args.compiler, *FLAGS, *include, '-fo=' + str(obj),
                                     str(source)], capture_output=True, text=True)
            if result.returncode == 0:
                item[3] = 'ok'
            else:
                item[3] = 'compile-error'
                lines = [line for line in (result.stdout + result.stderr).split('\n')
                         if 'Error' in line]
                item[4] = (lines[0] if lines else 'error')[:200].replace('\t', ' ')

    with open(args.out / 'report.tsv', 'w', newline='') as handle:
        writer = csv.writer(handle, delimiter='\t')
        writer.writerow(['addr', 'name', 'bytes', 'state', 'first_error'])
        writer.writerows(report)
    totals = {}
    for item in report:
        entry = totals.setdefault(item[3], [0, 0])
        entry[0] += 1
        entry[1] += int(item[2])
    for state, (n, size) in sorted(totals.items()):
        print('%-14s %5d functions %8d bytes' % (state, n, size))
    return 0


if __name__ == '__main__':
    sys.exit(main())
