/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Marker, lookup and table helpers, 0x2f740..0x48ae8. */

/* 0x2f740, 160 bytes. Draw every marker of the 16-byte table that is in range. */
void draw_markers_2f740(void)
{
    int16_t i;

    if (EF_U16(0x0019bf74u) == 0)
        return;
    for (i = 0; i < (int16_t)EF_U16(0x0019bf74u); i++) {
        ef_addr32 record = 0x0019bc10u + (uint32_t)((int32_t)i * 16);
        int32_t dx = (int32_t)(EF_U32(0x001a5be8u) - EF_U32(record + 8u)) >> 16;
        int32_t dy = (int32_t)(EF_U32(0x001a5becu) - EF_U32(record)) >> 16;

        if ((int32_t)length_2d_decb0(dy, dx) >= 0xc350)
            continue;
        guest_fn_a0ea8((int16_t)EF_U16(record + 0xeu), 0, (int16_t)EF_U16(record + 0xcu), 0);
        guest_fn_9e0c4((int16_t)EF_U16(record + 0xeu), EF_U32(record), EF_U32(record + 4u), EF_U32(record + 8u));
    }
}

/* 0x37b44, 129 bytes. Index of the last 0x45-byte record whose position cell is (x, y), or 0xff. */
uint32_t find_record_at_cell_37b44(int16_t x, int16_t y)
{
    ef_addr32 table = EF_U32(0x001a14f8u);
    uint32_t found = 0xff;
    int16_t i;

    for (i = 0; i < (int16_t)EF_U16(0x001a1506u); i++) {
        ef_addr32 record = table + (uint32_t)((int32_t)i * 0x45);

        if ((int32_t)EF_U32(record + 7u) / 2048 == (int32_t)x && (int32_t)EF_U32(record + 0xbu) / 2048 == (int32_t)y)
            found = (uint32_t)(int32_t)i;
    }
    return found;
}

/* 0x38750, 144 bytes. Store a scaled set of 18 percentages, or mark the slot unused. */
void set_scaled_table_38750(uint16_t a, uint16_t b, uint16_t scale, uint8_t d, uint8_t stack_e, uint8_t stack_f, uint8_t stack_g)
{
    int16_t i;

    if (guest_fn_cd7f0((int32_t)(int8_t)stack_f) == 0) {
        EF_U8(0x00125acfu) = 0xff;
        return;
    }
    EF_U16(0x00125ac6u) = a;
    EF_U16(0x00125ac8u) = b;
    EF_U8(0x00125acdu) = d;
    EF_U8(0x00125acfu) = stack_f;
    EF_U16(0x00125acau) = scale;
    EF_U8(0x00125accu) = stack_e;
    EF_U8(0x00125aceu) = stack_g;
    for (i = 0; i < 0x12; i++) {
        int32_t base = (int16_t)EF_U16(0x00125aa2u + (uint32_t)((int32_t)i * 2));

        EF_U16(0x001a1510u + (uint32_t)((int32_t)i * 2)) = (uint16_t)(base * (int32_t)(int16_t)scale / 100);
    }
}

/* 0x39960, 170 bytes. Build the 0x1a17c0/0x1a1810 ellipse tables (radii 10 and 8). */
void build_circle_39960(void)
{
    int16_t i;

    EF_U8(0x00126350u) = 1;
    for (i = 0; i < 0x14; i++) {
        int32_t quotient = (int32_t)i * 0xffff / 0x14;
        uint32_t index = ((uint32_t)quotient & 0xffffu) >> 3;
        int32_t cosine = (int16_t)EF_U16(0x00141e08u + index * 2u);
        int32_t sine = (int16_t)EF_U16(0x00142e08u + index * 2u);
        uint32_t slot = (uint32_t)((int32_t)i * 4);

        EF_U16(0x001a17c0u + slot) = (uint16_t)((cosine * 10) >> 15);
        EF_U16(0x001a1810u + slot) = (uint16_t)((cosine * 8) >> 15);
        EF_U16(0x001a1812u + slot) = (uint16_t)((-(sine * 8)) >> 15);
        EF_U16(0x001a17c2u + slot) = (uint16_t)((-(sine * 10)) >> 15);
    }
}

/* 0x40428, 145 bytes. Clear the 320x200 view and refresh the unit selected by the current key. */
void refresh_selected_unit_40428(void)
{
    ef_addr32 unit;

    latch_values_ce328(0, 0x13f, 0, 0xc7);
    guest_fn_cdf48(0, 0x13f, 0, 0xc7);
    unit = guest_fn_7489c((int32_t)EF_U32(0x001f71c8u) >> 16);
    if (EF_U8(0x0012db9eu) == 0x3e)
        unit = guest_fn_7489c((int16_t)EF_U16(0x001a823cu));
    if (EF_U8(0x0012db9eu) == 0x2d)
        unit = guest_fn_7489c((int16_t)EF_U16(0x001a8236u));
    if (EF_U8(0x0012db9eu) == 0x2b && unit != 0)
        unit = guest_fn_7489c((int16_t)EF_U16(unit + 0x43u));
    if (unit != 0)
        guest_fn_401ac((int16_t)EF_U16(unit + 0x28u), 0, 0);
}

/* 0x48ae8, 140 bytes. Classify an entry: 0/1 for ordinary, 2 for kind 2, logs unknown kinds. */
uint32_t classify_entry_48ae8(int16_t unit_id, int16_t entry)
{
    uint8_t kind = EF_U8(0x00126a62u + (uint32_t)((int32_t)entry * 7));
    ef_addr32 unit;
    ef_addr32 record;

    if (kind == 2)
        return 2;
    if (kind != 0) {
        guest_fn_cd12e(0x00184730u, kind, (uint32_t)(int32_t)entry);
        return 0;
    }
    unit = guest_fn_7489c((int32_t)unit_id);
    if (unit == 0)
        return 0;
    record = EF_U32(0x0019bb58u) + (uint32_t)((int32_t)(int16_t)EF_U16(unit + 0x28u) * 0xca);
    if (EF_U16(record + 0x50u) == (uint32_t)(int32_t)unit_id)
        return 0;
    return 1;
}
