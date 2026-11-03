/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* File-handle, entity-update and script helpers, 0x9cb20..0xacb68. */

/* 0x9cb20, 78 bytes. Try the two open archives, then fall back to 0xb3b90. */
uint32_t find_in_archives_9cb20(uint32_t key)
{
    uint32_t r;
    if (EF_U32(0x0040d1f0u) != 0 && (EF_U8(0x00132ab0u) & 1u)) {
        r = guest_fn_eb9f8(key, EF_U32(0x0040d1f0u));
        if (r != 0)
            return r;
    }
    if (EF_U32(0x0040d1f4u) != 0 && (EF_U8(0x00132ab0u) & 2u)) {
        r = guest_fn_eb9f8(key, EF_U32(0x0040d1f4u));
        if (r != 0)
            return r;
    }
    return guest_fn_b3b90(key);
}

/* 0x9cb70, 84 bytes. As 0x9cb20 with 0xeba4c and 0xa7b3c; -1 becomes 0. */
uint32_t lookup_in_archives_9cb70(uint32_t key)
{
    uint32_t r;
    if (EF_U32(0x0040d1f0u) != 0 && (EF_U8(0x00132ab0u) & 1u)) {
        r = lookup_field_eba4c(key, EF_U32(0x0040d1f0u));
        if (r != 0)
            return r;
    }
    if (EF_U32(0x0040d1f4u) != 0 && (EF_U8(0x00132ab0u) & 2u)) {
        r = lookup_field_eba4c(key, EF_U32(0x0040d1f4u));
        if (r != 0)
            return r;
    }
    r = guest_fn_a7b3c(key);
    return r == 0xffffffffu ? 0 : r;
}

/* 0x9ce84, 98 bytes. Dispatch a close request by flag bits 0x10000/0x20000/0x40000. */
void close_request_9ce84(uint32_t flags)
{
    uint32_t rest = flags & 0xfffcffffu;
    if (flags & 0x30000u) {
        if (flags & 0x10000u)
            release_or_forward_ebcd8(rest, EF_U32(0x0040d1f0u));
        else
            release_or_forward_ebcd8(rest, EF_U32(0x0040d1f4u));
        return;
    }
    if (flags & 0x40000u) {
        guest_fn_c3f80(EF_U32(0x0040d1e8u));
        EF_U32(0x0040d1e8u) = 0;
        return;
    }
    guest_fn_a7990(flags);
}

/* 0x9d4cd, 87 bytes. atoi: skip class-2 characters, optional sign, class-0x20 digits. */
int32_t parse_int(ef_addr32 text)
{
    int32_t value = 0;
    uint8_t sign;
    while (EF_U8(0x00195af0u + (uint8_t)(EF_U8(text) + 1u)) & 2u)
        ++text;
    sign = EF_U8(text);
    if (sign == 0x2b || sign == 0x2d)
        ++text;
    while (EF_U8(0x00195af0u + (uint8_t)(EF_U8(text) + 1u)) & 0x20u) {
        value = value * 10 + (int32_t)EF_U8(text) - 0x30;
        ++text;
    }
    return sign == 0x2d ? -value : value;
}

/* 0xa4380, 161 bytes. strcmp: 0, -1 or 1. */
int32_t guest_strcmp(ef_addr32 first, ef_addr32 second)
{
    if (first == second)
        return 0;
    for (;;) {
        uint8_t a = EF_U8(first);
        uint8_t b = EF_U8(second);
        if (a != b)
            return a < b ? -1 : 1;
        if (a == 0)
            return 0;
        ++first;
        ++second;
    }
}

/* 0xa5000, 111 bytes. Clamp two word fields so that they fit inside the extents at +0x43 / +0x45. */
void clamp_extents_a5000(ef_addr32 object)
{
    int32_t room = (EF_S32(object + 0x43u) >> 16) - (int32_t)(EF_U16(object + 0x61u) + EF_U16(object + 0x63u));
    if ((int32_t)EF_U16(object + 0x65u) < room)
        EF_U16(object + 0x65u) = (uint16_t)(EF_U16(object + 0x45u) - (EF_U16(object + 0x61u) + EF_U16(object + 0x63u)));
    room = (EF_S32(object + 0x45u) >> 16) - (int32_t)(EF_U16(object + 0x5du) + EF_U16(object + 0x5fu));
    if ((int32_t)EF_U16(object + 0x67u) < room)
        EF_U16(object + 0x67u) = (uint16_t)(EF_U16(object + 0x47u) - (EF_U16(object + 0x5du) + EF_U16(object + 0x5fu)));
}

/* 0xa59d8, 144 bytes. Run the per-object initialisation steps once for every entity (1..word 0x41eed8). */
void init_entities_a59d8(void)
{
    uint32_t i;
    for (i = 1; (uint16_t)i <= EF_U16(0x0041eed8u); ++i) {
        ef_addr32 object = EF_U32(0x0041edfcu + i * 4u);
        if (EF_U8(object + 0x3bu) & 0x80u)
            continue;
        guest_fn_a4ec0(object);
        guest_fn_a4eec(object);
        clamp_extents_a5000(object);
        guest_fn_a5070(object);
        guest_fn_a54f0(object);
        guest_fn_a5764(object);
        guest_fn_a66a0(object);
        guest_fn_a6998(object);
        guest_fn_a6a64(object);
        init_timers_a6940(object);
        guest_fn_a59ac(object);
        guest_fn_a51dc(object);
        EF_U8(object + 0x3bu) |= 0x80u;
    }
}

