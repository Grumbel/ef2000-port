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
    from . import asm_inventory, asm_x87
except ImportError:
    import asm_inventory
    import asm_x87

ROOT = Path(__file__).resolve().parents[1]
REGS = ('eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi')
SEGMENTS = ('es', 'cs', 'ss', 'ds', 'fs', 'gs')
STATE_ABI = 2
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
    uint32_t segment[6]; /* ES, CS, SS, DS, FS, GS: validated zero-base aliases. */
    uint32_t x87[8][3]; /* Physical registers: significand low/high, sign/exponent. */
    uint32_t x87_control, x87_status, x87_tag;
    uint32_t x87_ip, x87_cs, x87_dp, x87_ds, x87_opcode;
} ef_lift_state;
static unsigned ef_segments_valid(ef_lift_state *s) {
    unsigned i;
    for (i = 0; i < 6; ++i) {
        uint32_t selector = s->segment[i];
        if (i == 1) { if (selector != 0 && selector != 8) return 0; }
        else if (i == 2) { if (selector != 0 && selector != 16 && selector != 24) return 0; }
        else if (selector > 3 && selector != 8 && selector != 16 && selector != 24) return 0;
    }
    return 1;
}
static void ef_load_segment(ef_lift_state *s, unsigned index, uint32_t selector) {
    selector &= 0xffffu;
    if (index == 1 || (index == 2 && selector != 16 && selector != 24) ||
        (selector > 3 && selector != 8 && selector != 16 && selector != 24)) {
        s->fault = 7; return;
    }
    s->segment[index] = selector;
}
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
static uint32_t ef_arith_carry(ef_lift_state *s, uint32_t a, uint32_t b,
                               unsigned width, unsigned subtract, uint32_t input_carry) {
    uint32_t mask = ef_mask(width), sign = 1u << (width-1), result, carry, overflow;
    a &= mask; b &= mask;
    result = (subtract ? a-b-input_carry : a+b+input_carry) & mask;
    carry = subtract ? (uint64_t)a < (uint64_t)b + input_carry :
        (uint64_t)a + b + input_carry > mask;
    overflow = ((subtract ? (a ^ b) : ~(a ^ b)) & (a ^ result) & sign) != 0;
    s->flags = (s->flags & ~0x811u) | carry | (((a ^ b ^ result) & 16u)) |
        (overflow << 11);
    ef_szp(s, result, width);
    return result;
}
static uint32_t ef_arith(ef_lift_state *s, uint32_t a, uint32_t b,
                         unsigned width, unsigned subtract) {
    return ef_arith_carry(s, a, b, width, subtract, 0);
}
static uint32_t ef_logic(ef_lift_state *s, uint32_t value, unsigned width) {
    value &= ef_mask(width);
    /* AF is architecturally undefined; use the Unicorn oracle's zero policy. */
    s->flags &= ~0x811u;
    ef_szp(s, value, width);
    return value;
}
static int64_t ef_signed(uint32_t value, unsigned width) {
    value &= ef_mask(width);
    return (int64_t)value - ((value & (1u << (width-1))) ?
                            (int64_t)((uint64_t)1 << width) : 0);
}
static uint32_t ef_shift(ef_lift_state *s, uint32_t value, uint32_t count,
                         unsigned width, unsigned operation) {
    uint32_t mask = ef_mask(width), sign = 1u << (width-1), carry = 0, overflow = 0;
    count &= 31u; value &= mask;
    if (!count) return value;
    /* Iterate at most 31 times; never shift a C value by its type width.
       Undefined AF/OF follow the Unicorn oracle rather than being ignored. */
    while (count--) {
        if (operation == 0) {
            carry = (value & sign) != 0;
            value = (value << 1) & mask;
            overflow = ((value & sign) != 0) ^ carry;
        } else {
            carry = value & 1u;
            overflow = operation == 1 && (value & sign) != 0;
            value = (value >> 1) | (operation == 2 ? value & sign : 0);
        }
    }
    s->flags = (s->flags & ~0x811u) | carry | (overflow << 11);
    ef_szp(s, value, width);
    return value;
}
static uint32_t ef_double_shift(ef_lift_state *s, uint32_t value, uint32_t source,
                                uint32_t count, unsigned width, unsigned right) {
    uint32_t mask = ef_mask(width), sign = 1u << (width-1), carry = 0, overflow = 0, before;
    value &= mask; source &= mask; count &= 31u;
    if (!count) return value;
    /* Rotate the concatenated pair in bounded one-bit steps. This also
       preserves the oracle's undefined 16-bit count>16 results and OF/AF. */
    while (count--) {
        before = value;
        if (right) {
            carry = value & 1u;
            value = (value >> 1) | ((source & 1u) << (width-1));
            source = (source >> 1) | (carry << (width-1));
        } else {
            carry = (value & sign) != 0;
            value = ((value << 1) | (source >> (width-1))) & mask;
            source = ((source << 1) | carry) & mask;
        }
        overflow = ((before ^ value) & sign) != 0;
    }
    s->flags = (s->flags & ~0x811u) | carry | (overflow << 11);
    ef_szp(s, value, width);
    return value;
}
static uint64_t ef_multiply(ef_lift_state *s, uint32_t a, uint32_t b,
                            unsigned width, unsigned signed_operation) {
    uint64_t product;
    int64_t signed_product;
    uint32_t low, overflow;
    a &= ef_mask(width); b &= ef_mask(width);
    if (signed_operation) {
        signed_product = ef_signed(a, width) * ef_signed(b, width);
        product = (uint64_t)signed_product;
        low = (uint32_t)product & ef_mask(width);
        overflow = signed_product != ef_signed(low, width);
    } else {
        product = (uint64_t)a * b;
        low = (uint32_t)product & ef_mask(width);
        overflow = (product >> width) != 0;
    }
    /* SF/ZF/PF/AF are undefined by x86 here. Retain the oracle's policy. */
    (void)ef_logic(s, low, width);
    if (overflow) s->flags |= 0x801u;
    return product;
}
static uint32_t ef_rotate(ef_lift_state *s, uint32_t value, uint32_t count,
                          unsigned width, unsigned operation) {
    uint32_t carry = s->flags & 1u, next_carry, overflow, original_sign;
    value &= ef_mask(width); count &= 31u;
    if (operation >= 2) count %= width + 1;
    if (!count) return value;
    original_sign = value >> (width-1);
    while (count--) {
        if (!(operation & 1u)) {
            next_carry = value >> (width-1);
            value = ((value << 1) | (operation == 2 ? carry : next_carry)) & ef_mask(width);
        } else {
            next_carry = value & 1u;
            value = (value >> 1) | ((operation == 3 ? carry : next_carry) << (width-1));
        }
        carry = next_carry;
    }
    /* Include the oracle's undefined multi-bit OF policy in comparisons. */
    overflow = operation >= 2 ? original_sign ^ (value >> (width-1)) :
        operation == 0 ? (value >> (width-1)) ^ carry :
        ((value >> (width-1)) ^ (value >> (width-2))) & 1u;
    s->flags = (s->flags & ~0x801u) | carry | (overflow << 11);
    return value;
}
static void ef_divide(ef_lift_state *s, uint32_t divisor, unsigned width,
                       unsigned signed_operation) {
    uint64_t dividend, quotient, remainder;
    int64_t numerator, denominator, signed_quotient, signed_remainder, limit;
    uint32_t mask = ef_mask(width);
    divisor &= mask;
    if (!divisor) { s->fault = 5; return; }
    dividend = width == 8 ? s->r[0] & 0xffffu :
        ((uint64_t)(s->r[2] & mask) << width) | (s->r[0] & mask);
    if (signed_operation) {
        numerator = width == 32 ? ((dividend >> 63) ?
            -1 - (int64_t)(~dividend) : (int64_t)dividend) :
            ef_signed((uint32_t)dividend, width*2);
        denominator = ef_signed(divisor, width);
        if (numerator == INT64_MIN && denominator == -1) { s->fault = 5; return; }
        signed_quotient = numerator / denominator;
        signed_remainder = numerator % denominator;
        limit = (int64_t)1 << (width-1);
        if (signed_quotient < -limit || signed_quotient >= limit) { s->fault = 5; return; }
        quotient = (uint64_t)signed_quotient;
        remainder = (uint64_t)signed_remainder;
    } else {
        quotient = dividend / divisor; remainder = dividend % divisor;
        if (quotient > mask) { s->fault = 5; return; }
    }
    /* Division's undefined flags are preserved by the Unicorn oracle. */
    if (width == 8) {
        s->r[0] = (s->r[0] & 0xffff0000u) | (uint32_t)(quotient & 255u) |
            ((uint32_t)(remainder & 255u) << 8);
    } else {
        s->r[0] = (s->r[0] & ~mask) | (uint32_t)(quotient & mask);
        s->r[2] = (s->r[2] & ~mask) | (uint32_t)(remainder & mask);
    }
}
#endif
'''
RUNTIME = RUNTIME.replace('\n#endif\n', '\n' + asm_x87.STACK_RUNTIME + asm_x87.MEMORY80_RUNTIME +
                          asm_x87.REAL_LOAD_RUNTIME + asm_x87.INTEGER_LOAD_RUNTIME + '\n#endif\n')
RUNTIME += asm_x87.NARROW_RUNTIME + asm_x87.BINARY_RUNTIME


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
    if text in SEGMENTS:
        return ('seg', SEGMENTS.index(text), 16)
    if re.fullmatch(r'-?(?:0x[0-9a-f]+|[0-9]+)', text):
        value = int(text, 0)
        if not -0x80000000 <= value <= 0xffffffff:
            raise Unsupported('immediate wider than 32 bits')
        return ('imm', value & 0xffffffff)
    match = re.fullmatch(r'(?:(byte|word|dword|fword|qword|tbyte) ptr )?(?:(es|cs|ss|ds|fs|gs):)?(?:\[([^]]+)\]|(0x[0-9a-f]+))', text)
    if match:
        size, _, expression, absolute = match.groups()
        width = {'byte': 8, 'word': 16, 'dword': 32, 'fword': 48, 'qword': 64, 'tbyte': 80}.get(size)
        return ('mem', address_expression(expression or absolute), width)
    raise Unsupported('unsupported operand: ' + text)


def width_of(op):
    return op[2] if op[0] in ('reg', 'mem', 'seg') else None


def read(op, width):
    if op[0] == 'imm':
        return '0x%xu' % (op[1] & ((1 << width) - 1))
    if op[0] == 'reg':
        return '((s->r[%d] >> %d) & 0x%xu)' % (op[1], op[3], (1 << op[2])-1)
    if op[0] == 'seg':
        return '(s->segment[%d] & 0xffffu)' % op[1]
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


def condition_c(mnemonic):
    condition = CONDITIONS[mnemonic]
    for flag, mask in [('z', 0x40), ('c', 1), ('n', 0x80), ('o', 0x800), ('p', 4)]:
        condition = re.sub(r'\b' + flag + r'\b', '((s->flags & 0x%xu) != 0)' % mask, condition)
    return condition


def mutable_immediate(insn):
    """Prove a data suffix has a fixed operation, register and encoded width."""
    if insn['mnemonic'] not in ('mov', 'add', 'or', 'adc', 'sbb', 'and', 'sub', 'xor', 'cmp',
                                 'rol', 'ror', 'rcl', 'rcr', 'shl', 'shr', 'sar'):
        return None
    arguments = [operand(part) for part in insn['operands'].split(',')]
    if len(arguments) != 2 or arguments[0][0] != 'reg' or arguments[1][0] != 'imm':
        return None
    encoded = bytes.fromhex(insn['bytes'])
    prefix = 1 if encoded[0] == 0x66 else 0
    opcode = encoded[prefix]
    signed = False
    if 0xb0 <= opcode <= 0xb7 and insn['mnemonic'] == 'mov':
        index = opcode - 0xb0
        expected = ('reg', index % 4, 8, 8 if index >= 4 else 0)
        offset, width = prefix+1, 8
    elif 0xb8 <= opcode <= 0xbf and insn['mnemonic'] == 'mov':
        expected = ('reg', opcode-0xb8, 16 if prefix else 32, 0)
        offset, width = prefix+1, expected[2]
    elif opcode in (0x80, 0x81, 0x83, 0xc0, 0xc1) and len(encoded) >= prefix+3:
        modrm = encoded[prefix+1]
        if modrm & 0xc0 != 0xc0:
            return None
        group, index = (modrm >> 3) & 7, modrm & 7
        operations = (('add', 'or', 'adc', 'sbb', 'and', 'sub', 'xor', 'cmp')
                      if opcode in (0x80, 0x81, 0x83) else
                      ('rol', 'ror', 'rcl', 'rcr', 'shl', 'shr', None, 'sar'))
        if insn['mnemonic'] != operations[group]:
            return None
        bits = 8 if opcode in (0x80, 0xc0) else 16 if prefix else 32
        expected = ('reg', index % 4 if bits == 8 else index, bits, 8 if bits == 8 and index >= 4 else 0)
        offset, width = prefix+2, bits if opcode == 0x81 else 8
        signed = opcode == 0x83
    else:
        return None
    if arguments[0] != expected or len(encoded) != offset + width//8:
        return None
    value = int.from_bytes(encoded[offset:], 'little')
    if signed and value & 0x80:
        value -= 256
    if value & ((1 << expected[2])-1) != arguments[1][1] & ((1 << expected[2])-1):
        return None
    return offset, width, signed


def immediate_expression(insn):
    field = mutable_immediate(insn)
    if field is None:
        return None
    offset, width, signed = field
    expression = 'ef_read(s, 0x%xu, %d)' % (insn['address'] + offset, width)
    return '((%s ^ 0x80u) - 0x80u)' % expression if signed else expression


def string_instruction_c(mnemonic, text, repeat, pc, next_pc):
    arguments = [operand(part) for part in text.split(',')]
    if len(arguments) != 2:
        raise Unsupported('unsupported string operands')
    first, second = arguments
    memory = first if first[0] == 'mem' else second
    width = width_of(memory)
    if width not in (8, 16, 32):
        raise Unsupported('unsupported string width')
    source, destination = address_expression('esi'), address_expression('edi')
    accumulator = operand({8: 'al', 16: 'ax', 32: 'eax'}[width])
    def mem_at(op, address):
        return op[0] == 'mem' and op[1] == address and width_of(op) == width
    valid = {
        'movs': mem_at(first, destination) and mem_at(second, source),
        'cmps': mem_at(first, source) and mem_at(second, destination),
        'stos': mem_at(first, destination) and second == accumulator,
        'lods': first == accumulator and mem_at(second, source),
        'scas': first == accumulator and mem_at(second, destination),
    }
    if not valid.get(mnemonic):
        raise Unsupported('unsupported string addressing')
    code = []
    if repeat:
        code.append('if (!s->r[1]) { s->pc = 0x%xu; break; }' % next_pc)
    if mnemonic in ('movs', 'lods', 'cmps'):
        code.append('a = ef_read(s, s->r[6], %d);' % width)
    elif mnemonic in ('stos', 'scas'):
        code.append('a = %s;' % read(accumulator, width))
    if mnemonic in ('cmps', 'scas'):
        code.append('b = ef_read(s, s->r[7], %d);' % width)
    code.append('if (s->fault) return s->fault;')
    if mnemonic in ('movs', 'stos'):
        code += ['ef_write(s, s->r[7], %d, a);' % width, 'if (s->fault) return s->fault;']
    elif mnemonic == 'lods':
        code.append(write(accumulator, 'a', width))
    else:
        code.append('(void)ef_arith(s, a, b, %d, 1);' % width)
    step = '((s->flags & 0x400u) ? 0u-%du : %du)' % (width//8, width//8)
    if mnemonic in ('movs', 'lods', 'cmps'):
        code.append('s->r[6] += %s;' % step)
    if mnemonic in ('movs', 'stos', 'cmps', 'scas'):
        code.append('s->r[7] += %s;' % step)
    if repeat:
        code.append('--s->r[1];')
        if mnemonic in ('cmps', 'scas'):
            stop = '!=' if repeat == 'repnz' else '=='
            code.append('if ((s->flags & 0x40u) %s 0) { s->pc = 0x%xu; break; }' % (stop, next_pc))
        # Unicorn counts the terminal zero-count REP check as another step.
        code.append('s->pc = 0x%xu; break;' % pc)
    else:
        code.append('s->pc = 0x%xu; break;' % next_pc)
    return ' '.join(code)


def instruction_c(insn, starts):
    mnemonic, text = insn['mnemonic'], insn['operands']
    next_pc = insn['address'] + len(bytes.fromhex(insn['bytes']))
    if mnemonic in ('loop', 'loope', 'loopz', 'loopne', 'loopnz', 'jecxz', 'jcxz'):
        target = insn.get('direct_target')
        if target not in starts or next_pc not in starts:
            raise Unsupported('unresolved count branch or fallthrough')
        if mnemonic in ('jecxz', 'jcxz'):
            prefix = ''
            condition = '(s->r[1] & 0xffffu) == 0' if mnemonic == 'jcxz' else 's->r[1] == 0'
        else:
            prefix = 's->r[1] -= 1u; '
            condition = 's->r[1] != 0'
            if mnemonic in ('loope', 'loopz'):
                condition += ' && (s->flags & 0x40u)'
            elif mnemonic in ('loopne', 'loopnz'):
                condition += ' && !(s->flags & 0x40u)'
        return prefix + 's->pc = (%s) ? 0x%xu : 0x%xu; break;' % (condition, target, next_pc)
    if mnemonic in CONDITIONS or mnemonic == 'jmp':
        target = insn.get('direct_target')
        if target not in starts:
            raise Unsupported('unresolved or external branch')
        if mnemonic == 'jmp':
            return 's->pc = 0x%xu; break;' % target
        condition = condition_c(mnemonic)
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
    repeat = None
    if mnemonic in ('rep', 'repz', 'repnz'):
        repeat = mnemonic
        parts = text.split(None, 1)
        mnemonic, text = parts[0], parts[1] if len(parts) > 1 else ''
    if mnemonic in ('movs', 'cmps', 'stos', 'lods', 'scas'):
        return string_instruction_c(mnemonic, text, repeat, insn['address'], next_pc)
    if repeat:
        raise Unsupported('unsupported repeated instruction: ' + mnemonic)
    code = []
    # Only these instruction spellings are supported. Prefixes, x87, calls,
    # implicit segment operations and partial architectural models fail closed.
    stack_operation = asm_x87.stack_operation(mnemonic, text)
    memory_binary = mnemonic in ('fadd', 'fmul', 'fsub', 'fsubr', 'fdiv', 'fdivr') and text.strip().lower().startswith(('dword ptr ', 'qword ptr '))
    try:
        binary_operation = None if memory_binary else asm_x87.binary_register(insn)
    except ValueError as error:
        raise Unsupported(str(error)) from error
    real_load = mnemonic == 'fld' and text.strip().lower().startswith(('dword ptr ', 'qword ptr '))
    integer_load = mnemonic == 'fild' and text.strip().lower().startswith(('word ptr ', 'dword ptr ', 'qword ptr '))
    narrow_store = mnemonic in ('fst', 'fstp') and text.strip().lower().startswith(('dword ptr ', 'qword ptr '))
    if memory_binary or narrow_store or integer_load or real_load or (mnemonic in ('fld', 'fstp') and text.strip().lower().startswith('tbyte ptr ')):
        op = operand(text)
        try:
            if memory_binary:
                encoding = 0xd8 if width_of(op) == 32 else 0xdc
                group, operation, reverse = {'fadd': (0, 0, 0), 'fmul': (1, 2, 0),
                    'fsub': (4, 1, 0), 'fsubr': (5, 1, 1), 'fdiv': (6, 3, 0), 'fdivr': (7, 3, 1)}[mnemonic]
                helper = 'ef_x87_memory_binary(s, b, %d, %d, %d)' % (width_of(op), operation, reverse)
            elif narrow_store:
                encoding, group = (0xd9 if width_of(op) == 32 else 0xdd), (3 if mnemonic == 'fstp' else 2)
                helper = 'ef_x87_store_real(s, b, %d, %d)' % (width_of(op), mnemonic == 'fstp')
            elif integer_load:
                encoding, group = (0xdb if width_of(op) == 32 else 0xdf), (5 if width_of(op) == 64 else 0)
                helper = 'ef_x87_load_integer(s, b, %d)' % width_of(op)
            elif real_load:
                encoding, group = (0xd9 if width_of(op) == 32 else 0xdd), 0
                helper = 'ef_x87_load_real(s, b, %d)' % width_of(op)
            else:
                encoding, group = 0xdb, (5 if mnemonic == 'fld' else 7)
                helper = 'ef_x87_memory80(s, b, %d)' % (mnemonic == 'fld')
            segment, opcode = asm_x87.memory_encoding(insn, encoding, group)
        except ValueError as error:
            raise Unsupported(str(error)) from error
        code += ['b = %s;' % op[1],
                 'a = %s; if (a) return a;' % helper,
                 's->x87_ip = 0x%xu; s->x87_cs = s->segment[1]; s->x87_opcode = 0x%xu;' %
                 (insn['address'], opcode),
                 's->x87_dp = b; s->x87_ds = s->segment[%d];' % segment]
    elif binary_operation is not None:
        operation, left, right, destination, pop, opcode = binary_operation
        code += ['a = ef_x87_register_binary(s, %d, %d, %d, %d, %d); if (a) return a;' %
                 (operation, left, right, destination, pop),
                 's->x87_ip = 0x%xu; s->x87_cs = s->segment[1]; s->x87_opcode = 0x%xu;' %
                 (insn['address'], opcode)]
    elif stack_operation is not None:
        operation, argument = stack_operation
        encoded = bytes.fromhex(insn['bytes'])
        if len(encoded) != 2 or not 0xd8 <= encoded[0] <= 0xdf or encoded[1] < 0xc0:
            raise Unsupported('unsupported x87 stack encoding')
        code += ['a = ef_x87_stack(s, %d, %d); if (a) return a;' % (operation, argument),
                 's->x87_ip = 0x%xu; s->x87_cs = s->segment[1]; s->x87_opcode = 0x%xu;' %
                 (insn['address'], ((encoded[0] & 7) << 8) | encoded[1])]
    elif mnemonic == 'fninit' and not text:
        # FNINIT marks registers empty but preserves their physical contents.
        code += ['s->x87_control = 0x37fu; s->x87_status = 0; s->x87_tag = 0xffffu;',
                 's->x87_ip = 0; s->x87_cs = 0; s->x87_dp = 0; s->x87_ds = 0; s->x87_opcode = 0;']
    elif mnemonic == 'fnclex' and not text:
        code.append('s->x87_status &= 0x7f00u;')
    elif mnemonic in ('fnstcw', 'fnstsw', 'fldcw'):
        op = operand(text)
        if mnemonic == 'fnstsw' and op == operand('ax'):
            code.append(write(op, 's->x87_status', 16))
        elif op[0] == 'mem' and width_of(op) in (None, 16):
            if mnemonic == 'fldcw':
                code += ['if (s->x87_status & 0x80u) return 8;',
                         'a = %s; if (s->fault) return s->fault;' % read(op, 16),
                         's->x87_control = (a & 0x1f3fu) | 0x40u;',
                         's->x87_status = (s->x87_status & ~0x8080u) | '
                         '((s->x87_status & ~s->x87_control & 0x3fu) ? 0x8080u : 0u);']
            else:
                code.append(write(op, 's->x87_' + ('control' if mnemonic == 'fnstcw' else 'status'), 16))
        else:
            raise Unsupported('unsupported x87 control operand')
    elif mnemonic == 'sahf' and not text:
        code.append('s->flags = (s->flags & ~0xd5u) | ((s->r[0] >> 8) & 0xd5u);')
    elif mnemonic == 'lahf' and not text:
        code.append(write(operand('ah'), '(s->flags & 0xd5u) | 2u', 8))
    elif mnemonic == 'nop' and not text:
        pass
    elif mnemonic == 'mov' and any(part.strip().lower() in SEGMENTS for part in text.split(',')):
        arguments = [operand(part) for part in text.split(',')]
        if len(arguments) != 2:
            raise Unsupported('segment MOV operand count')
        dst, src = arguments
        if dst[0] == 'seg' and dst[1] != 1 and src[0] in ('reg', 'mem') and width_of(src) in (None, 16, 32):
            code += ['a = %s; if (s->fault) return s->fault;' % read(src, 16),
                     'ef_load_segment(s, %d, a);' % dst[1]]
        elif src[0] == 'seg' and dst[0] in ('reg', 'mem') and width_of(dst) in (None, 16, 32):
            width = width_of(dst) if dst[0] == 'reg' else 16
            code.append(write(dst, read(src, 16), width))
        else:
            raise Unsupported('unsupported segment MOV')
    elif mnemonic in ('les', 'lds', 'lss', 'lfs', 'lgs'):
        arguments = [operand(part) for part in text.split(',')]
        if (len(arguments) != 2 or arguments[0][0] != 'reg' or arguments[1][0] != 'mem' or
                width_of(arguments[0]) not in (16, 32)):
            raise Unsupported('unsupported far-pointer load')
        dst, src = arguments
        width = width_of(dst)
        code += ['a = ef_read(s, %s, %d);' % (src[1], width),
                 'b = ef_read(s, (uint32_t)(%s + %du), 16);' % (src[1], width//8),
                 'if (s->fault) return s->fault;',
                 'ef_load_segment(s, %d, b);' % SEGMENTS.index(mnemonic[1] + 's'),
                 'if (s->fault) return s->fault;', write(dst, 'a', width)]
    elif mnemonic.startswith('set') and 'j' + mnemonic[3:] in CONDITIONS:
        dst = operand(text)
        if dst[0] not in ('reg', 'mem') or width_of(dst) not in (None, 8):
            raise Unsupported('unsupported SETcc destination')
        code.append(write(dst, '((%s) ? 1u : 0u)' % condition_c('j' + mnemonic[3:]), 8))
    elif mnemonic == 'xlat':
        source = operand(text)
        if source[0] != 'mem' or source[1] != address_expression('ebx') or width_of(source) != 8:
            raise Unsupported('unsupported XLAT addressing')
        code += ['a = ef_read(s, (uint32_t)(s->r[3] + (s->r[0] & 255u)), 8);',
                 'if (s->fault) return s->fault;', write(operand('al'), 'a', 8)]
    elif mnemonic == 'bswap':
        dst = operand(text)
        if dst[0] != 'reg' or width_of(dst) != 32:
            raise Unsupported('unsupported BSWAP destination')
        code += ['a = %s;' % read(dst, 32), write(dst,
                 '((a & 255u) << 24) | ((a & 0xff00u) << 8) | ((a >> 8) & 0xff00u) | (a >> 24)', 32)]
    elif mnemonic == 'xchg':
        arguments = [operand(part) for part in text.split(',')]
        if (len(arguments) != 2 or any(op[0] not in ('reg', 'mem') for op in arguments) or
                all(op[0] == 'mem' for op in arguments)):
            raise Unsupported('unsupported XCHG operands')
        left, right = arguments
        width = width_of(left) or width_of(right)
        if width not in (8, 16, 32) or any(width_of(op) not in (None, width) for op in arguments):
            raise Unsupported('unsupported XCHG widths')
        code += ['a = %s; b = %s;' % (read(left, width), read(right, width)),
                 'if (s->fault) return s->fault;']
        # Write memory before changing a register used to calculate its address.
        if right[0] == 'mem':
            code += [write(right, 'a', width), 'if (s->fault) return s->fault;', write(left, 'b', width)]
        else:
            code += [write(left, 'b', width), 'if (s->fault) return s->fault;', write(right, 'a', width)]
    elif mnemonic in ('clc', 'stc', 'cmc', 'cld', 'std') and not text:
        code.append({'clc': 's->flags &= ~1u;', 'stc': 's->flags |= 1u;',
                     'cmc': 's->flags ^= 1u;', 'cld': 's->flags &= ~0x400u;',
                     'std': 's->flags |= 0x400u;'}[mnemonic])
    elif mnemonic in ('cbw', 'cwde', 'cwd', 'cdq') and not text:
        source_width = 8 if mnemonic == 'cbw' else 16 if mnemonic in ('cwde', 'cwd') else 32
        destination = operand('ax' if mnemonic == 'cbw' else 'eax' if mnemonic == 'cwde'
                              else 'dx' if mnemonic == 'cwd' else 'edx')
        value = ('(uint32_t)ef_signed(s->r[0], %d)' % source_width
                 if mnemonic in ('cbw', 'cwde') else
                 '(0u - ((s->r[0] >> %d) & 1u))' % (source_width-1))
        code.append(write(destination, value, width_of(destination)))
    elif mnemonic == 'enter':
        arguments = [operand(part) for part in text.split(',')]
        if (len(arguments) != 2 or any(op[0] != 'imm' for op in arguments) or
                arguments[0][1] > 65535 or arguments[1][1] > 255 or arguments[1][1] & 31):
            raise Unsupported('unsupported enter nesting or allocation')
        code += ['ef_write(s, s->r[4] - 4u, 32, s->r[5]);',
                 'if (s->fault) return s->fault;',
                 's->r[4] -= 4u; s->r[5] = s->r[4];',
                 's->r[4] -= %du;' % arguments[0][1]]
    elif mnemonic == 'leave' and not text:
        code += ['a = ef_read(s, s->r[5], 32); if (s->fault) return s->fault;',
                 's->r[4] = s->r[5] + 4u; s->r[5] = a;']
    elif mnemonic in ('push', 'pop'):
        op = operand(text)
        if op[0] == 'seg' and mnemonic == 'push':
            code += ['ef_write(s, s->r[4] - 4u, 32, %s);' % read(op, 16),
                     'if (s->fault) return s->fault; s->r[4] -= 4u;']
        elif op[0] == 'seg' and mnemonic == 'pop' and op[1] != 1:
            code += ['a = ef_read(s, s->r[4], 32); if (s->fault) return s->fault;',
                     'ef_load_segment(s, %d, a);' % op[1],
                     'if (s->fault) return s->fault; s->r[4] += 4u;']
        elif mnemonic == 'push' and (width_of(op) == 32 or op[0] == 'imm'):
            code += ['a = %s; if (s->fault) return s->fault;' % read(op, 32),
                     'ef_write(s, s->r[4] - 4u, 32, a);',
                     'if (s->fault) return s->fault; s->r[4] -= 4u;']
        elif mnemonic == 'pop' and op[0] == 'reg' and width_of(op) == 32:
            code += ['a = ef_read(s, s->r[4], 32); if (s->fault) return s->fault;',
                     's->r[4] += 4u;', write(op, 'a', 32)]
        else:
            raise Unsupported('unsupported stack operand')
    elif mnemonic in ('shl', 'sal', 'shr', 'sar', 'rol', 'ror', 'rcl', 'rcr'):
        arguments = [operand(part) for part in text.split(',')]
        if (len(arguments) != 2 or width_of(arguments[0]) not in (8, 16, 32) or
                (arguments[1][0] != 'imm' and arguments[1] != operand('cl'))):
            raise Unsupported('unsupported shift operands')
        dst, count = arguments
        width = width_of(dst)
        rotate = mnemonic in ('rol', 'ror', 'rcl', 'rcr')
        operation = ({'rol': 0, 'ror': 1, 'rcl': 2, 'rcr': 3} if rotate else
                     {'shl': 0, 'sal': 0, 'shr': 1, 'sar': 2})[mnemonic]
        code += ['a = %s; b = %s;' % (read(dst, width), immediate_expression(insn) or read(count, 32)),
                 'if (s->fault) return s->fault;',
                 'result = ef_%s(s, a, b, %d, %d);' % ('rotate' if rotate else 'shift', width, operation),
                 write(dst, 'result', width)]
    elif mnemonic in ('shld', 'shrd'):
        arguments = [operand(part) for part in text.split(',')]
        if len(arguments) != 3:
            raise Unsupported('unsupported double shift operands')
        dst, source, count = arguments
        width = width_of(source)
        if (source[0] != 'reg' or width not in (16, 32) or dst[0] not in ('reg', 'mem') or
                width_of(dst) not in (None, width) or
                (count[0] != 'imm' and count != operand('cl'))):
            raise Unsupported('unsupported double shift operands')
        code += ['a = %s; b = %s;' % (read(dst, width), read(source, width)),
                 'if (s->fault) return s->fault;',
                 'result = ef_double_shift(s, a, b, %s, %d, %d);' %
                 (read(count, 32), width, mnemonic == 'shrd'),
                 write(dst, 'result', width)]
    elif mnemonic in ('div', 'idiv'):
        divisor = operand(text)
        width = width_of(divisor)
        if divisor[0] not in ('reg', 'mem') or width not in (8, 16, 32):
            raise Unsupported('unsupported divide operand')
        code += ['a = %s; if (s->fault) return s->fault;' % read(divisor, width),
                 'ef_divide(s, a, %d, %d);' % (width, mnemonic == 'idiv')]
    elif mnemonic in ('mul', 'imul'):
        arguments = [operand(part) for part in text.split(',')]
        if not 1 <= len(arguments) <= (3 if mnemonic == 'imul' else 1):
            raise Unsupported('unsupported multiply operand count')
        width = width_of(arguments[0])
        if width not in (8, 16, 32):
            raise Unsupported('ambiguous multiply width')
        if len(arguments) == 1:
            if arguments[0][0] == 'imm':
                raise Unsupported('immediate multiply operand')
            dst = operand({8: 'al', 16: 'ax', 32: 'eax'}[width])
            left, right = dst, arguments[0]
        else:
            dst = arguments[0]
            if dst[0] != 'reg' or width == 8:
                raise Unsupported('unsupported multiply destination')
            left, right = (dst, arguments[1]) if len(arguments) == 2 else arguments[1:]
            if (len(arguments) == 3 and right[0] != 'imm') or any(
                    width_of(op) not in (None, width) for op in (left, right)):
                raise Unsupported('unsupported multiply source widths')
        code += ['a = %s; b = %s;' % (read(left, width), read(right, width)),
                 'if (s->fault) return s->fault;',
                 'product = ef_multiply(s, a, b, %d, %d);' % (width, mnemonic == 'imul')]
        if len(arguments) == 1 and width == 8:
            code.append(write(operand('ax'), '(uint32_t)product', 16))
        else:
            code.append(write(dst, '(uint32_t)product', width))
            if len(arguments) == 1:
                code.append(write(operand('dx' if width == 16 else 'edx'),
                                  '(uint32_t)(product >> %d)' % width, width))
    elif mnemonic in ('mov', 'lea', 'movzx', 'movsx', 'add', 'sub', 'cmp',
                       'adc', 'sbb', 'and', 'or', 'xor', 'test', 'inc', 'dec', 'neg', 'not'):
        operands = [operand(part) for part in text.split(',')]
        unary = mnemonic in ('inc', 'dec', 'neg', 'not')
        if len(operands) != (1 if unary else 2):
            raise Unsupported('operand count')
        dst = operands[0]
        width = width_of(dst)
        if (width is None and dst[0] == 'mem' and not unary and
                operands[1][0] == 'reg' and mnemonic not in ('movzx', 'movsx', 'lea')):
            width = width_of(operands[1])
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
                code.append('b = %s;' % (immediate_expression(insn) or read(src, src_width or width)))
            code.append('if (s->fault) return s->fault;')
            if mnemonic in ('mov', 'movzx'):
                value = 'b'
            elif mnemonic == 'movsx':
                value = '((b ^ 0x%xu) - 0x%xu)' % (1 << (src_width-1), 1 << (src_width-1))
            elif mnemonic in ('adc', 'sbb'):
                code.append('result = ef_arith_carry(s, a, b, %d, %d, s->flags & 1u);' %
                            (width, mnemonic == 'sbb'))
                value = 'result'
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


def translate(record, *, starts=None, renderer=instruction_c):
    if (record.get('boundary') != 'ghidra-body-ranges' or
            record.get('ghidra_instruction_layout_matches') is not True or
            record.get('overlapping_entries') or record.get('undecodable_addresses')):
        raise Unsupported('body or instruction layout not established')
    instructions = record['instructions']
    local_starts = {i['address'] for i in instructions}
    if record['address'] not in local_starts:
        raise Unsupported('entry is not an instruction start')
    starts = local_starts if starts is None else starts
    cases = ['case 0x%xu: %s' % (i['address'], renderer(i, starts)) for i in instructions]
    variables = [name for name in ('a', 'b', 'result', 'carry')
                 if re.search(r'\b' + name + r'\b', '\n'.join(cases))]
    declarations = 'uint32_t ' + ', '.join(variables) + ';' if variables else ''
    if re.search(r'\bproduct\b', '\n'.join(cases)):
        declarations += '\n    uint64_t product;'
    prefix = asm_x87.NARROW_DEFINE if any('ef_x87_store_real(' in case for case in cases) else ''
    if any('ef_x87_register_binary(' in case or 'ef_x87_memory_binary(' in case for case in cases):
        prefix += asm_x87.BINARY_DEFINE
    return prefix + '''/* Mechanical ASM-derived draft. NEVER COMMIT. Flat 32-bit segments only. */
