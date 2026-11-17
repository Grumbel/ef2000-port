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
    from . import asm_inventory, asm_x87, asm_segments
except ImportError:
    import asm_inventory
    import asm_x87
    import asm_segments

ROOT = Path(__file__).resolve().parents[1]
REGS = ('eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi')
SEGMENTS = ('es', 'cs', 'ss', 'ds', 'fs', 'gs')
STATE_ABI = 5
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
''' + asm_segments.RUNTIME + r'''
typedef struct {
    uint32_t r[8];
    uint32_t flags, pc, fault;
    uint8_t *memory;
    uint32_t memory_size;
    uint32_t segment[6]; /* ES, CS, SS, DS, FS, GS: validated zero-base aliases. */
    uint32_t x87[8][3]; /* Physical registers: significand low/high, sign/exponent. */
    uint32_t x87_control, x87_status, x87_tag;
    uint32_t x87_ip, x87_cs, x87_dp, x87_ds, x87_opcode;
    /* Bit 0/1: update comparison CCs on unmasked invalid/denormal. */
    uint32_t x87_compare_profile;
    uint32_t segment_profile; /* 0=legacy aliases, 1=descriptor-derived caches. */
    ef_lift_segment segment_cache[6];
    /* Opt-in live table windows in bounded RAM: bit 0=GDT, bit 1=LDT.
     * Bases/limits describe loaded table context, not saved descriptors. */
    uint32_t table_mask, table_base[2], table_limit[2];
} ef_lift_state;
static unsigned ef_segments_valid(ef_lift_state *s) {
    unsigned i;
    if (s->segment_profile == 1) {
        for (i = 0; i < 6; ++i) {
            const ef_lift_segment *cache = &s->segment_cache[i];
            if (cache->selector != s->segment[i] || cache->base || cache->flags & 2u) return 0;
        }
        return (s->segment[1] & 3u) == 3u &&
            (s->segment_cache[1].flags & 4u) && (s->segment_cache[2].flags & 4u);
    }
    if (s->segment_profile) return 0;
    for (i = 0; i < 6; ++i) {
        uint32_t selector = s->segment[i];
        if (i == 1) { if (selector != 0 && selector != 8) return 0; }
        else if (i == 2) { if (selector != 0 && selector != 16 && selector != 24) return 0; }
        else if (selector > 3 && selector != 8 && selector != 16 && selector != 24) return 0;
    }
    return 1;
}
/* Protected-mode LSL: selector rejection is ZF=0, not a guest fault.
 * Missing live context remains unsupported. P and the accessed bit are
 * irrelevant; unlike a segment load this instruction never installs a cache.
 */
