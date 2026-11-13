#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Lift an audited subset of 32-bit flat x86 ASM to explicit-state draft C.

Generated code stays under ignored local/. Unsupported instructions, uncertain
boundaries and decoder disagreement fail closed. This is an intermediate C
representation, not a replacement for a confirmed native function ABI.
"""

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re

try:
    from . import asm_inventory
except ImportError:
    import asm_inventory

ROOT = Path(__file__).resolve().parents[1]
REGS = ('eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi')
REG = {name: (index, 32, 0) for index, name in enumerate(REGS)}
for index, name in enumerate(('ax', 'cx', 'dx', 'bx', 'sp', 'bp', 'si', 'di')):
    REG[name] = (index, 16, 0)
for index, name in enumerate(('al', 'cl', 'dl', 'bl')):
    REG[name] = (index, 8, 0)
for index, name in enumerate(('ah', 'ch', 'dh', 'bh')):
    REG[name] = (index, 8, 8)

CONDITIONS = {
    'je': 'z', 'jz': 'z', 'jne': '!z', 'jnz': '!z',
    'jb': 'c', 'jc': 'c', 'jnae': 'c', 'jae': '!c', 'jnb': '!c', 'jnc': '!c',
    'jbe': '(c || z)', 'jna': '(c || z)', 'ja': '(!c && !z)', 'jnbe': '(!c && !z)',
    'js': 'n', 'jns': '!n', 'jo': 'o', 'jno': '!o',
    'jp': 'p', 'jpe': 'p', 'jnp': '!p', 'jpo': '!p',
    'jl': '(n != o)', 'jnge': '(n != o)', 'jge': '(n == o)', 'jnl': '(n == o)',
    'jle': '(z || n != o)', 'jng': '(z || n != o)',
    'jg': '(!z && n == o)', 'jnle': '(!z && n == o)',
}

# This support code models flat guest RAM explicitly, using unsigned arithmetic
# and byte loads/stores so host alignment and aliasing do not alter semantics.
RUNTIME = r'''/* Generated explicit-state x86 translation support. NEVER COMMIT. */
#ifndef EF_LIFT_STATE_H
#define EF_LIFT_STATE_H
#include <stdint.h>
typedef struct {
    uint32_t r[8];
    uint32_t flags, pc, fault;
    uint8_t *memory;
    uint32_t memory_size;
} ef_lift_state;
static uint32_t ef_mask(unsigned width) {
    return width == 32 ? UINT32_MAX : ((1u << width) - 1u);
}
static uint32_t ef_read(ef_lift_state *s, uint32_t address, unsigned width) {
    unsigned i, bytes = width / 8;
    uint32_t value = 0;
    if (bytes > s->memory_size || address > s->memory_size - bytes) {
        s->fault = 3; return 0;
    }
    for (i = 0; i < bytes; ++i) value |= (uint32_t)s->memory[address+i] << (8*i);
    return value;
}
static void ef_write(ef_lift_state *s, uint32_t address, unsigned width, uint32_t value) {
    unsigned i, bytes = width / 8;
    if (bytes > s->memory_size || address > s->memory_size - bytes) {
        s->fault = 3; return;
    }
    for (i = 0; i < bytes; ++i) s->memory[address+i] = (uint8_t)(value >> (8*i));
}
static void ef_szp(ef_lift_state *s, uint32_t result, unsigned width) {
    uint32_t byte = result & 255u, parity;
    byte ^= byte >> 4; byte &= 15u;
    parity = (0x9669u >> byte) & 1u;
    s->flags = (s->flags & ~0xc4u) | (parity << 2) |
        ((result == 0) << 6) | (((result >> (width-1)) & 1u) << 7);
}
static uint32_t ef_arith(ef_lift_state *s, uint32_t a, uint32_t b,
                         unsigned width, unsigned subtract) {
    uint32_t mask = ef_mask(width), sign = 1u << (width-1), result, carry, overflow;
    a &= mask; b &= mask;
    result = (subtract ? a-b : a+b) & mask;
    carry = subtract ? a < b : (uint64_t)a + b > mask;
    overflow = ((subtract ? (a ^ b) : ~(a ^ b)) & (a ^ result) & sign) != 0;
    s->flags = (s->flags & ~0x811u) | carry | (((a ^ b ^ result) & 16u)) |
        (overflow << 11);
    ef_szp(s, result, width);
    return result;
}
static uint32_t ef_logic(ef_lift_state *s, uint32_t value, unsigned width) {
    value &= ef_mask(width);
    /* AF is architecturally undefined; use the Unicorn oracle's zero policy. */
    s->flags &= ~0x811u;
    ef_szp(s, value, width);
    return value;
}
#endif
'''


class Unsupported(ValueError):
    pass


def address_expression(text):
    """Parse only 32-bit base/index/scale/displacement addressing."""
    text = text.replace(' ', '')
    parts = re.findall(r'[+-]?[^+-]+', text)
    if not parts or ''.join(parts) != text:
        raise Unsupported('unsupported address expression')
    terms = []
    for part in parts:
        negative = part.startswith('-')
        part = part.lstrip('+-')
        if part in REGS:
            term = 's->r[%d]' % REGS.index(part)
        elif re.fullmatch(r'(?:eax|ecx|edx|ebx|esp|ebp|esi|edi)\*[1248]', part):
            register, scale = part.split('*')
            term = '(s->r[%d] * %su)' % (REGS.index(register), scale)
        elif re.fullmatch(r'0x[0-9a-f]+|[0-9]+', part):
            value = int(part, 0)
            if value > 0xffffffff:
                raise Unsupported('address displacement wider than 32 bits')
            term = '0x%xu' % value
        else:
            raise Unsupported('unsupported address term: ' + part)
        terms.append(('-' if negative else '+') + term)
    return '(uint32_t)(0u' + ''.join(terms) + ')'


def operand(text):
    text = text.strip().lower()
    if text in REG:
        return ('reg', *REG[text])
    if re.fullmatch(r'-?(?:0x[0-9a-f]+|[0-9]+)', text):
        value = int(text, 0)
        if not -0x80000000 <= value <= 0xffffffff:
            raise Unsupported('immediate wider than 32 bits')
        return ('imm', value & 0xffffffff)
    match = re.fullmatch(r'(?:(byte|word|dword) ptr )?(?:(ds|ss):)?(?:\[([^]]+)\]|(0x[0-9a-f]+))', text)
    if match:
        size, _, expression, absolute = match.groups()
        width = {'byte': 8, 'word': 16, 'dword': 32}.get(size)
        return ('mem', address_expression(expression or absolute), width)
    raise Unsupported('unsupported operand: ' + text)


def width_of(op):
    return op[2] if op[0] == 'reg' else op[2] if op[0] == 'mem' else None


def read(op, width):
    if op[0] == 'imm':
        return '0x%xu' % (op[1] & ((1 << width) - 1))
    if op[0] == 'reg':
        return '((s->r[%d] >> %d) & 0x%xu)' % (op[1], op[3], (1 << op[2])-1)
    return 'ef_read(s, %s, %d)' % (op[1], width)


def write(op, value, width):
    if op[0] == 'reg':
        index, bits, shift = op[1:]
        if bits != width:
            raise Unsupported('register width mismatch')
        mask = ((1 << bits)-1) << shift
        return 's->r[%d] = (s->r[%d] & 0x%xu) | (((%s) & 0x%xu) << %d);' % (
            index, index, ~mask & 0xffffffff, value, (1 << bits)-1, shift)
    if op[0] == 'mem':
        return 'ef_write(s, %s, %d, %s);' % (op[1], width, value)
    raise Unsupported('immediate destination')


def instruction_c(insn, starts):
    mnemonic, text = insn['mnemonic'], insn['operands']
    next_pc = insn['address'] + len(bytes.fromhex(insn['bytes']))
    if mnemonic in CONDITIONS or mnemonic == 'jmp':
        target = insn.get('direct_target')
        if target not in starts:
            raise Unsupported('unresolved or external branch')
        if mnemonic == 'jmp':
            return 's->pc = 0x%xu; break;' % target
        condition = CONDITIONS[mnemonic]
        for flag, mask in [('z', 0x40), ('c', 1), ('n', 0x80), ('o', 0x800), ('p', 4)]:
            condition = re.sub(r'\b' + flag + r'\b', '((s->flags & 0x%xu) != 0)' % mask, condition)
        if next_pc not in starts:
            raise Unsupported('fallthrough outside body')
        return 's->pc = (%s) ? 0x%xu : 0x%xu; break;' % (condition, target, next_pc)
    if mnemonic == 'ret':
        if text and not re.fullmatch(r'0x[0-9a-f]+|[0-9]+', text):
            raise Unsupported('unsupported return')
        cleanup = int(text, 0) if text else 0
        if cleanup > 65535:
            raise Unsupported('invalid return cleanup')
        return ('s->pc = ef_read(s, s->r[4], 32); if (s->fault) return s->fault; '
                's->r[4] += %du; return 0;' % (4 + cleanup))
    if next_pc not in starts:
        raise Unsupported('fallthrough outside body')
    code = []
    # Only these instruction spellings are supported. Prefixes, x87, calls,
    # implicit segment operations and partial architectural models fail closed.
    if mnemonic == 'nop' and not text:
        pass
    elif mnemonic in ('clc', 'stc', 'cmc') and not text:
        code.append({'clc': 's->flags &= ~1u;', 'stc': 's->flags |= 1u;',
                     'cmc': 's->flags ^= 1u;'}[mnemonic])
    elif mnemonic == 'leave' and not text:
        code += ['a = ef_read(s, s->r[5], 32); if (s->fault) return s->fault;',
                 's->r[4] = s->r[5] + 4u; s->r[5] = a;']
    elif mnemonic in ('push', 'pop'):
        op = operand(text)
        if mnemonic == 'push' and (width_of(op) == 32 or op[0] == 'imm'):
            code += ['a = %s; if (s->fault) return s->fault;' % read(op, 32),
                     'ef_write(s, s->r[4] - 4u, 32, a);',
                     'if (s->fault) return s->fault; s->r[4] -= 4u;']
        elif mnemonic == 'pop' and op[0] == 'reg' and width_of(op) == 32:
            code += ['a = ef_read(s, s->r[4], 32); if (s->fault) return s->fault;',
                     's->r[4] += 4u;', write(op, 'a', 32)]
        else:
            raise Unsupported('unsupported stack operand')
    elif mnemonic in ('mov', 'lea', 'movzx', 'movsx', 'add', 'sub', 'cmp',
                       'and', 'or', 'xor', 'test', 'inc', 'dec', 'neg', 'not'):
        operands = [operand(part) for part in text.split(',')]
        unary = mnemonic in ('inc', 'dec', 'neg', 'not')
        if len(operands) != (1 if unary else 2):
            raise Unsupported('operand count')
        dst = operands[0]
        width = width_of(dst)
        if dst[0] == 'imm' or width not in (8, 16, 32):
            raise Unsupported('ambiguous destination width')
        if mnemonic == 'lea':
            if dst[0] != 'reg' or width != 32 or operands[1][0] != 'mem':
                raise Unsupported('unsupported lea')
            code.append(write(dst, operands[1][1], width))
        else:
            src = operands[1] if not unary else None
            if src and src[0] == dst[0] == 'mem':
                raise Unsupported('memory to memory operation')
            src_width = width_of(src) if src else None
            if mnemonic in ('movzx', 'movsx'):
                if dst[0] != 'reg' or src_width not in (8, 16) or src_width >= width:
                    raise Unsupported('unsupported extension widths')
            elif src_width is not None and src_width != width:
                raise Unsupported('operand width mismatch')
            if mnemonic not in ('mov', 'movzx', 'movsx'):
                code.append('a = %s;' % read(dst, width))
            if src:
                code.append('b = %s;' % read(src, src_width or width))
            code.append('if (s->fault) return s->fault;')
            if mnemonic in ('mov', 'movzx'):
                value = 'b'
            elif mnemonic == 'movsx':
                value = '((b ^ 0x%xu) - 0x%xu)' % (1 << (src_width-1), 1 << (src_width-1))
            elif mnemonic in ('add', 'sub', 'cmp', 'neg', 'inc', 'dec'):
                left = '0u' if mnemonic == 'neg' else 'a'
                right = 'a' if mnemonic == 'neg' else '1u' if unary else 'b'
                subtract = mnemonic in ('sub', 'cmp', 'neg', 'dec')
                if mnemonic in ('inc', 'dec'):
                    code.append('carry = s->flags & 1u;')
                code.append(('%s ef_arith(s, %%s, %%s, %%d, %%d);' %
                             ('(void)' if mnemonic == 'cmp' else 'result =')) %
                            (left, right, width, subtract))
                if mnemonic in ('inc', 'dec'):
                    code.append('s->flags = (s->flags & ~1u) | carry;')
                value = 'result'
            elif mnemonic == 'not':
                value = '~a'
            else:
                operation = {'and': '&', 'test': '&', 'or': '|', 'xor': '^'}[mnemonic]
                code.append(('%s ef_logic(s, a %%s b, %%d);' %
                             ('(void)' if mnemonic == 'test' else 'result =')) % (operation, width))
                value = 'result'
            if mnemonic not in ('cmp', 'test'):
                code.append(write(dst, value, width))
    else:
        raise Unsupported('unsupported instruction: ' + mnemonic)
    code += ['if (s->fault) return s->fault;', 's->pc = 0x%xu; break;' % next_pc]
    return ' '.join(code)


def translate(record):
    if (record.get('boundary') != 'ghidra-body-ranges' or
            record.get('ghidra_instruction_layout_matches') is not True or
            record.get('overlapping_entries') or record.get('undecodable_addresses')):
        raise Unsupported('body or instruction layout not established')
    instructions = record['instructions']
    starts = {i['address'] for i in instructions}
    if record['address'] not in starts:
        raise Unsupported('entry is not an instruction start')
    cases = ['case 0x%xu: %s' % (i['address'], instruction_c(i, starts)) for i in instructions]
    variables = [name for name in ('a', 'b', 'result', 'carry')
                 if re.search(r'\b' + name + r'\b', '\n'.join(cases))]
    declarations = 'uint32_t ' + ', '.join(variables) + ';' if variables else ''
    return '''/* Mechanical ASM-derived draft. NEVER COMMIT. Flat 32-bit segments only. */
#include "ef_lift_state.h"
unsigned ef_lift_%08x(ef_lift_state *s, uint32_t fuel) {
    %s
    if (s->fault) return s->fault;
    while (fuel--) {
        switch (s->pc) {
%s
        default: return 2;
        }
    }
    return 1;
}
''' % (record['address'], declarations, '\n'.join(cases))


def generate(inventory, output, image):
    payload = image.read_bytes()
    manifest = json.loads((inventory / 'manifest.json').read_text())
    if (manifest.get('schema') != 3 or manifest.get('boundary') != 'ghidra-body-ranges' or
            manifest.get('payload_sha256') != hashlib.sha256(payload).hexdigest()):
        raise ValueError('invalid inventory provenance')
    output.mkdir(parents=True, exist_ok=False)
    (output / 'ef_lift_state.h').write_text(RUNTIME)
    report = []
    for entry in manifest['entries']:
        path = inventory / entry['file']
        record = json.loads(path.read_text())
        # Recheck every encoded instruction against the caller-validated image.
        # A modified record must not inherit provenance merely from a manifest.
        for insn in record['instructions']:
            data = bytes.fromhex(insn['bytes'])
            site = insn['address']
            if not data or site < 0 or data != payload[site:site+len(data)]:
                raise ValueError('instruction payload mismatch')

        decoded = []
        for start, end in record['ranges']:
            decoded.extend(asm_inventory.decode_window(image, start, end-start))
        if decoded != record['instructions'] or record['address'] != entry['address']:
            raise ValueError('instruction decode mismatch')
        item = dict(address=entry['address'], state='unsupported', reason='')
        try:
            code = translate(record)
        except Unsupported as error:
            item['reason'] = str(error)
        else:
            item['state'] = 'generated-unverified'
            item['file'] = 'ef_lift_%08x.c' % entry['address']
            (output / item['file']).write_text(code)
            item['source_sha256'] = hashlib.sha256(code.encode()).hexdigest()
        report.append(item)
    result = dict(schema=1, payload_sha256=manifest['payload_sha256'],
                  inventory_sha256=hashlib.sha256((inventory / 'manifest.json').read_bytes()).hexdigest(),
                  model='flat-x86-explicit-state',
                  runtime_sha256=hashlib.sha256(RUNTIME.encode()).hexdigest(), entries=report)
    (output / 'report.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inventory', type=Path, required=True)
    parser.add_argument('--workspace', type=Path, default=ROOT / 'local/ghidra')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        output = args.output.resolve()
        if not output.is_relative_to(ROOT / 'local'):
            raise ValueError('generated ASM-derived C must stay under ignored local/')
        workspace = asm_inventory.ghidra_ef2000.validated_workspace(args.workspace)
        report = generate(args.inventory, output,
                          workspace / asm_inventory.ghidra_ef2000.IMAGE_NAME)
        print(dict(Counter(e['state'] for e in report['entries'])))
    except (OSError, ValueError) as error:
        parser.exit(1, f'asm_to_c: {error}\n')


if __name__ == '__main__':
    main()
