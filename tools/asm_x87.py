# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Raw x87 stack primitives for the explicit-state translator.

These model register/tag/status effects, not instruction environment updates.
The instruction renderer must separately supply guest IP/opcode/selector effects.
No original instructions are executed by these helpers.
"""

import re


def stack_operation(mnemonic, text):
    unary = {'fld1': 0, 'fldz': 1, 'fchs': 3, 'fabs': 4, 'fincstp': 9, 'fdecstp': 10}
    if mnemonic in unary and not text:
        return unary[mnemonic], 0
    register = {'fld': 2, 'fxch': 5, 'fst': 6, 'fstp': 7, 'ffree': 8}
    match = re.fullmatch(r'st(?:\(([0-7])\))?', text.strip().lower())
    if mnemonic in register and match:
        return register[mnemonic], int(match[1] or 0)
    return None


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