static unsigned ef_lsl(ef_lift_state *s, uint32_t selector, uint32_t *limit) {
    uint32_t offset, address, low, high, access, type, dpl, cpl;
    unsigned table;
    selector &= 0xffffu;
    if (s->segment_profile != 1) { s->fault = 7; return 0; }
    if (!(selector & 0xfffcu)) return 0;
    table = (selector >> 2) & 1u;
    if (!(s->table_mask & (1u << table))) { s->fault = 7; return 0; }
    offset = selector & 0xfff8u;
    if (offset > s->table_limit[table] || s->table_limit[table] - offset < 7u) return 0;
    if (s->table_base[table] > UINT32_MAX - offset) { s->fault = 7; return 0; }
    address = s->table_base[table] + offset;
    if (s->memory_size < 8u || address > s->memory_size - 8u) { s->fault = 7; return 0; }
    low = (uint32_t)s->memory[address] | (uint32_t)s->memory[address+1] << 8 |
          (uint32_t)s->memory[address+2] << 16 | (uint32_t)s->memory[address+3] << 24;
    high = (uint32_t)s->memory[address+4] | (uint32_t)s->memory[address+5] << 8 |
           (uint32_t)s->memory[address+6] << 16 | (uint32_t)s->memory[address+7] << 24;
    access = (high >> 8) & 255u; type = access & 15u;
    dpl = (access >> 5) & 3u; cpl = s->segment[1] & 3u;
    if (!(access & 16u) && type != 1 && type != 2 && type != 3 && type != 9 && type != 11) return 0;
    if ((access & 28u) != 28u && (cpl > dpl || (selector & 3u) > dpl)) return 0;
    *limit = (low & 0xffffu) | (high & 0xf0000u);
    if (high & 0x800000u) *limit = (*limit << 12) | 4095u;
    return 1;
}
static void ef_load_segment(ef_lift_state *s, unsigned index, uint32_t selector) {
    selector &= 0xffffu;
    if (s->segment_profile == 1 && index < 6 && index != 1 && index != 2 &&
        (selector & 0xfffcu) == 0) {
        /* Null data selectors need no descriptor lookup. Make the cache
         * unusable so that subsequent accesses fault, including after a
         * previously usable descriptor was loaded. RPL is still retained. */
        ef_lift_segment *cache = &s->segment_cache[index];
        cache->selector = selector;
        cache->base = cache->limit = cache->access = cache->flags = 0;
        s->segment[index] = selector;
        return;
    }
    if (s->segment_profile) {
        if (s->segment_profile == 1 && index < 6 && index != 1 && index != 2) {
            unsigned table = (selector >> 2) & 1u;
            uint32_t access_address;
            ef_lift_segment candidate;
            if ((s->table_mask & (1u << table)) &&
                !ef_segment_lookup(s->memory, s->memory_size,
                                   s->table_base[table], s->table_limit[table],
                                   selector, s->segment[1] & 3u, 0,
                                   &candidate, &access_address) &&
                !candidate.base && !(candidate.flags & 2u) &&
                /* Conforming-code access checks need a separate audit. */
                (candidate.access & 12u) != 12u) {
                s->memory[access_address] |= 1u;
                s->segment_cache[index] = candidate;
                s->segment[index] = selector;
                return;
            }
        }
        /* SS loads also need interrupt/debug inhibition, not modeled here. */
        s->fault = 7; return;
    }
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
/* A descriptor-backed access must retain its segment and permission kind. */
static unsigned ef_check_segment(ef_lift_state *s, unsigned segment, uint32_t address,
                                 uint32_t bytes, unsigned kind) {
    uint32_t linear;
    if (!s->segment_profile) return 0;
    if (s->segment_profile != 1 || segment >= 6 ||
        s->segment[segment] != s->segment_cache[segment].selector ||
        ef_segment_linear(&s->segment_cache[segment], s->segment[1] & 3u,
                          address, bytes, kind, &linear) || linear != address) {
        s->fault = 7; return 7;
    }
    return 0;
}
static uint32_t ef_read_segment(ef_lift_state *s, unsigned segment, uint32_t address, unsigned width) {
    if (ef_check_segment(s, segment, address, width/8u, segment == 2 ? 3 : 0)) return 0;
    return ef_read(s, address, width);
}
static uint32_t ef_read_code(ef_lift_state *s, uint32_t address, unsigned width) {
    if (ef_check_segment(s, 1, address, width/8u, 2)) return 0;
    return ef_read(s, address, width);
}
static void ef_write_segment(ef_lift_state *s, unsigned segment, uint32_t address,
                             unsigned width, uint32_t value) {
    if (ef_check_segment(s, segment, address, width/8u, segment == 2 ? 4 : 1)) return;
    ef_write(s, address, width, value);
}
static unsigned ef_stack_store(ef_lift_state *s, unsigned segment, uint32_t address, unsigned width, uint32_t value) {
    unsigned byte;
    if (width != 16 && width != 32) return 9;
    if (ef_check_segment(s, segment, address, width/8u, segment == 2 ? 4 : 1)) return s->fault;
    /* Preserve the flat oracle's partial writes, without committing ESP. */
    for (byte = 0; byte < width/8u; ++byte) {
        ef_write(s, address+byte, 8, value >> (8u*byte));
        if (s->fault) return s->fault;
    }
    return 0;
}
/* Match the flat integer oracle's save/restore order, including partial
 * effects on a memory fault. ESP commits only after the instruction completes.
 * Exception delivery itself remains outside this explicit-state model. */
static unsigned ef_stack_all(ef_lift_state *s, unsigned pop) {
    uint32_t stack = s->r[4], address, value;
    unsigned slot, byte, index;
    /* Validate descriptor extents before any register or RAM changes. The
     * discarded saved-ESP slot of POPA is not a memory operand. */
    for (slot = 0; slot < 8; ++slot) {
        if (pop && slot == 3) continue;
        address = (pop ? stack : stack-32u) + slot*4u;
        if (ef_check_segment(s, 2, address, 4, pop ? 3 : 4)) return s->fault;
    }
    for (slot = 0; slot < 8; ++slot) {
        index = 7u-slot;
        if (pop && index == 4) continue; /* Discard the saved ESP slot, without reading it. */
        address = (pop ? stack : stack-32u) + slot*4u;
        if (pop) {
            value = ef_read(s, address, 32);
            if (s->fault) return s->fault;
            s->r[index] = value;
        } else {
            value = s->r[index];
            for (byte = 0; byte < 4; ++byte) {
                ef_write(s, address+byte, 8, value >> (8u*byte));
                if (s->fault) return s->fault;
            }
        }
    }
    s->r[4] = pop ? stack+32u : stack-32u;
    return 0;
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
                          asm_x87.REAL_LOAD_RUNTIME + asm_x87.INTEGER_LOAD_RUNTIME + asm_x87.INTEGER_STORE_RUNTIME + asm_x87.ROUND_RUNTIME + asm_x87.COMPARE_RUNTIME + '\n#endif\n')
RUNTIME += asm_x87.NARROW_RUNTIME + asm_x87.BINARY_RUNTIME


class Unsupported(ValueError):
    pass


def address_expression(text):
    """Parse 32-bit addressing and legal 16-bit register combinations."""
    text = text.replace(' ', '')
    parts = re.findall(r'[+-]?[^+-]+', text)
    if not parts or ''.join(parts) != text:
        raise Unsupported('unsupported address expression')
    terms = []
    narrow = [part.lstrip('+') for part in parts
              if part.lstrip('+') in ('bx', 'bp', 'si', 'di')]
    if narrow:
        allowed = {('bx',), ('bp',), ('si',), ('di',),
                   ('bx', 'si'), ('bx', 'di'), ('bp', 'si'), ('bp', 'di')}
        if tuple(narrow) not in allowed:
            raise Unsupported('unsupported 16-bit address registers')
    for part in parts:
        negative = part.startswith('-')
        part = part.lstrip('+-')
        if narrow and part in ('bx', 'bp', 'si', 'di') and not negative:
            term = '(s->r[%d] & 0xffffu)' % REG[part][0]
        elif part in REGS and not narrow:
            term = 's->r[%d]' % REGS.index(part)
        elif not narrow and re.fullmatch(r'(?:eax|ecx|edx|ebx|esp|ebp|esi|edi)\*[1248]', part):
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
    expression = '(uint32_t)(0u' + ''.join(terms) + ')'
    return '(' + expression + ' & 0xffffu)' if narrow else expression


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
        size, segment, expression, absolute = match.groups()
        width = {'byte': 8, 'word': 16, 'dword': 32, 'fword': 48, 'qword': 64, 'tbyte': 80}.get(size)
        address = expression or absolute
        if segment is None:
            # Objdump prints an unscaled base before the index. A scaled EBP
            # index with no EBP base uses DS, unlike an EBP/ESP base using SS.
            base = re.search(r'(?:^|[+-])(eax|ecx|edx|ebx|esp|ebp|esi|edi|bx|bp|si|di)(?=[+-]|$)',
                             address.replace(' ', ''))
            segment = 'ss' if base and base.group(1) in ('esp', 'ebp', 'bp') else 'ds'
        return ('mem', address_expression(address), width, SEGMENTS.index(segment))
    raise Unsupported('unsupported operand: ' + text)


def width_of(op):
    return op[2] if op[0] in ('reg', 'mem', 'seg') else None


def scalar_stack_width(insn, op):
    """Check scalar PUSH/POP width/form; 32-bit address parsing stays mandatory."""
    encoded = bytes.fromhex(insn['bytes'])
    offset, word, segment = 0, False, False
    while offset < len(encoded):
        byte = encoded[offset]
        if byte == 0x66 and not word:
            word = True
        elif byte in (0x26, 0x2e, 0x36, 0x3e, 0x64, 0x65) and not segment:
            segment = True
        else:
            break
        offset += 1
    width = 16 if word else 32
    pop = insn['mnemonic'] == 'pop'
    if (offset >= len(encoded) or
            (op[0] != 'imm' and width_of(op) != width) or
            (insn['mnemonic'] == 'pushw' and (not word or op[0] != 'imm'))):
        raise Unsupported('scalar stack width/encoding mismatch')
    opcode = encoded[offset]
    if op[0] == 'imm' and not pop:
        if insn['mnemonic'] != ('pushw' if word else 'push') or opcode not in (0x68, 0x6a):
            raise Unsupported('unsupported stack immediate encoding')
        count = 1 if opcode == 0x6a else width//8
        if len(encoded) != offset+1+count:
            raise Unsupported('unsupported stack immediate length')
        immediate = int.from_bytes(encoded[offset+1:], 'little', signed=opcode == 0x6a)
        if (immediate ^ op[1]) & ((1 << width)-1):
            raise Unsupported('stack immediate encoding/spelling mismatch')
    elif op[0] == 'reg' and opcode in range(0x58 if pop else 0x50, 0x60 if pop else 0x58):
        if len(encoded) != offset+1 or (opcode & 7) != op[1]:
            raise Unsupported('stack register encoding/spelling mismatch')
    elif op[0] in ('mem', 'reg') and opcode == (0x8f if pop else 0xff):
        if len(encoded) < offset+2:
            raise Unsupported('missing stack ModRM')
        modrm = encoded[offset+1]
        mode, base = modrm >> 6, modrm & 7
        if ((modrm >> 3) & 7) != (0 if pop else 6) or (mode == 3) != (op[0] == 'reg'):
            raise Unsupported('unsupported stack ModRM')
        end = offset+2
        if mode == 3:
            if base != op[1]:
                raise Unsupported('stack register encoding/spelling mismatch')
        else:
            if base == 4:
                if len(encoded) <= end:
                    raise Unsupported('missing stack SIB')
                base = encoded[end] & 7
                end += 1
            end += 4 if mode == 2 or (mode == 0 and base == 5) else 1 if mode == 1 else 0
        if len(encoded) != end:
            raise Unsupported('unsupported stack address encoding')
    else:
        raise Unsupported('unsupported scalar stack encoding')
    return width


def read(op, width):
    if op[0] == 'imm':
        return '0x%xu' % (op[1] & ((1 << width) - 1))
    if op[0] == 'reg':
        return '((s->r[%d] >> %d) & 0x%xu)' % (op[1], op[3], (1 << op[2])-1)
    if op[0] == 'seg':
        return '(s->segment[%d] & 0xffffu)' % op[1]
    return 'ef_read_segment(s, %d, %s, %d)' % (op[3], op[1], width)


def write(op, value, width):
    if op[0] == 'reg':
        index, bits, shift = op[1:]
        if bits != width:
            raise Unsupported('register width mismatch')
        mask = ((1 << bits)-1) << shift
        return 's->r[%d] = (s->r[%d] & 0x%xu) | (((%s) & 0x%xu) << %d);' % (
            index, index, ~mask & 0xffffffff, value, (1 << bits)-1, shift)
    if op[0] == 'mem':
        return 'ef_write_segment(s, %d, %s, %d, %s);' % (op[3], op[1], width, value)
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
    expression = 'ef_read_code(s, 0x%xu, %d)' % (insn['address'] + offset, width)
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
    destination_operand = first if mnemonic in ('movs', 'stos') else second
    if mnemonic != 'lods' and destination_operand[3] != SEGMENTS.index('es'):
        raise Unsupported('string destination must use ES')
    source_operand = first if mnemonic == 'cmps' else second
    code = []
    if repeat:
        code.append('if (!s->r[1]) { s->pc = 0x%xu; break; }' % next_pc)
    if mnemonic == 'cmps':
        # Native AMD compatibility-mode evidence: ES destination access,
        # including its segment/page checks, precedes the source access.
        # Stop on the first fault rather than relying on fault overwrites.
        segment = source_operand[3]
        code += ['b = ef_read_segment(s, 0, s->r[7], %d); if (s->fault) return s->fault;' % width,
                 'a = ef_read_segment(s, %d, s->r[6], %d);' % (segment, width)]
    elif mnemonic in ('movs', 'lods'):
        code.append('a = ef_read_segment(s, %d, s->r[6], %d);' % (source_operand[3], width))
    elif mnemonic in ('stos', 'scas'):
        code.append('a = %s;' % read(accumulator, width))
    if mnemonic == 'scas':
        code.append('b = ef_read_segment(s, 0, s->r[7], %d);' % width)
    code.append('if (s->fault) return s->fault;')
    if mnemonic in ('movs', 'stos'):
        code += ['ef_write_segment(s, 0, s->r[7], %d, a);' % width, 'if (s->fault) return s->fault;']
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
    if insn['mnemonic'] in ('cs', 'ds'):
        encoded = bytes.fromhex(insn['bytes'])
        parts = insn['operands'].split(None, 1)
        valid = False
        if (len(parts) == 2 and parts[0] == 'mov' and encoded and
                encoded[0] == (0x2e if insn['mnemonic'] == 'cs' else 0x3e)):
            arguments = [operand(part) for part in parts[1].split(',')]
            if len(arguments) == 2:
                dst, src = arguments
                if len(encoded) == 6 and 0xb8 <= encoded[1] <= 0xbf:
                    valid = (dst == operand(REGS[encoded[1]-0xb8]) and
                             src == ('imm', int.from_bytes(encoded[2:], 'little')))
                elif (len(encoded) == 3 and encoded[1] in (0x89, 0x8b) and
                      encoded[2] >> 6 == 3):
                    reg = operand(REGS[(encoded[2] >> 3) & 7])
                    rm = operand(REGS[encoded[2] & 7])
                    valid = arguments == ([reg, rm] if encoded[1] == 0x8b else [rm, reg])
        if not valid:
            raise Unsupported('unsupported register MOV segment prefix')
        # No data memory operand exists, so this prefix selects no segment.
        # Retain its byte for fetch checks, length and static-code validation.
        insn = dict(insn, mnemonic='mov', operands=parts[1])
    if insn['mnemonic'] == 'data16':
        encoded = bytes.fromhex(insn['bytes'])
        parts = insn['operands'].split(None, 1)
        if not parts:
            raise Unsupported('unsupported data16 spelling')
        mnemonic, text = parts[0], parts[1] if len(parts) > 1 else ''
        # A checked segment override may accompany the operand-size prefix
        # on a memory segment MOV. Keep it for permission and address checks.
        segment_encoding = encoded
        overrides = {0x26: 'es', 0x2e: 'cs', 0x36: 'ss',
                     0x3e: 'ds', 0x64: 'fs', 0x65: 'gs'}
        override = overrides.get(encoded[1]) if len(encoded) > 1 and encoded[0] == 0x66 else None
        if override:
            segment_encoding = encoded[:1] + encoded[2:]
        if encoded == b'\x66\xf3\xa4' and mnemonic == 'rep':
            # Operand size has no effect on MOVSB; address size remains 32.
            if text != 'movs BYTE PTR es:[edi],BYTE PTR ds:[esi]':
                raise Unsupported('unsupported data16 MOVSB spelling')
        elif (len(segment_encoding) >= 3 and segment_encoding[0] == 0x66 and
              segment_encoding[1] in (0x8c, 0x8e) and segment_encoding[2] >> 6 != 3 and
              mnemonic == 'mov'):
            arguments = [operand(part) for part in text.split(',')]
            if len(arguments) != 2:
                raise Unsupported('unsupported data16 segment MOV operands')
            seg, mem = arguments if segment_encoding[1] == 0x8e else arguments[::-1]
            if (seg[0] != 'seg' or seg[1] != ((segment_encoding[2] >> 3) & 7) or
                    mem[0] != 'mem' or width_of(mem) != 16 or
                    (segment_encoding[1] == 0x8e and seg[1] == 1) or
                    (override and override + ':' not in text)):
                raise Unsupported('unsupported data16 segment MOV operands')
        else:
            raise Unsupported('unsupported data16 encoding/spelling')
        insn = dict(insn, mnemonic=mnemonic, operands=text)
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
        return ('a = ef_read_segment(s, 2, s->r[4], 32); if (s->fault) return s->fault; '
                'if (ef_check_segment(s, 1, a, 1, 2)) return s->fault; '
                's->pc = a; s->r[4] += %du; return 0;' % (4 + cleanup))
    # These transfers require architectural state we do not model yet. Report
    # that limitation before checking ordinary fallthrough: extending a body
    # past RETF/IRET or a debugger trap cannot make the operation translatable.
    if (mnemonic.startswith(('ret', 'iret')) or
            mnemonic in ('int', 'int3', 'into', 'hlt', 'ud2', 'lcall', 'ljmp',
                         'syscall', 'sysenter', 'sysexit', 'sysret')):
        raise Unsupported('unsupported instruction: ' + mnemonic)
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
    memory_compare = mnemonic in ('fcom', 'fcomp') and text.strip().lower().startswith(('dword ptr ', 'qword ptr '))
    memory_binary = mnemonic in ('fadd', 'fmul', 'fsub', 'fsubr', 'fdiv', 'fdivr') and text.strip().lower().startswith(('dword ptr ', 'qword ptr '))
    try:
        binary_operation = None if memory_binary else asm_x87.binary_register(insn)
        comparison = None if memory_compare else asm_x87.comparison_register(insn)
    except ValueError as error:
        raise Unsupported(str(error)) from error
    real_load = mnemonic == 'fld' and text.strip().lower().startswith(('dword ptr ', 'qword ptr '))
    integer_load = mnemonic == 'fild' and text.strip().lower().startswith(('word ptr ', 'dword ptr ', 'qword ptr '))
    integer_store = mnemonic in ('fist', 'fistp') and text.strip().lower().startswith(('word ptr ', 'dword ptr ', 'qword ptr '))
    narrow_store = mnemonic in ('fst', 'fstp') and text.strip().lower().startswith(('dword ptr ', 'qword ptr '))
    if memory_compare or memory_binary or narrow_store or integer_load or integer_store or real_load or (mnemonic in ('fld', 'fstp') and text.strip().lower().startswith('tbyte ptr ')):
        op = operand(text)
        try:
            if memory_compare:
                encoding, group = (0xd8 if width_of(op) == 32 else 0xdc), (3 if mnemonic == 'fcomp' else 2)
                helper = 'ef_x87_compare_memory(s, b, %d, %d)' % (width_of(op), mnemonic == 'fcomp')
            elif memory_binary:
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
            elif integer_store:
                if width_of(op) == 64 and mnemonic != 'fistp':
                    raise ValueError('unsupported 64-bit FIST encoding')
                encoding = 0xdb if width_of(op) == 32 else 0xdf
                group = 7 if width_of(op) == 64 else (3 if mnemonic == 'fistp' else 2)
                helper = 'ef_x87_store_integer(s, b, %d, %d)' % (width_of(op), mnemonic == 'fistp')
            elif real_load:
                encoding, group = (0xd9 if width_of(op) == 32 else 0xdd), 0
                helper = 'ef_x87_load_real(s, b, %d)' % width_of(op)
            else:
                encoding, group = 0xdb, (5 if mnemonic == 'fld' else 7)
                helper = 'ef_x87_memory80(s, b, %d)' % (mnemonic == 'fld')
            segment, opcode = asm_x87.memory_encoding(insn, encoding, group)
        except ValueError as error:
            raise Unsupported(str(error)) from error
        if segment != op[3]:
            raise Unsupported('x87 decoded segment mismatch')
        storing = narrow_store or integer_store or mnemonic == 'fstp'
        kind = (4 if storing else 3) if segment == 2 else (1 if storing else 0)
        code += ['if (s->x87_status & 0x80u) return 8;',
                 'b = %s;' % op[1],
                 'if (ef_check_segment(s, %d, b, %d, %d)) return s->fault;' % (segment, width_of(op)//8, kind),
                 'a = %s; if (a) return a;' % helper,
                 's->x87_ip = 0x%xu; s->x87_cs = s->segment[1]; s->x87_opcode = 0x%xu;' %
                 (insn['address'], opcode),
                 's->x87_dp = b; s->x87_ds = s->segment[%d];' % segment]
    elif mnemonic == 'fsqrt':
        if text or insn['bytes'] != 'd9fa':
            raise Unsupported('unsupported FSQRT encoding/spelling')
        code += ['a = ef_x87_sqrt(s); if (a) return a;',
                 's->x87_ip = 0x%xu; s->x87_cs = s->segment[1]; s->x87_opcode = 0x1fau;' % insn['address']]
    elif mnemonic == 'frndint':
        if text or insn['bytes'] != 'd9fc':
            raise Unsupported('unsupported FRNDINT encoding/spelling')
        code += ['a = ef_x87_round_integer(s); if (a) return a;',
                 's->x87_ip = 0x%xu; s->x87_cs = s->segment[1]; s->x87_opcode = 0x1fcu;' % insn['address']]
    elif comparison is not None:
        index, quiet, pop, opcode = comparison
        code += ['a = ef_x87_compare_register(s, %d, %d, %d); if (a) return a;' % (index, quiet, pop),
                 's->x87_ip = 0x%xu; s->x87_cs = s->segment[1]; s->x87_opcode = 0x%xu;' %
                 (insn['address'], opcode)]
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
        constant_encoding = {'fld1': b'\xd9\xe8', 'fldl2t': b'\xd9\xe9',
                             'fldl2e': b'\xd9\xea', 'fldpi': b'\xd9\xeb',
                             'fldlg2': b'\xd9\xec', 'fldln2': b'\xd9\xed', 'fldz': b'\xd9\xee'}
        if mnemonic in constant_encoding and encoded != constant_encoding[mnemonic]:
            raise Unsupported('x87 constant encoding/spelling mismatch')
        code += ['a = ef_x87_stack(s, %d, %d); if (a) return a;' % (operation, argument),
                 's->x87_ip = 0x%xu; s->x87_cs = s->segment[1]; s->x87_opcode = 0x%xu;' %
                 (insn['address'], ((encoded[0] & 7) << 8) | encoded[1])]
    elif mnemonic in ('fwait', 'wait') and not text:
        if insn['bytes'] != '9b':
            raise Unsupported('unsupported WAIT encoding')
        code.append('if (s->x87_status & 0x80u) return 8;')
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
    elif mnemonic == 'lsl':
        arguments = [operand(part) for part in text.split(',')]
        if (len(arguments) != 2 or arguments[0][0] != 'reg' or
                width_of(arguments[0]) not in (16, 32) or
                arguments[1][0] not in ('reg', 'mem') or
                width_of(arguments[1]) not in (None, 16, 32)):
            raise Unsupported('unsupported LSL operands')
        dst, src = arguments
        code += ['a = %s; if (s->fault) return s->fault;' % read(src, 16),
                 'b = ef_lsl(s, a, &a); if (s->fault) return s->fault;',
                 's->flags = (s->flags & ~64u) | (b ? 64u : 0u);',
                 'if (b) { %s }' % write(dst, 'a', width_of(dst))]
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
        code += ['if (ef_check_segment(s, %d, %s, %du, %d)) return s->fault;' %
                 (src[3], src[1], width//8 + 2, 3 if src[3] == 2 else 0),
                 'a = ef_read_segment(s, %d, %s, %d);' % (src[3], src[1], width),
                 'if (s->fault) return s->fault;',
                 'b = ef_read_segment(s, %d, (uint32_t)(%s + %du), 16);' % (src[3], src[1], width//8),
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
        code += ['a = ef_read_segment(s, %d, (uint32_t)(s->r[3] + (s->r[0] & 255u)), 8);' % source[3],
                 'if (s->fault) return s->fault;', write(operand('al'), 'a', 8)]
    elif mnemonic in ('bsf', 'bsr'):
        arguments = [operand(part) for part in text.split(',')]
        if len(arguments) != 2:
            raise Unsupported('unsupported bit scan operands')
        dst, src = arguments
        width = width_of(dst)
        if (dst[0] != 'reg' or width not in (16, 32) or src[0] not in ('reg', 'mem') or
                width_of(src) not in (None, width)):
            raise Unsupported('unsupported bit scan widths')
        code += ['a = %s; if (s->fault) return s->fault;' % read(src, width),
                 # Undefined flags follow the existing Unicorn oracle policy.
                 '(void)ef_logic(s, a, %d); if (a) { b = 0;' % width,
                 'while ((a >>= 1) != 0) ++b;' if mnemonic == 'bsr' else
                 'while (!(a & 1u)) { a >>= 1; ++b; }',
                 write(dst, 'b', width), '}']
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
        encoded = bytes.fromhex(insn['bytes'])
        if encoded != bytes((0xc8, arguments[0][1] & 255, arguments[0][1] >> 8, arguments[1][1])):
            raise Unsupported('unsupported ENTER encoding/spelling')
        code += ['a = s->r[4] - 4u; b = a - %du;' % arguments[0][1],
                 'ef_write_segment(s, 2, a, 32, s->r[5]);',
                 'if (s->fault) return s->fault;',
                 'if (ef_check_segment(s, 2, b, 1, 4)) return s->fault;',
                 'if (b >= s->memory_size) { s->fault = 3; return 3; }',
                 's->r[5] = a; s->r[4] = b;']
    elif mnemonic == 'leave' and not text:
        code += ['a = ef_read_segment(s, 2, s->r[5], 32); if (s->fault) return s->fault;',
                 's->r[4] = s->r[5] + 4u; s->r[5] = a;']
    elif mnemonic == 'cli':
        if text or insn['bytes'] != 'fa':
            raise Unsupported('CLI encoding/spelling mismatch')
        # Only the protected-mode case authorized by IOPL is modeled.
        # Reject VM86 and privilege failures rather than approximating
        # CR4.PVI/VME, VIF or a general-protection exception. CLI creates no
        # interrupt shadow; STI must remain unsupported until that is modeled.
        code += ['if ((s->flags & 0x20000u) ||',
                 '    (s->segment[1] & 3u) > ((s->flags >> 12) & 3u)) return 7;',
                 's->flags &= ~0x200u;']
    elif mnemonic in ('popf', 'popfw'):
        width = 16 if mnemonic == 'popfw' else 32
        if text or insn['bytes'] != ('669d' if width == 16 else '9d'):
            raise Unsupported('POPF encoding/spelling mismatch')
        # Protected-mode privilege comes from CS, not the requested stack
        # image. Virtual-8086 substitution requires CR4 state we do not model.
        code += ['if (s->flags & 0x20000u) return 7;',
                 'a = ef_read_segment(s, 2, s->r[4], %d); if (s->fault) return s->fault;' % width,
                 'b = %s;' % ('0x4fd5u' if width == 16 else '0x244fd5u'),
                 'if (!(s->segment[1] & 3u)) b |= 0x3000u;',
                 'if ((s->segment[1] & 3u) > ((s->flags >> 12) & 3u)) b &= ~0x200u;',
                 's->flags = ((s->flags & ~b) | (a & b)) & ~0x10000u;',
                 's->r[4] += %du;' % (width//8)]
    elif mnemonic in ('pushf', 'pushfw'):
        width = 16 if mnemonic == 'pushfw' else 32
        if text or insn['bytes'] != ('669c' if width == 16 else '9c'):
            raise Unsupported('PUSHF encoding/spelling mismatch')
        # This model has no virtual-8086/CR4.VME state. Do not approximate
        # its privilege checks or virtual interrupt flag substitution.
        code += ['if (s->flags & 0x20000u) return 7;',
                 'a = s->flags & %s;' % ('0xffffu' if width == 16 else '0xfcffffu'),
                 'b = ef_stack_store(s, 2, s->r[4] - %du, %d, a); if (b) return b;' % (width//8, width),
                 's->r[4] -= %du;' % (width//8)]
    elif mnemonic in ('pusha', 'popa'):
        if text or insn['bytes'] != ('60' if mnemonic == 'pusha' else '61'):
            raise Unsupported('unsupported PUSHA/POPA encoding/spelling')
        code.append('a = ef_stack_all(s, %d); if (a) return a;' % (mnemonic == 'popa'))
    elif mnemonic in ('push', 'pushw', 'pop', 'popw'):
        op = operand(text)
        if op[0] == 'seg' and mnemonic in ('push', 'pushw'):
            width = 16 if mnemonic == 'pushw' else 32
            opcode = ('06', '0e', '16', '1e', '0fa0', '0fa8')[op[1]]
            if insn['bytes'] != ('66' if width == 16 else '') + opcode:
                raise Unsupported('segment PUSH encoding/spelling mismatch')
            # For operand-size 32 use the flat oracle's zero-extended store.
            # Intel permits implementations to leave the upper half unchanged.
            code += ['a = %s;' % read(op, 16),
                     'b = ef_stack_store(s, 2, s->r[4] - %du, %d, a); if (b) return b;' % (width//8, width),
                     's->r[4] -= %du;' % (width//8)]
        elif op[0] == 'seg' and mnemonic in ('pop', 'popw') and op[1] != 1:
            width = 16 if mnemonic == 'popw' else 32
            opcode = ('07', '', '17', '1f', '0fa1', '0fa9')[op[1]]
            if insn['bytes'] != ('66' if width == 16 else '') + opcode:
                raise Unsupported('segment POP encoding/spelling mismatch')
            code += ['a = ef_read_segment(s, 2, s->r[4], 16); if (s->fault) return s->fault;',
                     'ef_load_segment(s, %d, a);' % op[1],
                     'if (s->fault) return s->fault; s->r[4] += %du;' % (width//8)]
        elif mnemonic in ('push', 'pushw') and op[0] in ('reg', 'mem', 'imm'):
            width = scalar_stack_width(insn, op)
            code += ['a = %s; if (s->fault) return s->fault;' % read(op, width),
                     'b = ef_stack_store(s, 2, s->r[4] - %du, %d, a); if (b) return b;' % (width//8, width),
                     's->r[4] -= %du;' % (width//8)]
        elif mnemonic == 'pop' and op[0] in ('reg', 'mem'):
            width = scalar_stack_width(insn, op)
            code += ['a = ef_read_segment(s, 2, s->r[4], %d); if (s->fault) return s->fault;' % width]
            if op[0] == 'mem':
                # Evaluate the destination using post-increment ESP, but keep
                # architectural ESP intact until every destination byte stores.
                destination = op[1].replace('s->r[4]', '(s->r[4] + %du)' % (width//8))
                code += ['b = %s;' % destination,
                         'result = ef_stack_store(s, %d, b, %d, a); if (result) return result;' % (op[3], width),
                         's->r[4] += %du;' % (width//8)]
            else:
                code += ['s->r[4] += %du;' % (width//8), write(op, 'a', width)]
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
    elif mnemonic == 'aam':
        base = operand(text)
        encoded = bytes.fromhex(insn['bytes'])
        if (base[0] != 'imm' or len(encoded) != 2 or encoded[0] != 0xd4 or
                base[1] != encoded[1]):
            raise Unsupported('AAM encoding/spelling mismatch')
        code += ['b = %s; if (s->fault) return s->fault;' % read(base, 8),
                 'if (!b) { s->fault = 5; return 5; }',
                 'a = s->r[0] & 255u;',
                 'result = (a/b)*256u + a%b;',
                 # CF/OF/AF are undefined; retain the flat oracle's zero policy.
                 '(void)ef_logic(s, result, 8);',
                 write(operand('ax'), 'result', 16)]
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
    cases = []
    for i in instructions:
        code = renderer(i, starts)
        fetch = 'if (ef_check_segment(s, 1, 0x%xu, %du, 2)) return s->fault; ' % (
            i['address'], len(bytes.fromhex(i['bytes'])))
        cases.append('case 0x%xu: %s%s' % (i['address'], fetch, code))
    variables = [name for name in ('a', 'b', 'result', 'carry')
                 if re.search(r'\b' + name + r'\b', '\n'.join(cases))]
    declarations = 'uint32_t ' + ', '.join(variables) + ';' if variables else ''
    if re.search(r'\bproduct\b', '\n'.join(cases)):
        declarations += '\n    uint64_t product;'
    prefix = asm_x87.NARROW_DEFINE if any('ef_x87_store_real(' in case for case in cases) else ''
    if any('ef_x87_register_binary(' in case or 'ef_x87_memory_binary(' in case or 'ef_x87_sqrt(' in case for case in cases):
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
    if (manifest.get('schema') not in (3, asm_inventory.SCHEMA) or manifest.get('boundary') != 'ghidra-body-ranges' or
            manifest.get('payload_sha256') != hashlib.sha256(payload).hexdigest()):
        raise ValueError('invalid inventory provenance')
    bodies = None
    if manifest['schema'] == asm_inventory.SCHEMA:
        bodies, digest = asm_inventory.load_body_ranges(inventory / 'ghidra-body-ranges.json', payload)
        if (digest != manifest.get('body_ranges_sha256') or
                manifest.get('instruction_normalization') not in
                (asm_inventory.NORMALIZATION, asm_inventory.PREVIOUS_NORMALIZATION)):
            raise ValueError('invalid normalized inventory provenance')
    if 'missing_body_addresses' in manifest:
        omitted = manifest['missing_body_addresses']
        if (bodies is None or not isinstance(omitted, list) or
                any(type(address) is not int or not 0 <= address < len(payload)
                    or address in bodies for address in omitted) or
                omitted != sorted(set(omitted))):
            raise ValueError('invalid missing body addresses')
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
        if bodies is not None:
            body = bodies.get(record['address'])
            layout = body.get('instruction_spans') if body else None
            normalized = asm_inventory.normalized_layout(layout, payload)
            matches = ([[i['address'], len(bytes.fromhex(i['bytes']))] for i in decoded] == normalized
                       if layout is not None else None)
            if (body is None or record['ranges'] != body['ranges'] or
                    record.get('ghidra_instruction_spans') != layout or
                    record.get('normalized_ghidra_instruction_spans') != normalized or
                    record.get('ghidra_instruction_layout_matches') != matches):
                raise ValueError('normalized Ghidra instruction layout mismatch')
            if matches and manifest.get('instruction_normalization') == asm_inventory.NORMALIZATION:
                decoded = asm_inventory.overlapping_test_moves(image, decoded)
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
