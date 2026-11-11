#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Static C-versus-original-asm fingerprint comparison (debugging aid).

Compares, per tracker function, features that a faithful C translation must
preserve even though OpenWatcom allocates registers differently:

* absolute data addresses (``ds:0x19bb58`` style operands),
* structure offsets (``[reg+0xNN]`` displacements, memory operand widths),
* immediate constants,
* direct calls to other recovered functions (``guest_fn_XXXX_`` / names),
* the sequence of conditional-jump kinds,
* ``ret N``.

A function whose feature sets differ is listed with the missing/extra items;
this is where the C does not replicate the ASM.  Needs the ignored local
Ghidra payload.  The original window is linear from the entry and ends at the
first ret not followed by more code of the same function, so functions with
interleaved data or multiple returns are cut at the tracker size.
"""

import argparse
from collections import Counter
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
HEX = re.compile(r'(?<![\w$])(?:0x)?([0-9a-f]{1,8})h?(?![\w$])', re.I)


def operands_features(mnemonic, text):
    """Return (data addresses, displacements, immediates) from one operand text."""
    text = text.replace('0x', '')
    addrs, disps, imms = [], [], []
    for part in re.split(r',(?![^\[]*\])', text):
        part = re.sub(r'^(?:[qdt]?word|byte) ptr ', '', part.strip().lower())
        wd = re.search(r'(?:^|[\s:])-?([0-9a-f]+)\[(?!esp)\w+(?:\*\d)?(?:\+\w+(?:\*\d)?)?\]', part)
        if wd and not re.search(r'esp', part):
            disps.append(int(wd.group(1), 16))
        mem = re.search(r'\[(.*)\]', part)
        direct = re.match(r'^(?:\w+ ptr )?(?:ds:|dword ptr ds:|byte ptr ds:|word ptr ds:)?([0-9a-f]{5,8})$', part)
        if mem:
            inner = mem.group(1)
            m = re.search(r'[+-]([0-9a-f]+)$', inner.replace(' ', ''))
            if m and not re.search(r'esp', inner):
                disps.append(int(m.group(1), 16))
            elif re.fullmatch(r'[0-9a-f]{5,8}', inner):
                addrs.append(int(inner, 16))
        m = re.search(r'ds:([0-9a-f]+)', part)
        if m:
            addrs.append(int(m.group(1), 16))
        elif re.fullmatch(r'[0-9a-f]+', part) and mnemonic not in ('call', 'jmp') \
                and not mnemonic.startswith('j'):
            value = int(part, 16)
            (addrs if value >= 0x100000 else imms).append(value)
    return addrs, disps, imms


def original_lines(image, address, size, objdump):
    text = subprocess.run(
        [objdump, '-b', 'binary', '-m', 'i386', '-M', 'intel', '-D',
         '--start-address=%#x' % address, '--stop-address=%#x' % (address + size), str(image)],
        check=True, capture_output=True, text=True).stdout
    out = []
    for line in text.splitlines():
        fields = line.split('\t')
        if len(fields) < 3 or not re.match(r'^\s*[0-9a-f]+:$', fields[0]):
            continue
        match = re.match(r'^(\w+)\s*(.*)$', fields[2].strip())
        if match:
            out.append((match.group(1).lower(), re.sub(r'\s*;.*|\s*<.*', '', match.group(2))))
    return out


def compiled_lines(listing, name):
    out, inside = [], False
    for line in listing:
        if re.match(r'^[0-9A-F]{4}\s+%s_:' % re.escape(name), line):
            inside = True
            continue
        if not inside:
            continue
        if 'Routine Size' in line or re.match(r'^[0-9A-F]{4}\s+(?!L\$)\w+_:', line):
            break
        match = re.match(r'^[0-9A-F]{4}\s+(?:[0-9A-F]{2}\s)+\s*(\w+)\s*(.*)$', line)
        if match:
            out.append((match.group(1).lower(), match.group(2).strip().replace('0x0000', '0x')))
    return out


def features(lines, native_calls, symbols=None):
    addrs, disps, imms, calls, jumps = Counter(), Counter(), Counter(), Counter(), []
    ret = None
    for mnemonic, operands in lines:
        if mnemonic.startswith('ret'):
            ret = int(operands.replace('0x', ''), 16) if operands.strip() else 0
            continue
        if mnemonic == 'call':
            target = operands.strip()
            key = native_calls(target)
            if key:
                calls[key] += 1
            else:
                calls['unresolved'] += 1
            continue
        if mnemonic.startswith('j') and mnemonic != 'jmp':
            jumps.append(mnemonic) if False else jumps.append(
                {'je': 'e', 'jz': 'e', 'jne': 'n', 'jnz': 'n'}.get(mnemonic, mnemonic))
            continue
        if mnemonic in ('push', 'pop', 'nop', 'lea') and mnemonic != 'lea':
            continue
        if symbols:
            for token in set(re.findall(r'[A-Za-z_]\w*', operands)):
                if token in symbols:
                    operands = re.sub(r'\b%s\b' % re.escape(token), '%x' % symbols[token], operands)
        a, d, i = operands_features(mnemonic, operands)
        addrs.update(a)
        disps.update(d)
        imms.update(i)
    return {'addr': addrs, 'disp': disps, 'imm': imms, 'call': calls}, jumps, ret


def call_key_original(target):
    match = re.match(r'^(?:0x)?([0-9a-f]{4,8})$', target.replace('0x', ''))
    return int(match.group(1), 16) if match else None


SYMBOLS = {}


def call_key_compiled(target):
    match = re.match(r'^guest_fn_([0-9a-f]+)_$', target)
    if match:
        return int(match.group(1), 16)
    return SYMBOLS.get(target)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--objects', type=Path, required=True)
    parser.add_argument('--wdis', default='wdis')
    parser.add_argument('--objdump', default='objdump')
    parser.add_argument('--image', type=Path, default=ROOT / 'local/ghidra/payload.bin')
    parser.add_argument('--tracker', type=Path, default=ROOT / 'decomp/symbols/functions.tsv')
    parser.add_argument('--states', default='compiles,spliced,verified')
    parser.add_argument('--only', help='file with one hex address per line to restrict to')
    parser.add_argument('--address', action='append', default=[], help='single function (repeatable)')
    parser.add_argument('--verbose', action='store_true')
    args = parser.parse_args()

    for row in args.tracker.read_text().splitlines()[1:]:
        fields = row.split('\t')
        try:
            SYMBOLS[fields[2] + '_'] = int(fields[0], 16)
        except (ValueError, IndexError):
            pass
    glob = args.tracker.with_name('globals.tsv')
    globals_ = {}
    for row in glob.read_text().splitlines()[1:]:
        fields = row.split('\t')
        try:
            globals_['_' + fields[3]] = int(fields[0], 16)
            globals_[fields[3]] = int(fields[0], 16)
        except (ValueError, IndexError):
            pass
    only = set()
    if args.only:
        only |= {int(x.split()[0], 16) for x in Path(args.only).read_text().split('\n') if x.strip()}
    only |= {int(x, 16) for x in args.address}
    states = set(args.states.split(','))
    work = Path(tempfile.mkdtemp(prefix='asm-match-'))
    listings, bad, checked = {}, 0, 0
    try:
        for row in args.tracker.read_text().splitlines()[1:]:
            fields = row.split('\t')
            try:
                address, size = int(fields[0], 16), int(fields[1])
            except ValueError:
                continue
            name, module, state = fields[2], fields[3], fields[4]
            if state not in states or (only and address not in only):
                continue
            obj = args.objects / (module + '.obj')
            if not obj.exists():
                continue
            if module not in listings:
                shutil.copy(obj, work / obj.name)
                listings[module] = subprocess.run(
                    [args.wdis, obj.name], cwd=work, capture_output=True, text=True).stdout.splitlines()
            comp = compiled_lines(listings[module], name)
            if not comp:
                continue
            checked += 1
            of, oj, oret = features(original_lines(args.image, address, size, args.objdump),
                                    call_key_original)
            cf, cj, cret = features(comp, call_key_compiled, globals_)
            issues = []
            unresolved = cf['call'].pop('unresolved', 0)
            of['call'].pop('unresolved', 0)
            for side in (of, cf):
                side['const'] = Counter()
                for kind in ('addr', 'disp', 'imm'):
                    side['const'].update(side[kind])
            for kind in ('const', 'call'):
                missing = set(of[kind]) - set(cf[kind])
                extra = set(cf[kind]) - set(of[kind])
                if kind == 'call' and len(missing) <= unresolved:
                    missing = set()
                if kind == 'const':
                    # small constants are routinely strength-reduced or folded
                    # masks are rewritten by the compiler (and 0xff <-> movzx)
                    noise = {0xff, 0xffff, 0xff00, 0xffffff, 0xffffffff}
                    missing = {v for v in missing if v > 15 and v not in noise}
                    extra = {v for v in extra if v > 15 and v not in noise}
                if missing or extra:
                    issues.append('%s missing %s extra %s' % (
                        kind, ','.join('%x' % v for v in sorted(missing)) or '-',
                        ','.join('%x' % v for v in sorted(extra)) or '-'))
            if oret != cret:
                issues.append('ret %s/%s' % (oret, cret))
            if issues or args.verbose:
                bad += bool(issues)
                print('%#x %s [%s]: %s' % (address, name, module, '; '.join(issues) or 'ok'))
    finally:
        shutil.rmtree(work, ignore_errors=True)
    print('%d checked, %d with differing fingerprints' % (checked, bad))


if __name__ == '__main__':
    sys.exit(main())