#include "ef_lift_state.h"
unsigned ef_lift_%08x(ef_lift_state *s, uint32_t fuel) {
    %s
    if (s->fault) return s->fault;
    if (!ef_segments_valid(s)) return 7;
    while (fuel--) {
        switch (s->pc) {
%s
        default: return 2;
        }
    }
    return 1;
}
''' % (record['address'], declarations, '\n'.join(cases))


def read_inventory(inventory, image):
    """Validate payload bytes and independently decode every input record."""
    payload = image.read_bytes()
    manifest = json.loads((inventory / 'manifest.json').read_text())
    if (manifest.get('schema') != 3 or manifest.get('boundary') != 'ghidra-body-ranges' or
            manifest.get('payload_sha256') != hashlib.sha256(payload).hexdigest()):
        raise ValueError('invalid inventory provenance')
    records = []
    seen = set()
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
        if (decoded != record['instructions'] or record['address'] != entry['address'] or
                record['address'] in seen):
            raise ValueError('instruction decode mismatch')
        seen.add(record['address'])
        records.append(record)
    return manifest, records


def generate(inventory, output, image):
    manifest, records = read_inventory(inventory, image)
    output.mkdir(parents=True, exist_ok=False)
    (output / 'ef_lift_state.h').write_text(RUNTIME)
    report = []
    for record in records:
        entry = {'address': record['address']}
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
                  state_abi=STATE_ABI,
                  inventory_sha256=hashlib.sha256((inventory / 'manifest.json').read_bytes()).hexdigest(),
                  model='flat-x86-explicit-state',
                  runtime_sha256=hashlib.sha256(RUNTIME.encode()).hexdigest(), entries=report)
    bind_backends(result, [(output / e['file']).read_text() for e in report if e['state'] == 'generated-unverified'])
    (output / 'report.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


def bind_backends(report, codes):
    codes = list(codes)
    required = any(asm_x87.NARROW_DEFINE in code for code in codes)
    report['required_backends'] = [asm_x87.NARROW_BACKEND] if required else []
    if required:
        report['backend_source_sha256'] = hashlib.sha256((ROOT / 'tools/asm_x87_narrow.c').read_bytes()).hexdigest()
    if any(asm_x87.BINARY_DEFINE in code for code in codes):
        report['required_backends'].append(asm_x87.BINARY_BACKEND)
        report['binary_backend_source_sha256'] = hashlib.sha256((ROOT / 'tools/asm_x87_binary.c').read_bytes()).hexdigest()


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
