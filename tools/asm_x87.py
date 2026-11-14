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
BINARY_BACKEND = 'softfloat-3e-8086-binary-v1'
BINARY_DEFINE = '#define EF_LIFT_SOFTFLOAT_BINARY 1\n'


def comparison_register(insn):
    mnemonic, text = insn['mnemonic'], insn['operands'].strip().lower()
    if mnemonic not in ('fcom', 'fcomp', 'fucom', 'fucomp', 'fcompp', 'fucompp', 'ftst'):
        return None
    encoded = bytes.fromhex(insn['bytes'])
    special = {'fcompp': (b'\xde\xd9', 1, 0, 2), 'fucompp': (b'\xda\xe9', 1, 1, 2),
               'ftst': (b'\xd9\xe4', 8, 0, 0)}
    if mnemonic in special:
        expected, index, quiet, pop = special[mnemonic]
        if encoded != expected or text:
            raise ValueError('unsupported x87 comparison encoding/spelling')
    else:
        match = re.fullmatch(r'st(?:\(([0-7])\))?', text)
        if not match:
            raise ValueError('unsupported x87 comparison register')
        index = int(match[1] or 0)
        quiet, pop = int(mnemonic.startswith('fu')), int(mnemonic.endswith('p'))
        expected = bytes((0xdd if quiet else 0xd8, (0xe0 if quiet else 0xd0)+pop*8+index))
        if encoded != expected:
            raise ValueError('x87 comparison encoding/spelling mismatch')
    return index, quiet, pop, ((encoded[0] & 7) << 8) | encoded[1]


def binary_register(insn):
    """Decode register arithmetic, checking the independently decoded spelling."""
    mnemonic = insn['mnemonic']
    names = ('fadd', 'fmul', 'fsub', 'fsubr', 'fdiv', 'fdivr')
    if mnemonic.rstrip('p') not in names:
        return None
    encoded = bytes.fromhex(insn['bytes'])
    if len(encoded) != 2 or encoded[0] not in (0xd8, 0xdc, 0xde) or encoded[1] < 0xc0:
        raise ValueError('unsupported x87 arithmetic encoding')
    group, index = (encoded[1] >> 3) & 7, encoded[1] & 7
    if group not in (0, 1, 4, 5, 6, 7):
        raise ValueError('unsupported x87 arithmetic group')
    operation = {0: 0, 1: 2, 4: 1, 5: 1, 6: 3, 7: 3}[group]
    destination = 0 if encoded[0] == 0xd8 else index
    left, right = (0, index) if group in (0, 1, 4, 6) else (index, 0)
    reverse = group in (5, 7) if encoded[0] == 0xd8 else group in (4, 6)
    expected = {0: 'fadd', 1: 'fmul', 4: 'fsub', 5: 'fsub', 6: 'fdiv', 7: 'fdiv'}[group]
    expected += 'r' if reverse else ''
    expected += 'p' if encoded[0] == 0xde else ''
    registers = []
    for operand in insn['operands'].lower().replace(' ', '').split(','):
        match = re.fullmatch(r'st(?:\(([0-7])\))?', operand)
        if not match:
            raise ValueError('unsupported x87 arithmetic operands')
        registers.append(int(match[1] or 0))
    if mnemonic != expected or registers != ([0, index] if encoded[0] == 0xd8 else [index, 0]):
        raise ValueError('x87 arithmetic encoding/spelling mismatch')
    return operation, left, right, destination, int(encoded[0] == 0xde), ((encoded[0] & 7) << 8) | encoded[1]


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
/* Exact widening preserves a signaling payload for arithmetic. FLD separately
   quiets sNaN and applies its own mask-dependent push behavior. */
