/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Position range checks, tactical entry and map redraw helpers. */

/* 0x509bc, 96 bytes. Select the tactical code from a two-digit field and apply or bind it. */
void apply_tactical_code_509bc(ef_addr32 record)
{
    int32_t n = (int16_t)(EF_U8(record + 0x42u) - 0x30u);
    uint32_t b;

    if (n == 1) {
        b = EF_U8(record + 0x43u);
        if ((int32_t)b >= 0x30 && (int32_t)b <= 0x39)
            n = (int32_t)b - 0x26;
    }
    if (tactical_lookup_contains(2) != 0) {
        update_dialog_bindings(0x001326e7u, 0x00132704u);
        return;
    }
    guest_fn_50b68_w(2, (uint32_t)n & 0xffu);
    guest_fn_5086c_w();
}

/* 0x3dbf0, 189 bytes. Flag the player unit as out of the map when its cell leaves the valid range. */
void check_unit_leaves_map_3dbf0(void)
{
    int32_t id = EF_S32(0x001f71c8u) >> 16;
    int16_t x;
    int16_t y;

    if (id == 0xff)
        return;
    if (guest_fn_7489c(id) == 0)
        return;
    x = (int16_t)(EF_S32(0x001a5becu) >> 11);
    y = (int16_t)(EF_S32(0x001a5be8u) >> 11);
    if (x <= 0x18d && y <= 0x18d && x >= 2 && y >= 2)
        return;
    if (EF_U8(0x001a3647u) != 0)
        EF_U8(0x001a3647u) = 0;
    configure_navigation_record(0x00130e15u);
    if (x < 0x18e && y < 0x18e && x >= 1 && y >= 1)
        return;
    EF_U8(0x001a3623u) = 1;
    if (EF_U8(0x00127da9u) != 0) {
        configure_scene_feature(0xff);
        return;
    }
    EF_U16(0x00126518u) = 1;
}

/* 0x7a9bc, 111 bytes. Replay the 400x400 byte-pair grid through the plane source when it is enabled. */
void replay_plane_grid_7a9bc(void)
{
    int32_t x;
    int32_t off;

    if (EF_U8(0x0012afbdu) == 0)
        return;
    select_plane_source(0);
    guest_fn_a1b54_w(0x001f8db0u, 2, 0);
    for (x = 0; x < 0x190; x++) {
        for (off = 0; off != 0x27100; off += 0x190) {
            uint32_t hi = EF_U8(EF_U32(0x001f8eb4u) + (uint32_t)off + (uint32_t)x);
            uint32_t lo = EF_U8(EF_U32(0x001f8eb8u) + (uint32_t)off + (uint32_t)x);
            uint32_t v = (hi << 8) | lo;

            if ((uint16_t)v != 0xffffu)
                guest_fn_a1d00_w((int16_t)v);
        }
    }
    guest_fn_a1cb4();
}

/* 0x5a998, 117 bytes. Draw the tactical coordinates of the player unit from the current table row. */
void draw_player_coordinates_5a998(void)
{
    ef_addr32 row = 0x00128170u + (uint32_t)EF_U8(0x001a6d1bu) * 15u;
    int16_t x = (int16_t)EF_U16(row);
    int32_t y = ((EF_S32(row + 4u) >> 16) * 3) / 6;
    ef_addr32 obj = guest_fn_7489c(EF_S32(0x001f71c8u) >> 16);

    if (obj == 0)
        return;
    guest_fn_5c7dc_w(obj);
    draw_tactical_coordinates(x, (int16_t)y, 0x00185ed0u);
    if ((EF_S32(0x001a8240u) >> 16) != -1)
        guest_fn_59adc_w();
}
