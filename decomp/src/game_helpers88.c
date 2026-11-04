/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Bit pattern table, distance approximation, bounds and map-cell record scans. */

/* 0xc2f80, 125 bytes. Fill a 512-byte table: for each of the 8 bit values and 16 nibble patterns write four bytes (the bit value where the pattern bit is set, else 0). */
void fill_bit_pattern_table_c2f80(ef_addr32 table)
{
    uint32_t bit;
    uint32_t pat;

    for (bit = 1; bit < 0x100; bit += bit) {
        for (pat = 0; pat < 16; pat++) {
            EF_U8(table) = (pat & 8u) ? (uint8_t)bit : 0;
            EF_U8(table + 1u) = (pat & 4u) ? (uint8_t)bit : 0;
            EF_U8(table + 2u) = (pat & 2u) ? (uint8_t)bit : 0;
            EF_U8(table + 3u) = (pat & 1u) ? (uint8_t)bit : 0;
            table += 4u;
        }
    }
}

/* 0xce578, 125 bytes. Approximate 3D distance: 15/16 of the largest absolute component plus 3/8 of the other two. */
int32_t approx_distance3_ce578(int32_t a, int32_t b, int32_t c)
{
    int32_t big = a < 0 ? -a : a;
    int32_t mid = b < 0 ? -b : b;
    int32_t small = c < 0 ? -c : c;
    int32_t t;

    if (mid > big) { t = big; big = mid; mid = t; }
    if (small > big) { t = big; big = small; small = t; }
    if (small > mid) { t = mid; mid = small; small = t; }
    return ((mid + small) * 3) / 8 + (big * 15) / 16;
}

/* 0xd1fc0, 127 bytes. Compute (once) the 2D bounding box of an object's vertex list into fields 0x10..0x1c. */
void compute_vertex_bounds_d1fc0(ef_addr32 object)
{
    ef_addr32 list;
    ef_addr32 cur;
    ef_addr32 end;
    int32_t max_x = (int32_t)0x80000000;
    int32_t min_x = 0x7fffffff;
    int32_t max_y = (int32_t)0x80000000;
    int32_t min_y = 0x7fffffff;

    if ((EF_U8(object + 0x2cu) & 2u) != 0)
        return;
    list = EF_U32(object + 0x4cu);
    cur = list + 4u;
    end = cur + EF_U32(list) * 4u;
    do {
        ef_addr32 v = EF_U32(cur);
        int32_t x = EF_S32(v + 0x18u);
        int32_t y = EF_S32(v + 0x1cu);
        if (y < min_y) min_y = y;
        if (y > max_y) max_y = y;
        if (x < min_x) min_x = x;
        if (x > max_x) max_x = x;
        cur += 4u;
    } while (cur < end);
    EF_S32(object + 0x14u) = max_x;
    EF_S32(object + 0x10u) = min_x;
    EF_S32(object + 0x1cu) = max_y;
    EF_S32(object + 0x18u) = min_y;
    EF_U8(object + 0x2cu) = (uint8_t)(EF_U8(object + 0x2cu) | 2u);
}

/* 0xcb55c, 155 bytes. Mark records whose map cell (position / 10, 40 cells per row) passes the cc1ac test as used. */
void mark_records_by_map_cell_cb55c(ef_addr32 list, ef_addr32 map, uint32_t extra)
{
    ef_addr32 rec = EF_U32(list);

    while (rec != 0) {
        uint32_t type = EF_U8(rec + 0xdu);
        if (type != 1 && type != 7) {
            int32_t row = 0x27 - EF_S32(rec + 0x3fu) / 10;
            int32_t col = EF_S32(rec + 0x3bu) / 10;
            int16_t cell = (int16_t)(row * 0x28);
            cell = (int16_t)((int32_t)cell + col);
            if (cell >= 0x640)
                cell = 0x63f;
            if ((uint16_t)guest_fn_cc1ac_x((uint32_t)EF_U8(rec + 0xeu), (uint32_t)EF_U8(map + (uint32_t)(int32_t)cell), extra) != 0) {
                EF_U8(rec + 0x15u) = 0;
                EF_U8(rec + 0xdu) = 1;
            }
        }
        rec = EF_U32(rec + 4u);
    }
}
