/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Map hotspot markers: seven near-identical per-type routines. */

/* Draw the markers of every list node whose type byte (+0xb) equals code. The marker position comes from the first matching table: the (row, column) word pairs at p1, p2 and p3 are used when the group-flag test passes (positive, negative, zero in turn) and the matching enable word at 0x126554/6/8 is set; a fourth source at 0x134740 is used when the record test 0xbc7ec passes. The position is a cell in the map buffer at 0x126570 (0x280 bytes per row). */
static void draw_hotspots_common(ef_addr32 head, ef_addr32 holder, uint32_t code, uint32_t p1, uint32_t p2, uint32_t p3)
{
    ef_addr32 node;

    for (node = EF_U32(holder); node != 0; node = EF_U32(node + 4u)) {
        uint8_t found = 0;
        ef_addr32 pos = 0;
        int32_t x;
        int32_t y;

        if (EF_U8(node + 0xbu) != code)
            continue;
        if ((uint16_t)group_flag_positive_cc22c(0, EF_U8(node + 9u), head) != 0 && EF_U16(0x00126554u) != 0) {
            pos = EF_U32(0x00126570u) + (uint32_t)((int32_t)EF_S16(p1 + 2u) * 0x280 + EF_S16(p1 - 2u + 2u));
            found = 1;
        } else if ((uint16_t)group_flag_negative_cc1ac(0, EF_U8(node + 9u), head) != 0 && EF_U16(0x00126556u) != 0) {
            pos = EF_U32(0x00126570u) + (uint32_t)((int32_t)EF_S16(p2 + 2u) * 0x280 + EF_S16(p2 - 2u + 2u));
            found = 1;
        } else if ((uint16_t)group_flag_zero_cc1ec(0, EF_U8(node + 9u), head) != 0 && EF_U16(0x00126558u) != 0) {
            pos = EF_U32(0x00126570u) + (uint32_t)((int32_t)EF_S16(p3 + 2u) * 0x280 + EF_S16(p3 - 2u + 2u));
            found = 1;
        }
        if ((uint16_t)guest_fn_bc7ec(node) != 0) {
            pos = EF_U32(0x00126570u) + (uint32_t)((int32_t)EF_S16(0x00134744u) * 0x280 + EF_S16(0x00134742u));
            found = 1;
        }
        if (found == 0)
            continue;
        y = (int16_t)(0x190 - EF_S16(node + 0x1au) - 0xa);
        x = (int16_t)(EF_S16(node + 0x16u) - 0xa);
        guest_fn_ca620((int32_t)pos, x, y, 0x14u, 0x14u, 0x280u);
        if (EF_U16(0x0012656au) != 0)
            guest_fn_440f4_x(EF_U8(node + 9u), x, y, 0x14u, 0x1au, 0x280u);
    }
}

/* 0x447ec, 382 bytes. Draw the hotspot markers for list type 0x1d. */
void draw_hotspots_447ec(ef_addr32 head, ef_addr32 holder)
{
    draw_hotspots_common(head, holder, 0x1d, 0x00134418u, 0x00134406u, 0x0013440fu);
}

/* 0x4496c, 382 bytes. Draw the hotspot markers for list type 0x13. */
void draw_hotspots_4496c(ef_addr32 head, ef_addr32 holder)
{
    draw_hotspots_common(head, holder, 0x13, 0x0013442au, 0x00134421u, 0x00134433u);
}

/* 0x44c68, 382 bytes. Draw the hotspot markers for list type 0x2b. */
void draw_hotspots_44c68(ef_addr32 head, ef_addr32 holder)
{
    draw_hotspots_common(head, holder, 0x2b, 0x00134460u, 0x00134457u, 0x00134469u);
}

/* 0x44de8, 382 bytes. Draw the hotspot markers for list type 0x26. */
void draw_hotspots_44de8(ef_addr32 head, ef_addr32 holder)
{
    draw_hotspots_common(head, holder, 0x26, 0x0013447bu, 0x00134472u, 0x00134484u);
}

