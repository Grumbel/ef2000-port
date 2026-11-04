/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Node pool, countdown and scaled-multiply helpers, 0x65410..0x833a0. */

/* 0x65410, 135 bytes. Move a node from the free list to the head of the active list and fill it. */
void activate_node_65410(uint32_t a, uint32_t b, uint32_t c, uint8_t kind,
                         uint16_t stack_word, uint8_t stack_b, uint8_t stack_c)
{
    ef_addr32 node = EF_U32(0x001a7c58u);
    ef_addr32 next_free = node;

    if (node != 0) {
        ef_addr32 head;

        next_free = EF_U32(node + 0x16u);
        if (next_free != 0)
            EF_U32(next_free + 0x12u) = 0;
        head = EF_U32(0x001a7c54u);
        EF_U32(node + 0x12u) = 0;
        if (head != 0)
            EF_U32(head + 0x12u) = node;
        EF_U16(node + 0x1au) = 0;
        EF_U8(node + 0xdu) = 0;
        EF_U32(node) = a;
        EF_U32(node + 4u) = b;
        EF_U32(node + 8u) = c;
        EF_U8(node + 0xcu) = kind;
        EF_U16(node + 0xeu) = stack_word;
        EF_U8(node + 0x10u) = stack_b;
        EF_U8(node + 0x11u) = stack_c;
        head = EF_U32(0x001a7c54u);
        EF_U32(0x001a7c54u) = node;
        EF_U16(0x001a81aau) = (uint16_t)(EF_U16(0x001a81aau) + 1u);
        EF_U32(node + 0x16u) = head;
    }
    EF_U32(0x001a7c58u) = next_free;
}

/* 0x6583c, 143 bytes. Advance the vertical countdown; step to the next frame when it expires. */
void advance_countdown_y_6583c(void)
{
    uint16_t value = EF_U16(0x001a81a8u);
    uint8_t frame = EF_U8(0x001a81dcu);

    if (EF_U8(0x001a81deu) == 0) {
        frame = 0;
        value = 0;
    } else {
        int16_t limit;

        value = (uint16_t)(value + (uint16_t)(EF_U8(0x001a81deu) * EF_U16(0x00127d92u)));
        limit = (int16_t)EF_U16(0x001282b2u + (uint32_t)frame * 2u);
        if ((int16_t)value > limit) {
            value = (uint16_t)(value - (uint16_t)limit);
            EF_U16(0x001a81a8u) = value;
            guest_fn_29dd0((int32_t)EF_U32(0x001f71c8u) >> 16);
            frame = (uint8_t)(EF_U8(0x001a81dcu) + 1u);
            value = EF_U16(0x001a81a8u);
            if (frame >= 4)
                frame = 0;
        }
        EF_U8(0x001a81deu) = 0;
    }
    EF_U8(0x001a81dcu) = frame;
    EF_U16(0x001a81a8u) = value;
}

/* 0x658cc, 143 bytes. Advance the horizontal countdown; step to the next frame when it expires. */
void advance_countdown_x_658cc(void)
{
    uint16_t value = EF_U16(0x001a81a6u);
    uint8_t frame = EF_U8(0x001a81dbu);

    if (EF_U8(0x001a81ddu) == 0) {
        frame = 0;
        value = 0;
    } else {
        int16_t limit;

        value = (uint16_t)(value + (uint16_t)(EF_U8(0x001a81ddu) * EF_U16(0x00127d92u)));
        limit = (int16_t)EF_U16(0x001282bau + (uint32_t)frame * 2u);
        if ((int16_t)value > limit) {
            value = (uint16_t)(value - (uint16_t)limit);
            EF_U16(0x001a81a6u) = value;
            guest_fn_29b14((int32_t)EF_U32(0x001f71c8u) >> 16);
            frame = (uint8_t)(EF_U8(0x001a81dbu) + 1u);
            value = EF_U16(0x001a81a6u);
            if (frame >= 4)
                frame = 0;
        }
        EF_U8(0x001a81ddu) = 0;
    }
    EF_U8(0x001a81dbu) = frame;
    EF_U16(0x001a81a6u) = value;
}

/* 0x7aa3c, 132 bytes. Pick two random table values for the current stream slot. */
void pick_random_pair_7aa3c(void)
{
    uint32_t slot = EF_U32(0x001f8ea0u);
    uint32_t first;
    uint32_t second;

    EF_U32(0x001f8e08u + slot * 12u) = EF_U32(0x001f8ea4u);
    first = guest_fn_b3f58((int32_t)(int16_t)guest_fn_b3db0(0x27100u));
    EF_U32(0x001f8e00u + EF_U32(0x001f8ea0u) * 12u) = first;
    EF_U32(0x001f8eb8u) = first;
    second = guest_fn_b3f58((int32_t)(int16_t)guest_fn_b3db0(0x27100u));
    EF_U32(0x001f8e04u + EF_U32(0x001f8ea0u) * 12u) = second;
    EF_U32(0x001f8eb4u) = second;
}

/* 0x82168, 142 bytes. Claim the first free (0xff) slot in a row of eight and link both tables. */
void claim_slot_82168(int16_t owner, int16_t row, uint8_t kind)
{
    uint8_t claimed = 0;
    uint8_t i;

    for (i = 0; i < 8; i++) {
        ef_addr32 base = (ef_addr32)((int32_t)row * 0x11) + EF_U32(0x001fcb40u);
        ef_addr32 entry = base + (uint32_t)i * 2u;

        if ((int16_t)EF_U16(entry + 1u) == 0xff && claimed == 0) {
            EF_U16(entry + 1u) = (uint16_t)owner;
            EF_U8(base) = kind;
            if (kind == 0)
                EF_U16(EF_U32(0x0019bb58u) + (uint32_t)((int32_t)owner * 0xca) + 0x1cu) = (uint16_t)row;
            else
                EF_U16(EF_U32(0x001a14f8u) + (uint32_t)((int32_t)owner * 0x45) + 0x21u) = (uint16_t)row;
            claimed = 1;
        }
    }
}

/* 0x833a0, 131 bytes. Compute a*b/c, saturating to +/-0x7fffffff-ish when the product overflows. */
int32_t muldiv_saturate_833a0(int32_t a, int32_t b, int32_t c)
{
    int64_t product = (int64_t)(int32_t)abs_cf94a(a) * (int64_t)(int32_t)abs_cf94a(b);
    int64_t limit = (int64_t)(int32_t)abs_cf94a(c) * (int64_t)0x7fffffff;

    if (product > limit) {
        if ((a ^ b ^ c) < 0)
            return (int32_t)0x80000000u;
        return 0x7fffffff;
    }
    if (c == 0)
        return 0; /* original traps on divide by zero */
    return (int32_t)(((int64_t)a * (int64_t)b) / (int64_t)c);
}
