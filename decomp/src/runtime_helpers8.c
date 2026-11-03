/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Window/widget library helpers, 0xa33a4..0xa5b1c. */

/* 0xa3acc, 122 bytes. Install the table of ten handler addresses. */
void install_handlers_a3acc(void)
{
    EF_U32(0x00136ba8u) = 0xa3a34u;
    EF_U32(0x00136bacu) = 0xa3a44u;
    EF_U32(0x00136ba0u) = 0xa3a54u;
    EF_U32(0x00136bb0u) = 0xa3ac8u;
    EF_U32(0x00136ba4u) = 0xa3a68u;
    EF_U32(0x00136bb4u) = 0xa3a78u;
    EF_U32(0x00136bb8u) = 0xa3a88u;
    EF_U32(0x00136bbcu) = 0xa3aa8u;
    EF_U32(0x00136bc0u) = 0xa3ab8u;
    EF_U32(0x00136bc4u) = 0xa3a98u;
}

/* 0xa33a4, 74 bytes. Dispatch on the low nibble of the entry's kind byte. */
void dispatch_entry_a33a4(uint32_t entry)
{
    uint32_t index = entry & 0xffffu;
    uint8_t kind = EF_U8(EF_U32(0x00135878u + (index << 5)) + 0xdu) & 0xfu;

    if (kind == 1)
        guest_fn_a32e8(index);
    else if (kind == 3)
        guest_fn_a334c(index);
    else if (kind == 7)
        guest_fn_a3824(entry & 0xffu);
}

/* 0xa33f0, 52 bytes. Dispatch every entry of the list at 0x136b84. */
void dispatch_all_a33f0(void)
{
    uint16_t i = 0;

    while ((uint32_t)i < EF_U8(EF_U32(0x00136b88u))) {
        dispatch_entry_a33a4(EF_U8(EF_U32(0x00136b84u) + i));
        i++;
    }
}

/* 0xa4678, 82 bytes. Find the registered entry whose name equals key, or 0. */
ef_addr32 find_registered_a4678(ef_addr32 key)
{
    uint16_t i;

    for (i = 1; i <= EF_U16(0x0041eed8u); i++) {
        ef_addr32 entry = EF_U32(0x0041edfcu + (uint32_t)i * 4u);

        if (entry != 0 && guest_fn_9d524(entry, key) == 0)
            return entry;
    }
    return 0;
}

/* 0xa4dd0, 65 bytes. Run a widget modally until its flag 4 is cleared. */
ef_addr32 run_widget_a4dd0(ef_addr32 widget)
{
    EF_U8(0x0041f2deu) = 1;
    guest_fn_a4cb4(EF_U8(widget + 0x10u));
    guest_fn_ac9a0(widget);
    if (EF_U8(widget + 0x3bu) & 4u) {
        do {
            guest_fn_ac5f8(0);
        } while (EF_U8(widget + 0x3bu) & 4u);
    }
    guest_fn_ac9e8();
    EF_U8(0x0041f2deu) = 0;
    return widget;
}

/* 0xa5484, 106 bytes. Move a widget, clamped to the screen unless flag 0x10 is set. */
void move_widget_a5484(ef_addr32 widget, int16_t x, int16_t y)
{
    if ((EF_U8(widget + 0x3au) & 0x10u) == 0) {
        int32_t limit;

        if (x < 0)
            x = 0;
        if (y < 0)
            y = 0;
        limit = (int32_t)EF_U32(0x0015b744u) - (int32_t)(int16_t)EF_U16(widget + 0x45u);
        if ((int32_t)x >= limit)
            x = (int16_t)(EF_U16(0x0015b744u) - EF_U16(widget + 0x45u));
        limit = (int32_t)EF_U32(0x0015b748u) - (int32_t)(int16_t)EF_U16(widget + 0x47u);
        if ((int32_t)y >= limit)
            y = (int16_t)(EF_U16(0x0015b748u) - EF_U16(widget + 0x47u));
    }
    EF_U16(widget + 0x41u) = (uint16_t)x;
    EF_U16(widget + 0x43u) = (uint16_t)y;
    guest_fn_a51dc(widget);
}

/* 0xa570c, 87 bytes. Toggle the child controls of kinds 5, 6, 0xd, 0xe and 0xf. */
void toggle_children_a570c(ef_addr32 widget, uint8_t state)
{
    ef_addr32 child = EF_U32(widget + 0x91u);

    while (child != 0) {
        uint8_t kind = EF_U8(child + 0x8cu) & 0x1fu;

        if (kind == 5 || kind == 6 || kind == 0xd || kind == 0xe || kind == 0xf)
            guest_fn_a739c(child, (uint8_t)(state ^ 1u));
        child = EF_U32(child + 0x21u);
    }
    guest_fn_a56f8(widget);
}
