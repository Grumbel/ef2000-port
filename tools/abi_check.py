#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Static ABI comparison of recovered functions against the original code.

For every tracker function in a linkable state this compares the original
disassembly (linear window from the entry, `objdump`) with the compiled
OpenWatcom object (`wdis`) and reports

* ``ret``: the final ``ret N`` immediates differ.  The caller then corrupts
  the stack (the original cleans N bytes, the C pops a different amount).
  hybrid_link adds no trampoline for stack-argument functions, so this class
  of mismatch is fatal in the hybrid image.
* ``clobber`` (with --clobbers): GPRs the C writes (not saved/restored) that the
  original never writes.  Informational: the hybrid trampoline covers most.
* ``x87`` (with --x87): only one of the two uses the FPU.  Informational: the
  original often calls a helper that the C inlines.

Exit status is 1 when any ``ret`` mismatch is found among functions that are
not listed in the exclude file.  Needs no game executable; the flat image is
the ignored local Ghidra payload.  Debugging aid, never committed output.
"""

import argparse
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
GPR = ('eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp')
SUBREG = {'ax': 'eax', 'al': 'eax', 'ah': 'eax', 'bx': 'ebx', 'bl': 'ebx',
          'bh': 'ebx', 'cx': 'ecx', 'cl': 'ecx', 'ch': 'ecx', 'dx': 'edx',
          'dl': 'edx', 'dh': 'edx', 'si': 'esi', 'di': 'edi', 'bp': 'ebp'}
WRITERS = re.compile(r'^(mov|lea|add|sub|adc|sbb|xor|and|or|shl|shr|sar|sal|'
                     r'rol|ror|imul|neg|not|inc|dec|movzx|movsx|bsf|bsr|set\w+|'
                     r'cmov\w+|lods\w?)$')


def reg(text):
    text = text.strip().lower()
    return SUBREG.get(text, text)


def analyze(lines):
    """Return (clobbers, ret_immediate, uses_x87) of (mnemonic, operands) pairs."""
    writes, pushed, popped = set(), [], []
    ret, x87 = None, False
    for mnemonic, operands in lines:
        mnemonic = mnemonic.lower()
        if mnemonic == 'push':
            pushed.append(reg(operands))
        elif mnemonic == 'pop':
            popped.append(reg(operands))
        elif mnemonic.startswith('ret'):
            ret = int(operands, 16) if operands.strip() else 0
        elif mnemonic.startswith('f'):
            x87 = True
        elif mnemonic == 'cdq':
            writes.add('edx')
        elif mnemonic in ('cwde', 'cbw'):
            writes.add('eax')
        elif mnemonic in ('mul', 'div', 'idiv'):
            writes.update(('eax', 'edx'))
        elif mnemonic == 'xchg':
            writes.update(reg(item) for item in operands.split(',') if reg(item) in GPR)
        elif mnemonic.startswith(('movs', 'stos', 'rep')):
            writes.update(('esi', 'edi', 'ecx'))
        elif WRITERS.match(mnemonic):
            destination = reg(operands.split(',')[0])
            if destination in GPR:
                writes.add(destination)
    return writes - (set(pushed) & set(popped)), ret, x87


def original_lines(image, address, size, objdump):
    text = subprocess.run(
        [objdump, '-b', 'binary', '-m', 'i386', '-M', 'intel', '-D',
         '--start-address=%#x' % address, '--stop-address=%#x' % (address + size), str(image)],
        check=True, capture_output=True, text=True).stdout
    lines = []
    for line in text.splitlines():
        # "addr:<TAB>bytes<TAB>mnemonic operands"; byte-only continuation lines of
        # long instructions have no third field and must be skipped.
        fields = line.split('\t')
        if len(fields) < 3 or not re.match(r'^\s*[0-9a-f]+:$', fields[0]):
            continue
        match = re.match(r'^(\w+)\s*(.*)$', fields[2].strip())
        if match:
            lines.append((match.group(1), re.sub(r'\s*;.*', '', match.group(2)).replace('0x', '')))
    return lines


def compiled_lines(listing, name):
    lines, inside = [], False
    for line in listing:
        if re.match(r'^[0-9A-F]{4}\s+%s_:' % re.escape(name), line):
            inside = True
            continue
        if not inside:
            continue
        # A routine ends at its "Routine Size" line or at the next non-local label.
        if 'Routine Size' in line or re.match(r'^[0-9A-F]{4}\s+(?!L\$)\w+_:', line):
            break
        match = re.match(r'^[0-9A-F]{4}\s+(?:[0-9A-F]{2}\s)+\s*(\w+)\s*(.*)$', line)
        if match:
            lines.append((match.group(1), match.group(2).strip().replace('0x0000', '').replace('0x', '')))
    return lines


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--objects', type=Path, required=True,
                        help='directory of compiled hand objects (.../decomp-objects)')
    parser.add_argument('--wdis', default='wdis')
    parser.add_argument('--objdump', default='objdump')
    parser.add_argument('--image', type=Path, default=ROOT / 'local/ghidra/payload.bin')
    parser.add_argument('--tracker', type=Path, default=ROOT / 'decomp/symbols/functions.tsv')
    parser.add_argument('--exclude', type=Path, default=ROOT / 'decomp/symbols/hybrid-exclude.tsv')
    parser.add_argument('--states', default='compiles,spliced,verified')
    parser.add_argument('--clobbers', action='store_true', help='also report extra clobbered registers')
    parser.add_argument('--x87', action='store_true',
                        help='also report FPU-use differences (usually the original calls an x87 helper that the C inlines)')
    parser.add_argument('--include-excluded', action='store_true')
    args = parser.parse_args()

    excluded = set()
    for line in args.exclude.read_text().splitlines():
        try:
            excluded.add(int(line.split('\t')[0], 16))
        except ValueError:
            pass
    states = set(args.states.split(','))
    work = Path(tempfile.mkdtemp(prefix='abi-check-'))
    listings = {}
    failures = 0
    try:
        for row in args.tracker.read_text().splitlines()[1:]:
            fields = row.split('\t')
            try:
                address, size = int(fields[0], 16), int(fields[1])
            except ValueError:
                continue
            name, module, state = fields[2], fields[3], fields[4]
            if state not in states or (address in excluded and not args.include_excluded):
                continue
            obj = args.objects / (module + '.obj')
            if not obj.exists():
                continue
            if module not in listings:
                shutil.copy(obj, work / obj.name)
                listings[module] = subprocess.run(
                    [args.wdis, obj.name], cwd=work, capture_output=True, text=True).stdout.splitlines()
            compiled = compiled_lines(listings[module], name)
            if not compiled:
                continue
            o_writes, o_ret, o_x87 = analyze(original_lines(args.image, address, size, args.objdump))
            c_writes, c_ret, c_x87 = analyze(compiled)
            issues = []
            if o_ret is not None and c_ret is not None and o_ret != c_ret:
                issues.append('ret original %d, compiled %d' % (o_ret, c_ret))
                failures += 1
            if args.clobbers and c_writes - o_writes:
                issues.append('clobber ' + ','.join(sorted(c_writes - o_writes)))
            if args.x87 and o_x87 != c_x87:
                issues.append('x87 original %s, compiled %s' % (o_x87, c_x87))
            if issues:
                print('%#x %s: %s' % (address, name, '; '.join(issues)))
    finally:
        shutil.rmtree(work, ignore_errors=True)
    print('%d ret mismatch(es)' % failures)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
