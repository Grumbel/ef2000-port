/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Orientation test, byte-swapped log write, nearest-entry search, MIDI reset and code lookup helpers. */

/* 0xd22d0, 80 bytes. Return 1 when the three records' 16.16 (x,y) points form a clockwise triangle (negative cross product). */
uint8_t triangle_is_clockwise_d22d0(ef_addr32 triangle)
{
    ef_addr32 p0 = EF_U32(triangle);
    ef_addr32 p1 = EF_U32(triangle + 4u);
    ef_addr32 p2 = EF_U32(triangle + 8u);
    int32_t x0 = EF_S32(p0 + 0x18u) >> 16;
    int32_t y0 = EF_S32(p0 + 0x1cu) >> 16;
    int32_t x1 = EF_S32(p1 + 0x18u) >> 16;
    int32_t y1 = EF_S32(p1 + 0x1cu) >> 16;
    int32_t x2 = EF_S32(p2 + 0x18u) >> 16;
    int32_t y2 = EF_S32(p2 + 0x1cu) >> 16;

    return (uint8_t)(((x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0)) < 0);
}

/* 0xd1860, 52 bytes. Write a 16-bit word to the log in big-endian byte order. */
void write_log_word_swapped_d1860(uint16_t value)
{
    uint8_t buf[2];

    buf[0] = (uint8_t)(value >> 8);
    buf[1] = (uint8_t)value;
    write_log_d16d0((uint32_t)(uintptr_t)buf, 2);
}

/* 0xd642c, 73 bytes. Index of the table entry closest to the given value (first minimum wins, limit 0x100). */
uint8_t nearest_entry_index_d642c(int32_t value, ef_addr32 table, int32_t count)
{
    uint32_t best = 0x100;
    int32_t best_index = 0;
    int32_t i;

    for (i = 0; i < count; i++) {
        uint32_t d = abs_a28f2(EF_S32(table + (uint32_t)i * 4u) - value);
        if ((int32_t)d < (int32_t)best) {
            best_index = i;
            best = abs_a28f2(EF_S32(table + (uint32_t)i * 4u) - value);
        }
    }
    return (uint8_t)best_index;
}

/* 0xd6024, 53 bytes. Send two controller messages on each of the 16 MIDI channels (reset sequence). */
void midi_reset_channels_d6024(void)
{
    int32_t ch;

    for (ch = 0; ch < 16; ch++) {
        guest_fn_d605c_x((uint32_t)ch | 0xb0u, 0x40, 0);
        guest_fn_d605c_x((uint32_t)ch | 0xb0u, 0x7b, 0x7f);
    }
}

/* 0xdcc98, 61 bytes. Look up a 6-bit code in the table at 0x15e1be and dispatch it to cf7c4; returns the entry's count + 1. */
uint8_t dispatch_code_table_a_dcc98(uint8_t code, int16_t dx_val, int16_t bx_val, int16_t cx_val)
{
    int8_t idx = (int8_t)EF_U8(0x0015e1beu + (uint32_t)(code & 0x3fu));

    if (idx == -1)
        return 0;
    guest_fn_cf7c4_x(EF_U32(0x0015e350u + (uint32_t)(int32_t)idx * 4u), (uint32_t)(int32_t)bx_val, (uint32_t)(int32_t)dx_val, (uint32_t)(int32_t)cx_val);
    return (uint8_t)(EF_U8(0x0015e1feu + (uint32_t)(int32_t)idx) + 1u);
}

/* 0xdd164, 74 bytes. Same lookup as dcc98 with code 0xa5 remapped to 0x4e and the second table set. */
uint8_t dispatch_code_table_b_dd164(uint8_t code, int16_t dx_val, int16_t bx_val, int16_t cx_val)
{
    int8_t idx;

    if (code == 0xa5)
        code = 0x4e;
    idx = (int8_t)EF_U8(0x0015e1beu + (uint32_t)(code & 0x3fu));
    if (idx == -1)
        return 0;
    guest_fn_cf7c4_x(EF_U32(0x0015e5b4u + (uint32_t)(int32_t)idx * 4u), (uint32_t)(int32_t)bx_val, (uint32_t)(int32_t)dx_val, (uint32_t)(int32_t)cx_val);
    return (uint8_t)(EF_U8(0x0015e264u + (uint32_t)(int32_t)idx) + 1u);
}
