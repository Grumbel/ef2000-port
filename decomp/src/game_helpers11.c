/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Command dispatch, pixel-trail and screen-setup helpers, 0x3cc94..0x77084. */

/* 0x3cc94, 85 bytes. Plot a pixel for each step of the string walker. */
void plot_trail_3cc94(ef_addr32 start)
{
    ef_addr32 cursor = start;
    uint32_t first;
    uint32_t second;

    guest_fn_3cc28(start);
    if (EF_U8(start) == 0)
        return;
    do {
        cursor = guest_fn_3c994(cursor);
        first = guest_fn_9d4cd(EF_U32(0x001a2e8cu));
        second = guest_fn_9d4cd(EF_U32(0x001a2e90u));
        store_plane_pixel_3c94c((int16_t)first, (int16_t)second, (int16_t)((int32_t)EF_U32(0x001a2ea0u) >> 16));
    } while (EF_U8(cursor) != 0);
}

/* 0x413b0, 68 bytes. */
void init_screen_413b0(void)
{
    if (EF_U32(0x00126570u) == 0) {
        uint32_t handle = guest_fn_b3db0(0x4b000u);

        EF_U32(0x00126570u) = guest_fn_b3f58((int32_t)(int16_t)handle);
    }
    guest_fn_c36b4(0x001843fcu, 0, 0, 0x280, 0x1e0, EF_U32(0x00126570u));
}

/* 0x507dc, 23 bytes. */
void refresh_widget_507dc(void)
{
    ef_addr32 widget = guest_fn_a46cc(0x00184eccu);

    guest_fn_a4cb4(EF_U8(widget + 0x10u));
}

/* 0x50e84, 69 bytes. Wait for a key other than 0x3b with a long enough name, then handle it. */
uint32_t wait_command_50e84(void)
{
    for (;;) {
        guest_fn_50ca4();
        if (EF_U8(0x001a5986u) == 0x3bu)
            continue;
        if (guest_fn_b1b8b(0x001a5986u) < 3u)
            continue;
        break;
    }
    guest_fn_50d04();
    EF_U8(0x00127d30u) = 0;
    if ((text_ends_with_colon_50dc8() & 0xffffu) != 0)
        return 0;
    guest_fn_50d9c();
    return 1;
}

/* 0x540d0, 64 bytes. */
void reset_selection_540d0(void)
{
    uint16_t cached = EF_U16(0x001a5c1eu);

    EF_U16(0x001a5c18u) = 0;
    EF_U8(0x001a5c20u) = 0;
    EF_U16(0x001a5c1cu) = 0;
    EF_U16(0x001a5c1au) = 0;
    if (cached == 0)
        EF_U16(0x001a5c1eu) = (uint16_t)guest_fn_9ddfc(0x00185b9cu, 0);
}

/* 0x54e80, 26 bytes. */
void request_name_54e80(uint32_t value)
{
    guest_fn_a7a1c(EF_U32(0x001a5d18u), 0x100, value + 1u);
}

/* 0x77034, 81 bytes. Send command 0x44/0x45/0x43 for kind 0/1/2. */
void send_slot_command_77034(int16_t unit_id, int16_t value, uint8_t kind)
{
    uint32_t command;

    if (kind == 0)
        command = 0x44;
    else if (kind == 1)
        command = 0x45;
    else if (kind == 2)
        command = 0x43;
    else
        return;
    guest_fn_46254((uint32_t)(int32_t)unit_id, (int32_t)value, command, kind, 0, 0);
}
