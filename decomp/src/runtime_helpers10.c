/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Widget library helpers, 0xa63c0..0xa9d0c. */

/* 0xa63c0, 60 bytes. Update a widget's position words from the colour nibble. */
void update_widget_metrics_a63c0(ef_addr32 widget)
{
    uint8_t nibble = (uint8_t)(EF_U8(widget + 0x8bu) >> 4);

    EF_U16(widget + 0x2fu) = (uint16_t)(guest_fn_e284c(EF_U32(widget + 0x27u), nibble) + 8u);
    EF_U16(widget + 0x31u) = (uint16_t)read_table_e192c((uint8_t)(EF_U8(widget + 0x8bu) >> 4));
}

/* 0xa73cc, 57 bytes. Set bit 6 of the located widget's flags and refresh it. */
ef_addr32 set_widget_bit6_a73cc(ef_addr32 key, uint32_t unused_edx, uint8_t flag)
{
    ef_addr32 widget = guest_fn_a78bc(key);

    (void)unused_edx;
    if (widget != 0) {
        uint8_t bit = flag & 1u;

        EF_U8(widget + 0x1du) = (uint8_t)((EF_U8(widget + 0x1du) & 0xbfu) | (bit << 6));
        if (EF_U8(0x0041f2e5u) == 0)
            guest_fn_a5ef8(EF_U8(widget + 0x50u));
    }
    return widget;
}

/* 0xa74bc, 83 bytes. Move a widget (negative coordinates clamp to 0) and relayout it. */
void move_widget_a74bc(ef_addr32 widget, int16_t x, int16_t y)
{
    if (widget == 0)
        return;
    EF_U16(widget + 4u) = (uint16_t)(x >= 0 ? (int32_t)x : 0);
    EF_U16(widget + 6u) = (uint16_t)(y >= 0 ? (int32_t)y : 0);
    guest_fn_a69d0(EF_U32(widget + 0x51u), widget);
    guest_fn_a66cc(widget, 0);
    guest_fn_a6808(widget);
    if (EF_U8(0x0041f2e5u) == 0)
        guest_fn_a6d00(widget);
}

/* 0xa7588, 42 bytes. Show a widget. */
void show_widget_a7588(ef_addr32 widget)
{
    if (widget == 0)
        return;
    guest_fn_a7510(widget);
    guest_fn_a66cc(widget, 1);
    if (EF_U8(0x0041f2e5u) == 0)
        guest_fn_a6d00(widget);
}

/* 0xa788c, 45 bytes. Find the child control of a widget whose name equals key. */
ef_addr32 find_child_by_name_a788c(ef_addr32 widget, ef_addr32 key)
{
    ef_addr32 child = EF_U32(widget + 0x91u);

    while (child != 0) {
        if (guest_fn_9d524(child + 0x3fu, key) == 0)
            return child;
        child = EF_U32(child + 0x21u);
    }
    return 0;
}

/* 0xa8060, 80 bytes. Write the fill byte to every live entry of the table, clear dead ones. */
void fill_entries_a8060(void)
{
    int16_t count = (int16_t)EF_U16(0x004194c4u);
    uint8_t fill = EF_U8(0x0018d544u);
    int16_t i;

    for (i = 0; i < count; i++) {
        uint32_t slot = 0x00418708u + (uint32_t)((int32_t)i * 4);

        if (EF_U8(0x0041948eu + (uint32_t)(int32_t)i) == 0)
            EF_U32(slot) = 0;
        else
            EF_U8(EF_U32(slot)) = fill;
    }
    EF_U16(0x004194c4u) = (uint16_t)count;
}

/* 0xa9d0c, 60 bytes. Select the list node at position count (1-based) and return its payload. */
uint32_t select_list_node_a9d0c(void)
{
    uint16_t count = EF_U16(0x004194c2u);
    ef_addr32 node = EF_U32(0x00419200u);
    int16_t i = 1;

    while ((int32_t)i < (int32_t)count) {
        i++;
        node = EF_U32(node);
    }
    EF_U32(0x004191f8u) = node;
    EF_U16(0x004194c2u) = count;
    return EF_U32(node + 8u);
}