/* 0x450f4, 382 bytes. Draw the hotspot markers for list type 0x5. */
void draw_hotspots_450f4(ef_addr32 head, ef_addr32 holder)
{
    draw_hotspots_common(head, holder, 0x5, 0x001344a8u, 0x0013449fu, 0x001344b1u);
}

/* 0x45274, 382 bytes. Draw the hotspot markers for list type 0x49. */
void draw_hotspots_45274(ef_addr32 head, ef_addr32 holder)
{
    draw_hotspots_common(head, holder, 0x49, 0x001344c3u, 0x001344bau, 0x001344ccu);
}

/* 0x453f4, 382 bytes. Draw the hotspot markers for list type 0x4f. */
void draw_hotspots_453f4(ef_addr32 head, ef_addr32 holder)
{
    draw_hotspots_common(head, holder, 0x4f, 0x001344deu, 0x001344d5u, 0x001344e7u);
}

/* Like draw_hotspots_common, but the type byte is at +0xa, the group index at +0xb and the marker is placed from the extent record behind +0x13 (word +8 = x, word +0xc = y). */
static void draw_extent_markers_common(ef_addr32 head, ef_addr32 holder, uint32_t code, uint32_t p1, uint32_t p2, uint32_t p3)
{
    ef_addr32 node;

    for (node = EF_U32(holder); node != 0; node = EF_U32(node + 4u)) {
        uint8_t found = 0;
        ef_addr32 pos = 0;
        ef_addr32 ext;
        int32_t x;
        int32_t y;

        if (EF_U8(node + 0xau) != code)
            continue;
        if ((uint16_t)group_flag_positive_cc22c(0, EF_U8(node + 0xbu), head) != 0 && EF_U16(0x00126554u) != 0) {
            pos = EF_U32(0x00126570u) + (uint32_t)((int32_t)EF_S16(p1 + 2u) * 0x280 + EF_S16(p1));
            found = 1;
        } else if ((uint16_t)group_flag_negative_cc1ac(0, EF_U8(node + 0xbu), head) != 0 && EF_U16(0x00126556u) != 0) {
            pos = EF_U32(0x00126570u) + (uint32_t)((int32_t)EF_S16(p2 + 2u) * 0x280 + EF_S16(p2));
            found = 1;
        } else if ((uint16_t)group_flag_zero_cc1ec(0, EF_U8(node + 0xbu), head) != 0 && EF_U16(0x00126558u) != 0) {
            pos = EF_U32(0x00126570u) + (uint32_t)((int32_t)EF_S16(p3 + 2u) * 0x280 + EF_S16(p3));
            found = 1;
        }
        if (found == 0)
            continue;
        ext = EF_U32(node + 0x13u);
        y = (int16_t)(0x190 - EF_S16(ext + 0xcu) - 0xa);
        x = (int16_t)(EF_S16(ext + 8u) - 0xa);
        guest_fn_ca620((int32_t)pos, x, y, 0x14u, 0x14u, 0x280u);
        if (EF_U16(0x0012656au) != 0)
            guest_fn_440f4_x(EF_U8(node + 0xbu), x, y, 0x14u, 0x1au, 0x280u);
    }
}

/* 0x43834, 368 bytes. Draw the extent markers for list type 0xe. */
void draw_extent_markers_43834(ef_addr32 head, ef_addr32 holder)
{
    draw_extent_markers_common(head, holder, 0xe, 0x00134325u, 0x0013431cu, 0x0013432eu);
}

/* 0x439a4, 368 bytes. Draw the extent markers for list type 0x17. */
void draw_extent_markers_439a4(ef_addr32 head, ef_addr32 holder)
{
    draw_extent_markers_common(head, holder, 0x17, 0x00134325u, 0x0013431cu, 0x0013432eu);
}

/* 0x43c84, 368 bytes. Draw the extent markers for list type 0xf. */
void draw_extent_markers_43c84(ef_addr32 head, ef_addr32 holder)
{
    draw_extent_markers_common(head, holder, 0xf, 0x00134340u, 0x00134337u, 0x00134349u);
}
