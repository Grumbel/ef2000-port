/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Slot scans, reset and circle-table builders, 0x173c4..0x1fe4c. */

/* 0x173c4, 112 bytes. Scan the owner's linked units for a slot code of 6 or 0x15. */
uint32_t find_special_slot_173c4(int16_t owner)
{
    int32_t offset;

    for (offset = 0; offset < 0xe; offset += 2) {
        ef_addr32 record = EF_U32(0x0019bb58u) + (uint32_t)((int32_t)owner * 0xca) + (uint32_t)offset;
        uint32_t linked = EF_U16(record + 0x50u);
        ef_addr32 unit;
        int32_t slot;

        if (linked == 0xff)
            continue;
        unit = guest_fn_7489c((int16_t)linked);
        if (unit == 0)
            continue;
        for (slot = 0; slot < 0x14; slot++) {
            uint16_t code = (uint16_t)(int16_t)(int8_t)EF_U8(unit + 0x19bu + (uint32_t)slot * 3u);

            if (code == 6)
                return 6;
            if (code == 0x15)
                return 0x15; /* the original leaves the high word of EAX unspecified here */
        }
    }
    return 0;
}

/* 0x1d318, 149 bytes. Reset the game-loop state and run ten warm-up ticks. */
void reset_loop_state_1d318(void)
{
    int16_t i;

    guest_fn_1a580();
    EF_U8(0x001a3649u) = 0;
    EF_U16(0x00198430u) = 0;
    EF_U32(0x001a3104u) = 0;
    EF_U8(0x00198463u) = 0;
    EF_U32(0x001a3100u) = 0;
    EF_U16(0x00198424u) = 0;
    EF_U8(0x001264e0u) = 0;
    EF_U8(0x0015bb48u) = 1;
    EF_U8(0x00132ac2u) = 0;
    guest_fn_18a68();
    for (i = 0; i < 10; i++)
        guest_fn_5331c();
    EF_U8(0x001a363bu) = 0;
    EF_U16(0x0019844au) = 0;
    EF_U8(0x00198465u) = 0;
    EF_U8(0x001a363cu) = 0;
    EF_U16(0x0019844eu) = 0;
}

/* Fill 20 (cos, sin) pairs of an ellipse with radii r1, r2 over the step multiplier. */
static void fill_circle(int32_t step, int32_t r1, int32_t r2, uint32_t first_base, uint32_t second_base)
{
    int16_t i;

    for (i = 0; i < 0x14; i++) {
        int32_t quotient = (int32_t)i * step / 0x14;
        uint32_t index = ((uint32_t)quotient & 0xffffu) >> 3;
        int32_t cosine = (int16_t)EF_U16(0x00141e08u + index * 2u);
        int32_t sine = (int16_t)EF_U16(0x00142e08u + index * 2u);
        uint32_t slot = (uint32_t)((int32_t)i * 4);

        EF_U16(first_base + slot) = (uint16_t)((r1 * cosine) >> 15);
        EF_U16(second_base + slot) = (uint16_t)((r2 * cosine) >> 15);
        EF_U16(first_base + 2u + slot) = (uint16_t)((-(r1 * sine)) >> 15);
        EF_U16(second_base + 2u + slot) = (uint16_t)((-(r2 * sine)) >> 15);
    }
}

/* 0x1fd88, 195 bytes. Build the 0x19b030/0x19b080 circle tables (full turn is 0xc000 steps). */
void build_circle_1fd88(void)
{
    EF_U8(0x001237f8u) = 1;
    fill_circle(0xc000, (int16_t)EF_U16(0x001237fau), (int16_t)EF_U16(0x001237fcu), 0x0019b030u, 0x0019b080u);
}

/* 0x1fe4c, 192 bytes. Build the 0x19afe0/0x19af90 circle tables from the two radius bytes. */
void build_circle_1fe4c(void)
{
    EF_U8(0x001237feu) = 1;
    fill_circle(0xffff, EF_U8(0x001237ffu), EF_U8(0x00123800u), 0x0019afe0u, 0x0019af90u);
}
