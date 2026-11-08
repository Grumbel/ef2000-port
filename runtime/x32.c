/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
 */

/* Translation of the NON-DPMI 3509 success path, original file 0x2bd8..0x2c0b.
 * Caller owns allocation-state initialization. No addresses from a particular
 * DOS run are hard-coded into this calculation.
 */
#include "x32.h"

int x32_stack_reserve(struct x32_allocation *state, u32 sp, u32 size,
                      u32 flags, struct x32_stack_result *result) {
    if (sp < size) return 0;
    u32 boundary = (sp - size) & 0xfffff000u;
    if (boundary < state->minimum_stack) return 0;
    if (state->arena_start < state->stack_end && state->arena_end > boundary) return 0;
    u32 consumed = state->stack_end - boundary;
    if (state->free_bytes < consumed) return 0;
    state->stack_end = boundary;
    state->free_bytes -= consumed;
    u32 before_dec = boundary - state->free_bytes;
    u32 limit = before_dec - 1;
    if (state->arena_start < state->stack_end && state->arena_end - 1 > limit)
        limit = state->arena_end - 1;
    /* Reproduce flags from DEC EDX, followed by the common success CLC. */
    flags &= ~0x8d5u; /* OF, SF, ZF, AF, PF, CF */
    if (limit & 0x80000000u) flags |= 0x80;
    if (!limit) flags |= 0x40;
    if ((before_dec ^ limit ^ 1) & 0x10) flags |= 0x10;
    if (before_dec == 0x80000000u) flags |= 0x800;
    u32 parity = limit & 255;
    parity ^= parity >> 4; parity ^= parity >> 2; parity ^= parity >> 1;
    if (!(parity & 1)) flags |= 4;
    result->low_limit = limit;
    result->flags = flags;
    return 1;
}

/* Core non-DPMI 3508 path, file 0x28b5..0x2911, with host memory already
 * reserved. Swap-file extension at 0x4821 is outside this no-swap memory model.
 */
int x32_heap_grow(struct x32_allocation *state, u32 requested, u32 flags,
                  struct x32_heap_result *result) {
    u32 rounded = (requested + 0xfff) & 0xfffff000u;
    if (rounded >= state->heap_ceiling) return 0;
    if (state->heap_end < state->arena_end && rounded > state->arena_start) return 0;
    if (rounded > state->heap_end) {
        u32 growth = rounded - state->heap_end;
        if (growth > state->free_bytes) return 0;
        state->free_bytes -= growth;
        state->heap_end = rounded;
    }
    u32 available_floor = state->stack_end - state->free_bytes;
    u32 difference = available_floor - state->minimum_stack;
    /* Final CMP EAX,minimum_stack; optional MOV; common CLC. */
    flags &= ~0x8d5u;
    if (difference & 0x80000000u) flags |= 0x80;
    if (!difference) flags |= 0x40;
    if ((available_floor ^ state->minimum_stack ^ difference) & 0x10) flags |= 0x10;
    if ((available_floor ^ state->minimum_stack) & (available_floor ^ difference) & 0x80000000u)
        flags |= 0x800;
    u32 parity = difference & 255;
    parity ^= parity >> 4; parity ^= parity >> 2; parity ^= parity >> 1;
    if (!(parity & 1)) flags |= 4;
    result->heap_end = state->heap_end;
    result->stack_low = available_floor > state->minimum_stack ? available_floor : state->minimum_stack;
    result->flags = flags;
    return 1;
}
