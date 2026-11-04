/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Angle classes, page-cell setup and group walks, 0x92070..0xab0dc. */

/* 0x92070, 169 bytes. Classify the signed angle word into a -2..1 step (kept when out of range). */
void classify_angle_92070(void)
{
    int16_t step = (int16_t)EF_U16(0x0012dba6u);
    int32_t value = (int16_t)EF_U16(0x0012db9au);

    if (value >= 0) {
        if (value < 0x2000)
            step = -2;
        else if (value < 0x3000)
            step = -1;
        else if (value < 0x4000)
            step = 0;
        else if (value < 0x5000)
            step = 1;
    } else {
        if (value > -0x2000)
            step = -2;
        else if (value > -0x3000)
            step = -1;
        else if (value > -0x4000)
            step = 0;
        else if (value > -0x5000)
            step = 1;
    }
    EF_U16(0x0012dba6u) = (uint16_t)step;
}

/* 0x9dfc0, 165 bytes. Select a page and its layout; refresh each cell when the layout is active. */
void select_page_layout_9dfc0(int16_t page, uint16_t b, uint16_t c, uint16_t d)
{
    ef_addr32 table;
    ef_addr32 layout;
    int32_t column;
    int32_t row;

    EF_U16(0x0041722au) = (uint16_t)page;
    table = guest_fn_b3f58((int32_t)page);
    EF_U32(0x00417174u) = table;
    layout = EF_U32(0x0040d200u + (uint32_t)EF_U16(table) * 4u);
    EF_U16(0x00417194u) = b;
    EF_U16(0x00417196u) = c;
    EF_U32(0x0041717cu) = layout;
    EF_U16(0x00417198u) = d;
    if (EF_U8(0x00137a00u) == 0)
        return;
    if ((EF_U8(layout + 5u) & 1u) != 0) {
        for (column = 0; column < (int32_t)EF_U8(EF_U32(0x0041717cu) + 1u); column++) {
            for (row = 0; row < 8; row++)
                guest_fn_a0c04((int32_t)(int16_t)column, (int32_t)(int16_t)row);
        }
    } else {
        for (column = 0; column < (int32_t)EF_U8(EF_U32(0x0041717cu) + 1u); column++)
            guest_fn_a0c04((int32_t)(int16_t)column, 0);
    }
}

/* 0xa1424, 161 bytes. Stamp a page cell with a scaled duration and a command code. */
void stamp_page_cell_a1424(int16_t column, int16_t value, uint32_t code, uint8_t flag)
{
    ef_addr32 base = EF_U32(0x00417174u) + 0xbu;
    ef_addr32 cell = base + (uint32_t)EF_U8(EF_U32(0x0041717cu) + 1u) * 29u + (uint32_t)((int32_t)column * 2);
    int16_t scale = (int16_t)EF_U16(EF_U32(0x00132accu));
    int16_t result;

    if (scale != 0) {
        result = (int16_t)(((int32_t)value * (int32_t)scale) / 100);
        if (value < 1)
            result = 1;
    } else {
        result = 1;
    }
    EF_U8(cell + 3u) = 0;
    EF_U8(cell + 4u) = 0;
    EF_U8(cell + 5u) = 0;
    EF_U8(cell + 7u) = 0;
    EF_U8(cell + 6u) = flag;
    EF_U8(cell + 2u) = (uint8_t)result;
    EF_U16(cell) = (uint16_t)((code & 0x3ffu) | 0x2c00u);
}

/* 0xa905c, 122 bytes. Latch the target of an incoming order message. */
void latch_order_target_a905c(ef_addr32 message)
{
    uint8_t kind;
    uint8_t sub;

    if (EF_U8(0x001371ffu) != 0) {
        EF_U8(0x00137200u) = 1;
        return;
    }
    if (EF_U8(0x001371ecu) == 0)
        return;
    kind = EF_U8(message);
    if (kind == 1 || kind != 0)
        return;
    sub = EF_U8(message + 1u);
    if (sub == 0) {
        EF_U16(0x00137202u) = EF_U16(message + 2u);
        EF_U8(0x001371f0u) = 1;
        return;
    }
    if (sub == 1) {
        EF_U8(0x001371f0u) = kind;
        EF_U16(0x00137202u) = 0xffffu;
    }
}

/* 0xab0dc, 149 bytes. Visit every kind-0xe node of every group and notify its three links. */
void notify_group_links_ab0dc(uint32_t target)
{
    ef_addr32 group = EF_U32(0x00419200u);
    int16_t group_index = 0;

    while ((int32_t)group_index < (int32_t)EF_U16(0x004194c2u)) {
        ef_addr32 node = EF_U32(group + 8u);
        int16_t i = 0;

        while ((int32_t)i < (int32_t)EF_U16(group + 0xcu)) {
            if (EF_U8(node) == 0x0e) {
                ef_addr32 info = EF_U32(node + 9u);

                guest_fn_ab0b0(target, EF_U32(info + 0x1eu));
                if (EF_U32(info + 0x22u) != 0)
                    guest_fn_ab0b0(target, EF_U32(info + 0x22u));
                if (EF_U32(info + 0x26u) != 0)
                    guest_fn_ab0b0(target, EF_U32(info + 0x26u));
            }
            i++;
            node = EF_U32(node + 1u);
        }
        group_index++;
        group = EF_U32(group);
    }
    guest_fn_ab060(target);
}