static unsigned ef_x87_real_bits(uint32_t low, uint32_t high, unsigned width, uint32_t value[3]) {
    uint32_t exponent, maximum, bias, sign, flags = 0;
    uint64_t fraction, implicit, significand;
    unsigned fraction_bits, shift;
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
        exponent = 0x7fffu;
    } else if (exponent) {
        significand = (implicit | fraction) << shift;
        exponent += bias;
    } else if (fraction) {
        flags = 2; exponent = bias + 1u;
        while (!(fraction & implicit)) { fraction <<= 1; --exponent; }
        significand = fraction << shift;
    } else significand = 0;
    value[0] = (uint32_t)significand; value[1] = (uint32_t)(significand >> 32);
    value[2] = sign | exponent;
    return flags;
}
static unsigned ef_x87_load_real(ef_lift_state *s, uint32_t address, unsigned width) {
    uint32_t low, high = 0, flags, value[3];
    unsigned destination = (ef_x87_index(s, 0) + 7u) & 7u;
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
        flags = ef_x87_real_bits(low, high, width, value);
        if (flags & 1u) value[1] |= 0x40000000u;
        s->x87_status |= flags;
        if (flags & ~s->x87_control) {
            s->x87_status |= 0x8080u;
            /* Unmasked IE suppresses the push. Unmasked DE still loads the
               exact normalized result and defers delivery until a wait. */
            if (flags & 1u) return 0;
        }
    }
    ef_x87_put(s, destination, value); ef_x87_top(s, destination);
    return 0;
}
'''

ROUND_RUNTIME = r'''
static unsigned ef_x87_round_integer(ef_lift_state *s) {
    uint32_t value[3] = {0, 0xc0000000u, 0xffffu};
    uint64_t sig, magnitude, remainder, half;
    unsigned top = ef_x87_index(s, 0), exponent, negative, rounding, flags = 0, increment = 0;
    int power, shift;
    if (s->x87_status & 0x80u) return 8;
    if (ef_x87_empty(s, top)) {
        if (!ef_x87_stack_fault(s, 0)) return 0;
    } else {
        value[0] = s->x87[top][0]; value[1] = s->x87[top][1]; value[2] = s->x87[top][2];
        sig = (uint64_t)value[0] | (uint64_t)value[1] << 32;
        exponent = value[2] & 0x7fffu; negative = value[2] & 0x8000u;
        s->x87_status &= ~0x200u;
        if (exponent && !(sig >> 63)) {
            flags = 1; value[0] = 0; value[1] = 0xc0000000u; value[2] = 0xffffu;
        } else if (exponent == 0x7fffu) {
            /* Infinities and quiet NaNs pass through. Quiet a signaling NaN
             * only if invalid is masked; otherwise leave the source intact. */
            if ((sig & 0x7fffffffffffffffu) && !(value[1] & 0x40000000u)) {
                flags = 1; value[1] |= 0x40000000u;
            }
        } else if (sig) {
            if (!exponent) flags = 2;
            if (!(flags & ~s->x87_control)) {
                power = (int)(exponent ? exponent : 1u) - 16383;
                /* At exponent 63 and above every raw-80 value is integral.
                 * FRNDINT ignores precision control and has no integer range limit. */
                if (power < 63) {
                    shift = 63 - power;
                    magnitude = shift >= 64 ? 0 : sig >> shift;
                    remainder = shift >= 64 ? sig : sig & (((uint64_t)1 << shift) - 1u);
                    rounding = (s->x87_control >> 10) & 3u;
                    if (remainder) {
                        flags |= 0x20u;
                        if (rounding == 0 && shift <= 64) {
                            half = (uint64_t)1 << (shift - 1);
                            increment = remainder > half || (remainder == half && (magnitude & 1u));
                        } else if (rounding == 1) increment = negative != 0;
                        else if (rounding == 2) increment = negative == 0;
                    }
                    magnitude += increment;
                    if (increment) flags |= 0x200u;
                    exponent = 0;
                    if (magnitude) {
                        exponent = 16446u;
                        while (!(magnitude >> 63)) { magnitude <<= 1; --exponent; }
                    }
                    value[0] = (uint32_t)magnitude; value[1] = (uint32_t)(magnitude >> 32);
                    value[2] = exponent | negative;
                }
            }
        }
        s->x87_status |= flags;
        if (flags & ~s->x87_control & 0x3fu) {
            s->x87_status |= 0x8080u;
            if (flags & ~s->x87_control & 3u) return 0;
        }
    }
    ef_x87_put(s, top, value);
    return 0;
}
'''

INTEGER_STORE_RUNTIME = r'''
/* Convert raw extended precision without host floating point or signed overflow.
 * Integer stores ignore PC and do not raise DE, including extended denormals. */