/* 0xa5a68, 84 bytes. */
void update_entities_a5a68(void)
{
    uint32_t i;
    for (i = 1; (uint16_t)i <= EF_U16(0x0041eed8u); ++i) {
        ef_addr32 object = EF_U32(0x0041edfcu + i * 4u);
        if (object != 0 && (EF_U8(object + 0x3bu) & 2u) && (EF_U8(object + 0x3bu) & 4u)) {
            guest_fn_a4a94(i);
            guest_fn_a499c(i);
        }
    }
}

/* 0xa6940, 86 bytes. Walk the child list at +0x91 (next at +0x21) and default the timers at +0x14 / +0x16. */
void init_timers_a6940(ef_addr32 object)
{
    ef_addr32 child = EF_U32(object + 0x91u);
    while (child != 0) {
        if (EF_U16(child + 0x59u) != 0 && EF_U16(child + 0x14u) == 0) {
            EF_U16(child + 0x14u) = EF_U16(child + 8u);
            if (EF_U16(child + 0x14u) == 0)
                EF_U16(child + 0x14u) = 0x10;
        }
        if (EF_U16(child + 0x63u) != 0 && EF_U16(child + 0x16u) == 0) {
            EF_U16(child + 0x16u) = EF_U16(child + 8u);
            if (EF_U16(child + 0x16u) == 0)
                EF_U16(child + 0x16u) = 0x10;
        }
        child = EF_U32(child + 0x21u);
    }
}

/* 0xa7328, 76 bytes. Toggle flag bit 0 at +0x1d and propagate it. */
void toggle_active_a7328(ef_addr32 object)
{
    EF_U8(object + 0x1du) = (uint8_t)((EF_U8(object + 0x1du) & 0xfeu) | ((EF_U8(object + 0x1du) & 1u) == 0));
    if ((EF_U8(object + 0x1du) & 1u) && (EF_U8(object + 0x85u) & 0x1fu))
        guest_fn_a729c(EF_U32(0x0041edfcu + (uint32_t)EF_U8(object + 0x50u) * 4u), object);
    guest_fn_a6d00(object);
}

/* 0xa89f0, 72 bytes. Wait up to 600 polls for key 0x19. */
void wait_for_key_a89f0(void)
{
    int16_t i;
    if (EF_U8(0x001371fdu) == 0)
        return;
    EF_U8(0x0041f833u) = 0;
    for (i = 0; i < 0x258; ++i) {
        guest_fn_dc2e0();
        if (EF_U8(0x0041f833u) == 0x19) {
            EF_U8(0x0041f833u) = 0;
            break;
        }
    }
    EF_U8(0x001371fdu) = 0;
}

/* Common tail of the three script comparison handlers. */
static void script_branch_taken(ef_addr32 op)
{
    if (EF_U8(op + 0xau) == 1) {
        EF_U32(0x00418e38u + (uint32_t)(EF_S32(0x004194aau) >> 16) * 4u) = EF_U32(0x004191dcu);
        EF_U16(0x004194acu) = (uint16_t)(EF_U16(0x004194acu) + 1u);
    }
    EF_U32(0x004191ecu) = EF_U32(op + 0xdu);
}

/* 0xa8f80, 73 bytes. Branch when the variable equals the operand. */
void script_if_equal_a8f80(ef_addr32 op)
{
    int32_t value = EF_S32(guest_fn_a7ec4(EF_U32(op + 1u) + 9u));
    if (value == EF_S32(op + 6u))
        script_branch_taken(op);
}

/* 0xa8fcc, 71 bytes. Branch when the variable differs. */
void script_if_not_equal_a8fcc(ef_addr32 op)
{
    int32_t value = EF_S32(guest_fn_a7ec4(EF_U32(op + 1u) + 9u));
    if (value != EF_S32(op + 6u))
        script_branch_taken(op);
}

/* 0xa9014, 71 bytes. Branch when the variable is not greater than the operand. */
void script_if_not_greater_a9014(ef_addr32 op)
{
    int32_t value = EF_S32(guest_fn_a7ec4(EF_U32(op + 1u) + 9u));
    if (!(value > EF_S32(op + 6u)))
        script_branch_taken(op);
}

/* 0xacb68, 124 bytes. Latch "previous" copies of the fields and shift flag bits 0..2 to 3..5 of byte +0x1e. */
void latch_previous_acb68(ef_addr32 record)
{
    uint8_t flags;
    EF_U32(record + 8u) = EF_U32(record + 4u);
    EF_U16(record + 2u) = EF_U16(record);
    EF_U32(record + 0x14u) = EF_U32(record + 0x10u);
    EF_U16(record + 0xeu) = EF_U16(record + 0xcu);
    EF_U8(record + 0x1du) = EF_U8(record + 0x1cu);
    flags = EF_U8(record + 0x1eu);
    flags = (uint8_t)((flags & 0xf7u) | ((flags & 1u) << 3));
    flags = (uint8_t)((flags & 0xefu) | (((flags >> 1) & 1u) << 4));
    flags = (uint8_t)((flags & 0xdfu) | (((flags >> 2) & 1u) << 5));
    EF_U8(record + 0x1eu) = flags;
    EF_U16(record + 0x21u) = EF_U16(record + 0x1fu);
    EF_U16(record + 0x25u) = EF_U16(record + 0x23u);
}
