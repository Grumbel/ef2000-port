# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Raw x87 register/memory primitives for the explicit-state translator.

These model register/tag/status effects, not instruction environment updates.
The instruction renderer must separately supply guest IP/opcode/selector effects.
No original instructions are executed by these helpers.
"""

import re

NARROW_BACKEND = 'softfloat-3e-8086-narrow-v1'
NARROW_DEFINE = '#define EF_LIFT_SOFTFLOAT_NARROW 1\n'


def stack_operation(mnemonic, text):
    unary = {'fld1': 0, 'fldz': 1, 'fchs': 3, 'fabs': 4, 'fincstp': 9, 'fdecstp': 10}
    if mnemonic in unary and not text:
        return unary[mnemonic], 0
    register = {'fld': 2, 'fxch': 5, 'fst': 6, 'fstp': 7, 'ffree': 8}
    match = re.fullmatch(r'st(?:\(([0-7])\))?', text.strip().lower())
    if mnemonic in register and match:
        return register[mnemonic], int(match[1] or 0)
    return None


def memory80_encoding(insn):
    """Return encoded data segment and FOP; reject unmodeled prefix/address forms."""
    return memory_encoding(insn, 0xdb, 5 if insn['mnemonic'] == 'fld' else 7)


def memory_encoding(insn, opcode, group):
    encoded = bytes.fromhex(insn['bytes'])
    segments = {0x26: 0, 0x2e: 1, 0x36: 2, 0x3e: 3, 0x64: 4, 0x65: 5}
    explicit = segments.get(encoded[0]) if encoded else None
    offset = 1 if explicit is not None else 0
    if len(encoded) < offset + 2 or encoded[offset] != opcode:
        raise ValueError('unsupported x87 memory encoding')
    modrm = encoded[offset+1]
    mode, base = modrm >> 6, modrm & 7
    if mode == 3 or ((modrm >> 3) & 7) != group:
        raise ValueError('unsupported x87 memory encoding')
    if base == 4:
        if len(encoded) < offset + 3:
            raise ValueError('missing x87 SIB')
        base = encoded[offset+2] & 7
    segment = explicit if explicit is not None else (2 if base in (4, 5) and not (mode == 0 and base == 5) else 3)
    return segment, ((encoded[offset] & 7) << 8) | modrm


STACK_RUNTIME = r'''
enum {
    EF_X87_ONE, EF_X87_ZERO, EF_X87_LOAD, EF_X87_CHS, EF_X87_ABS,
    EF_X87_EXCHANGE, EF_X87_STORE, EF_X87_STORE_POP, EF_X87_FREE,
    EF_X87_INC_TOP, EF_X87_DEC_TOP
};
static unsigned ef_x87_index(ef_lift_state *s, unsigned index) {
    return ((s->x87_status >> 11) + index) & 7u;
}
static unsigned ef_x87_empty(ef_lift_state *s, unsigned index) {
    return ((s->x87_tag >> (index * 2)) & 3u) == 3u;
}
static unsigned ef_x87_classify(const uint32_t *value) {
    uint32_t exponent = value[2] & 0x7fffu;
    if (!exponent && !(value[0] | value[1])) return 1;
    if (!exponent || exponent == 0x7fffu || !(value[1] & 0x80000000u)) return 2;
    return 0;
}
static void ef_x87_put(ef_lift_state *s, unsigned index, const uint32_t *value) {
    unsigned word;
    for (word = 0; word < 3; ++word) s->x87[index][word] = value[word];
    s->x87_tag = (s->x87_tag & ~(3u << (index * 2))) |
                 (ef_x87_classify(value) << (index * 2));
}
static void ef_x87_top(ef_lift_state *s, unsigned index) {
    s->x87_status = (s->x87_status & ~0x3800u) | ((index & 7u) << 11);
}
static unsigned ef_x87_stack_fault(ef_lift_state *s, unsigned overflow) {
    s->x87_status = (s->x87_status & ~0x200u) | 0x41u | (overflow ? 0x200u : 0u);
    if (s->x87_control & 1u) return 1;
    s->x87_status |= 0x8080u;
    return 0;
}
static unsigned ef_x87_stack(ef_lift_state *s, unsigned operation, unsigned argument) {
    uint32_t indefinite[3] = {0, 0xc0000000u, 0xffffu};
    uint32_t value[3], other[3];
    unsigned top, index, destination, word, empty_top, empty_index;
    if (operation > EF_X87_DEC_TOP || argument > 7) return 9;
    /* Delivery is deferred to the next waiting instruction. Do not pretend
       that the pending #MF has been handled by a register operation. */
    if (s->x87_status & 0x80u) return 8;
    top = ef_x87_index(s, 0); index = ef_x87_index(s, argument);
    if (operation <= EF_X87_LOAD) {
        destination = (top + 7u) & 7u;
        if (operation == EF_X87_LOAD && ef_x87_empty(s, index)) {
            if (!ef_x87_stack_fault(s, 0)) return 0;
            for (word = 0; word < 3; ++word) value[word] = indefinite[word];
        } else if (!ef_x87_empty(s, destination)) {
            if (!ef_x87_stack_fault(s, 1)) return 0;
            for (word = 0; word < 3; ++word) value[word] = indefinite[word];
        } else {
            s->x87_status &= ~0x200u;
            for (word = 0; word < 3; ++word)
                value[word] = operation == EF_X87_LOAD ? s->x87[index][word] : 0;
            if (operation == EF_X87_ONE) { value[1] = 0x80000000u; value[2] = 0x3fffu; }
        }
        ef_x87_put(s, destination, value);
        ef_x87_top(s, destination);
        return 0;
    }
    if (operation >= EF_X87_FREE) {
        s->x87_status &= ~0x200u;
        if (operation == EF_X87_FREE) s->x87_tag |= 3u << (index * 2);
        else ef_x87_top(s, top + (operation == EF_X87_INC_TOP ? 1u : 7u));
        return 0;
    }
    empty_top = ef_x87_empty(s, top); empty_index = ef_x87_empty(s, index);
    if (empty_top || (operation == EF_X87_EXCHANGE && empty_index)) {
        if (!ef_x87_stack_fault(s, 0)) return 0;
    } else s->x87_status &= ~0x200u;
    for (word = 0; word < 3; ++word) {
        value[word] = empty_top ? indefinite[word] : s->x87[top][word];
        other[word] = empty_index ? indefinite[word] : s->x87[index][word];
    }
    if (operation == EF_X87_EXCHANGE) {
        ef_x87_put(s, top, other); ef_x87_put(s, index, value);
    } else if (operation == EF_X87_STORE || operation == EF_X87_STORE_POP) {
        ef_x87_put(s, index, value);
        if (operation == EF_X87_STORE_POP) {
            s->x87_tag |= 3u << (top * 2);
            ef_x87_top(s, top + 1u);
        }
    } else {
        if (!empty_top) {
            if (operation == EF_X87_CHS) value[2] ^= 0x8000u;
            else value[2] &= 0x7fffu;
        }
        ef_x87_put(s, top, value);
    }
    return 0;
}
'''

MEMORY80_RUNTIME = r'''
static unsigned ef_x87_memory80(ef_lift_state *s, uint32_t address, unsigned load) {
    uint32_t value[3];
    unsigned top = ef_x87_index(s, 0), destination = (top + 7u) & 7u, word;
    if (s->x87_status & 0x80u) return 8;
    /* Validate the full ten-byte operand before any stack/status effect. */
    if (s->memory_size < 10u || address > s->memory_size - 10u) {
        s->fault = 3; return 3;
    }
    if ((load && !ef_x87_empty(s, destination)) || (!load && ef_x87_empty(s, top))) {
        if (!ef_x87_stack_fault(s, load)) return 0;
        value[0] = 0; value[1] = 0xc0000000u; value[2] = 0xffffu;
    } else {
        s->x87_status &= ~0x200u;
        if (load) {
            value[0] = ef_read(s, address, 32);
            value[1] = ef_read(s, address + 4u, 32);
            value[2] = ef_read(s, address + 8u, 16);
        } else for (word = 0; word < 3; ++word) value[word] = s->x87[top][word];
    }
    /* Extended loads/stores preserve encodings, including sNaNs/unnormals;
       unlike narrow memory operands they do not perform a conversion. */
    if (load) {
        ef_x87_put(s, destination, value); ef_x87_top(s, destination);
    } else {
        ef_write(s, address, 32, value[0]);
        ef_write(s, address + 4u, 32, value[1]);
        ef_write(s, address + 8u, 16, value[2]);
        s->x87_tag |= 3u << (top * 2);
        ef_x87_top(s, top + 1u);
    }
    return 0;
}
'''

REAL_LOAD_RUNTIME = r'''
static unsigned ef_x87_load_real(ef_lift_state *s, uint32_t address, unsigned width) {
    uint32_t low, high = 0, exponent, maximum, bias, sign, flags = 0, value[3];
    uint64_t fraction, implicit, significand;
    unsigned fraction_bits, shift, destination = (ef_x87_index(s, 0) + 7u) & 7u;
    if (width != 32 && width != 64) return 9;
    if (s->x87_status & 0x80u) return 8;
    if (s->memory_size < width / 8u || address > s->memory_size - width / 8u) {
        s->fault = 3; return 3;
    }
    if (!ef_x87_empty(s, destination)) {
        if (!ef_x87_stack_fault(s, 1)) return 0;
        value[0] = 0; value[1] = 0xc0000000u; value[2] = 0xffffu;
    } else {
        s->x87_status &= ~0x200u;
        low = ef_read(s, address, 32);
        if (width == 64) high = ef_read(s, address + 4u, 32);
        if (width == 32) {
            fraction_bits = 23; maximum = 255; bias = 16256;
            sign = (low >> 16) & 0x8000u; exponent = (low >> 23) & 255u;
            fraction = low & 0x7fffffu;
        } else {
            fraction_bits = 52; maximum = 2047; bias = 15360;
            sign = (high >> 16) & 0x8000u; exponent = (high >> 20) & 2047u;
            fraction = ((uint64_t)(high & 0xfffffu) << 32) | low;
        }
        implicit = (uint64_t)1 << fraction_bits;
        shift = 63u - fraction_bits;
        if (exponent == maximum) {
            if (fraction && !(fraction & (implicit >> 1))) flags = 1;
            significand = ((uint64_t)1 << 63) | (fraction << shift);
            if (fraction) significand |= (uint64_t)1 << 62;
            exponent = 0x7fffu;
        } else if (exponent) {
            significand = (implicit | fraction) << shift;
            exponent += bias;
        } else if (fraction) {
            flags = 2; exponent = bias + 1u;
            while (!(fraction & implicit)) { fraction <<= 1; --exponent; }
            significand = fraction << shift;
        } else significand = 0;
        s->x87_status |= flags;
        if (flags & ~s->x87_control) {
            s->x87_status |= 0x8080u;
            /* Unmasked IE suppresses the push. Unmasked DE still loads the
               exact normalized result and defers delivery until a wait. */
            if (flags & 1u) return 0;
        }
        value[0] = (uint32_t)significand; value[1] = (uint32_t)(significand >> 32);
        value[2] = sign | exponent;
    }
    ef_x87_put(s, destination, value); ef_x87_top(s, destination);
    return 0;
}
'''

INTEGER_LOAD_RUNTIME = r'''
static unsigned ef_x87_load_integer(ef_lift_state *s, uint32_t address, unsigned width) {
    uint32_t value[3], exponent, negative;
    uint64_t magnitude;
    unsigned destination = (ef_x87_index(s, 0) + 7u) & 7u;
    if (width != 16 && width != 32 && width != 64) return 9;
    if (s->x87_status & 0x80u) return 8;
    if (s->memory_size < width / 8u || address > s->memory_size - width / 8u) {
        s->fault = 3; return 3;
    }
    if (!ef_x87_empty(s, destination)) {
        if (!ef_x87_stack_fault(s, 1)) return 0;
        value[0] = 0; value[1] = 0xc0000000u; value[2] = 0xffffu;
    } else {
        s->x87_status &= ~0x200u;
        magnitude = ef_read(s, address, width == 64 ? 32 : width);
        if (width == 64) magnitude |= (uint64_t)ef_read(s, address + 4u, 32) << 32;
        negative = (unsigned)(magnitude >> (width-1));
        if (negative) {
            /* Unsigned negation also handles INT64_MIN without signed UB. */
            magnitude = (uint64_t)0 - magnitude;
            if (width != 64) magnitude &= ((uint64_t)1 << width) - 1u;
        }
        exponent = 0;
        if (magnitude) {
            exponent = 16446u;
            while (!(magnitude & ((uint64_t)1 << 63))) { magnitude <<= 1; --exponent; }
        }
        value[0] = (uint32_t)magnitude; value[1] = (uint32_t)(magnitude >> 32);
        value[2] = exponent | (negative ? 0x8000u : 0u);
    }
    ef_x87_put(s, destination, value); ef_x87_top(s, destination);
    return 0;
}
'''

# Outside the base header guard: a later body in the combined translation unit
# may be the first to enable this optional dependency.
NARROW_RUNTIME = r'''
#if defined(EF_LIFT_SOFTFLOAT_NARROW) && !defined(EF_LIFT_NARROW_RUNTIME)
#define EF_LIFT_NARROW_RUNTIME
unsigned ef_x87_narrow(const uint32_t *, unsigned, unsigned, uint32_t *);
static unsigned ef_x87_store_real(ef_lift_state *s, uint32_t address, unsigned width, unsigned pop) {
    uint32_t indefinite[3] = {0, 0xc0000000u, 0xffffu}, out[2], flags, tiny, unmasked;
    unsigned top = ef_x87_index(s, 0);
    const uint32_t *value = s->x87[top];
    if (width != 32 && width != 64) return 9;
    if (s->x87_status & 0x80u) return 8;
    if (s->memory_size < width / 8u || address > s->memory_size - width / 8u) {
        s->fault = 3; return 3;
    }
    if (ef_x87_empty(s, top)) {
        if (!ef_x87_stack_fault(s, 0)) return 0;
        value = indefinite;
    }
    flags = ef_x87_narrow(value, width, (s->x87_control >> 10) & 3u, out);
    if (flags & 0x10000u) return 9;
    tiny = width == 32 ? !(out[0] & 0x7f800000u) && (out[0] & 0x7fffffu) :
           !(out[1] & 0x7ff00000u) && ((out[1] & 0xfffffu) | out[0]);
    /* Unmasked underflow includes exact subnormal results. */
    if (!(s->x87_control & 0x10u) && tiny) flags |= 0x10u;
    unmasked = flags & ~s->x87_control & 0x3fu;
    if (unmasked & 0x19u) {
        /* IE/OE/UE suppress store/pop and do not add PE or rounded-up C1. */
        s->x87_status = (s->x87_status & ~0x200u) | (flags & 0x1fu) | 0x8080u;
        return 0;
    }
    s->x87_status = (s->x87_status & ~0x200u) | flags;
    if (unmasked) s->x87_status |= 0x8080u;
    ef_write(s, address, 32, out[0]);
    if (width == 64) ef_write(s, address + 4u, 32, out[1]);
    if (pop) { s->x87_tag |= 3u << (top * 2); ef_x87_top(s, top + 1u); }
    return 0;
}
#endif
'''