static unsigned ef_x87_integer_bits(const uint32_t *value, unsigned width,
                                    unsigned rounding, uint64_t *out) {
    uint64_t sig = (uint64_t)value[0] | (uint64_t)value[1] << 32;
    uint64_t magnitude, remainder = 0, half, limit;
    unsigned negative = (value[2] >> 15) & 1u, increment = 0;
    unsigned exponent = value[2] & 0x7fffu;
    int power, shift;
    limit = (uint64_t)1 << (width - 1u);
    *out = limit; /* Masked invalid conversion: integer indefinite. */
    if (exponent == 0x7fffu || (exponent && !(sig >> 63))) return 1;
    if (!negative) --limit;
    power = (int)(exponent ? exponent : 1u) - 16383;
    if (power >= 63) {
        shift = power - 63;
        if (shift >= 64 || sig > (limit >> shift)) return 1;
        magnitude = sig << shift;
    } else {
        shift = 63 - power;
        magnitude = shift >= 64 ? 0 : sig >> shift;
        remainder = shift >= 64 ? sig : sig & (((uint64_t)1 << shift) - 1u);
        if (remainder) {
            if (rounding == 0 && shift <= 64) {
                half = (uint64_t)1 << (shift - 1);
                increment = remainder > half || (remainder == half && (magnitude & 1u));
            } else if (rounding == 1) increment = negative;
            else if (rounding == 2) increment = !negative;
        }
        magnitude += increment;
        if (magnitude > limit) return 1;
    }
    *out = negative ? (uint64_t)0 - magnitude : magnitude;
    return (remainder ? 0x20u : 0u) | (increment ? 0x200u : 0u);
}
static unsigned ef_x87_store_integer(ef_lift_state *s, uint32_t address, unsigned width, unsigned pop) {
    uint64_t out;
    unsigned flags, top = ef_x87_index(s, 0);
    if (width != 16 && width != 32 && width != 64) return 9;
    if (s->x87_status & 0x80u) return 8;
    if (s->memory_size < width / 8u || address > s->memory_size - width / 8u) {
        s->fault = 3; return 3;
    }
    if (ef_x87_empty(s, top)) {
        if (!ef_x87_stack_fault(s, 0)) return 0;
        flags = 1; out = (uint64_t)1 << (width - 1u);
    } else flags = ef_x87_integer_bits(s->x87[top], width, (s->x87_control >> 10) & 3u, &out);
    s->x87_status = (s->x87_status & ~0x200u) | flags;
    if (flags & ~s->x87_control & 0x3fu) {
        s->x87_status |= 0x8080u;
        if (flags & 1u) return 0;
    }
    ef_write(s, address, width == 64 ? 32 : width, (uint32_t)out);
    if (width == 64) ef_write(s, address + 4u, 32, (uint32_t)(out >> 32));
    if (pop) { s->x87_tag |= 3u << (top * 2u); ef_x87_top(s, top + 1u); }
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

BINARY_RUNTIME = r'''
#if defined(EF_LIFT_SOFTFLOAT_BINARY) && !defined(EF_LIFT_BINARY_RUNTIME)
#define EF_LIFT_BINARY_RUNTIME
unsigned ef_x87_binary_control(const uint32_t *, const uint32_t *, unsigned, unsigned, uint32_t *);
unsigned ef_x87_binary_operand(const uint32_t *, const uint32_t *, unsigned, unsigned, unsigned, uint32_t *);
static unsigned ef_x87_register_binary(ef_lift_state *s, unsigned operation,
                                      unsigned left, unsigned right, unsigned destination, unsigned pop) {
    uint32_t value[3] = {0, 0xc0000000u, 0xffffu}, flags;
    unsigned top = ef_x87_index(s, 0);
    if (s->x87_status & 0x80u) return 8;
    left = ef_x87_index(s, left); right = ef_x87_index(s, right);
    destination = ef_x87_index(s, destination);
    if (ef_x87_empty(s, left) || ef_x87_empty(s, right)) {
        if (!ef_x87_stack_fault(s, 0)) return 0;
    } else {
        flags = ef_x87_binary_control(s->x87[left], s->x87[right], operation, s->x87_control, value);
        if (flags & 0x10000u) return 9;
        s->x87_status = (s->x87_status & ~0x200u) | (flags & 0x23fu);
        if (flags & ~s->x87_control & 0x3fu) s->x87_status |= 0x8080u;
        if (flags & 0x20000u) return 0;
    }
    ef_x87_put(s, destination, value);
    if (pop) { s->x87_tag |= 3u << (top * 2); ef_x87_top(s, top + 1u); }
    return 0;
}
static unsigned ef_x87_memory_binary(ef_lift_state *s, uint32_t address, unsigned width,
                                    unsigned operation, unsigned reverse) {
    uint32_t value[3] = {0, 0xc0000000u, 0xffffu}, operand[3], flags, low, high = 0;
    unsigned top = ef_x87_index(s, 0), denormal;
    if (width != 32 && width != 64) return 9;
    if (s->x87_status & 0x80u) return 8;
    if (s->memory_size < width / 8u || address > s->memory_size - width / 8u) {
        s->fault = 3; return 3;
    }
    if (ef_x87_empty(s, top)) {
        if (!ef_x87_stack_fault(s, 0)) return 0;
    } else {
        low = ef_read(s, address, 32);
        if (width == 64) high = ef_read(s, address+4u, 32);
        denormal = (ef_x87_real_bits(low, high, width, operand) & 2u) != 0;
        flags = ef_x87_binary_operand(reverse ? operand : s->x87[top],
                                      reverse ? s->x87[top] : operand,
                                      operation, s->x87_control, denormal, value);
        if (flags & 0x10000u) return 9;
        s->x87_status = (s->x87_status & ~0x200u) | (flags & 0x23fu);
        if (flags & ~s->x87_control & 0x3fu) s->x87_status |= 0x8080u;
        if (flags & 0x20000u) return 0;
    }
    ef_x87_put(s, top, value);
    return 0;
}
#endif
'''

COMPARE_RUNTIME = r'''
static unsigned ef_x87_nan(const uint32_t *v) {
    return (v[2] & 0x7fffu) == 0x7fffu && ((v[1] & 0x7fffffffu) || v[0]);
}
static int ef_x87_order(const uint32_t *a, const uint32_t *b) {
    uint64_t left = ((uint64_t)a[1] << 32) | a[0], right = ((uint64_t)b[1] << 32) | b[0];
    int ae = (int)(a[2] & 0x7fffu), be = (int)(b[2] & 0x7fffu), order;
    unsigned negative = a[2] & 0x8000u;
    if (!left && !right && !ae && !be) return 0;
    if ((a[2] ^ b[2]) & 0x8000u) return negative ? -1 : 1;
    if (!ae) ae = 1;
    if (!be) be = 1;
    if (!left) ae = -32768;
    else while (!(left & ((uint64_t)1 << 63))) { left <<= 1; --ae; }
    if (!right) be = -32768;
    else while (!(right & ((uint64_t)1 << 63))) { right <<= 1; --be; }
    order = ae < be || (ae == be && left < right) ? -1 :
            ae > be || (ae == be && left > right) ? 1 : 0;
    return negative ? -order : order;
}
static unsigned ef_x87_compare_values(ef_lift_state *s, const uint32_t *a, const uint32_t *b,
                                      unsigned empty, unsigned quiet, unsigned pop, unsigned denormal) {
    unsigned nan_a, nan_b, unsupported, flags = 0, codes, unmasked, i, top = ef_x87_index(s, 0);
    int order;
    if (s->x87_compare_profile & ~3u) return 9;
    if (s->x87_status & 0x80u) return 8;
    nan_a = ef_x87_nan(a); nan_b = ef_x87_nan(b);
    unsupported = ((a[2] & 0x7fffu) && !(a[1] & 0x80000000u)) ||
                  ((b[2] & 0x7fffu) && !(b[1] & 0x80000000u));
    if (empty) flags = 0x41u;
    else if (unsupported || (nan_a && (!quiet || !(a[1] & 0x40000000u))) ||
                           (nan_b && (!quiet || !(b[1] & 0x40000000u)))) flags = 1;
    if (empty || unsupported || nan_a || nan_b) codes = 0x4500u;
    else {
        if (denormal || (!(a[2] & 0x7fffu) && (a[0] || a[1])) ||
                        (!(b[2] & 0x7fffu) && (b[0] || b[1]))) flags = 2;
        order = ef_x87_order(a, b);
        codes = order < 0 ? 0x100u : order == 0 ? 0x4000u : 0;
    }
    s->x87_status = (s->x87_status & ~0x200u) | flags;
    unmasked = flags & ~s->x87_control & 3u;
    if (!unmasked || (s->x87_compare_profile & (unmasked & 1u ? 1u : 2u)))
        s->x87_status = (s->x87_status & ~0x4500u) | codes;
    if (unmasked) { s->x87_status |= 0x8080u; return 0; }
    for (i = 0; i < pop; ++i) s->x87_tag |= 3u << (((top+i) & 7u)*2);
    if (pop) ef_x87_top(s, top+pop);
    return 0;
}
static unsigned ef_x87_compare_register(ef_lift_state *s, unsigned index, unsigned quiet, unsigned pop) {
    uint32_t zero[3] = {0, 0, 0};
    unsigned top = ef_x87_index(s, 0), other = ef_x87_index(s, index);
    return ef_x87_compare_values(s, s->x87[top], index == 8 ? zero : s->x87[other],
        ef_x87_empty(s, top) || (index != 8 && ef_x87_empty(s, other)), quiet, pop, 0);
}
static unsigned ef_x87_compare_memory(ef_lift_state *s, uint32_t address, unsigned width, unsigned pop) {
    uint32_t value[3], low, high = 0;
    unsigned top = ef_x87_index(s, 0), flags;
    if (width != 32 && width != 64) return 9;
    if (s->x87_status & 0x80u) return 8;
    if (s->memory_size < width/8u || address > s->memory_size-width/8u) { s->fault = 3; return 3; }
    low = ef_read(s, address, 32);
    if (width == 64) high = ef_read(s, address+4u, 32);
    flags = ef_x87_real_bits(low, high, width, value);
    return ef_x87_compare_values(s, s->x87[top], value, ef_x87_empty(s, top), 0, pop, (flags & 2u) != 0);
}
'''
