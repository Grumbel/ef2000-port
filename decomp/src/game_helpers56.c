/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Assorted small state helpers. */

/* 0x53c10, 69 bytes. Accumulate the frame ticks and clear the idle flag once past the limit. */
void accumulate_ticks_53c10(void)
{
    EF_S32(0x001a5b5cu) += (int32_t)EF_U16(0x00127d92u);
    guest_fn_53350_w(0, 0, 0, 0, 0, 1.0f, 0);
    if (EF_U16(0x00126518u) == 0 && EF_S32(0x001a5b5cu) > 0xfa)
        EF_U8(0x001a363au) = 0;
}

/* 0x70698, 67 bytes. Clear the motion accumulators and set the default scale limits. */
void reset_motion_limits_70698(void)
{
    EF_U32(0x001f7100u) = 0;
    EF_U32(0x001f7104u) = 0;
    EF_U32(0x001f70b0u) = 0;
    EF_U32(0x001f70b4u) = 0;
    guest_fn_6e8d8_w(0, 0, 0, 0, 2.5);
    EF_U32(0x001f7110u) = 0;
    EF_U32(0x001f7114u) = 0;
    guest_fn_6eaf4_w(0, 0, 0, 0, 1.3);
}

/* 0x7feb0, 69 bytes. Start the page, or fall back to the default control mode. */
void start_page_7feb0(void)
{
    EF_U32(0x0015bac8u) = 0x001327d8u;
    if (guest_fn_d6e58_w() != 0) {
        EF_U8(0x0015bac6u) = 0;
        EF_U8(0x0015bac7u) = 0;
        guest_fn_82e40_w();
        return;
    }
    EF_U32(0x00132ae4u) = 0x0007fcdcu;
    set_control_mode_a(1);
    guest_fn_80120_w();
}

/* 0xc39ac, 68 bytes. Latch the view scroll and leave the pending-scroll mode. */
void leave_scroll_mode_c39ac(void)
{
    EF_U32(0x0015b754u) = EF_U32(0x00146e4cu);
    if (EF_U8(0x00146e4bu) == 0)
        return;
    EF_U8(0x00146e4bu) = 0;
    EF_U8(0x0041f3ecu) = 0;
    EF_U32(0x00164604u) = EF_U32(0x00164608u);
    EF_U32(0x0041f3d4u) = EF_U32(0x00164608u);
    guest_fn_fc39c(EF_U32(0x00164608u), EF_U32(0x0015b744u), EF_U32(0x0015b748u), 0);
}

/* 0xc39f0, 54 bytes. Enter the pending-scroll mode with the given latch value. */
void enter_scroll_mode_c39f0(uint32_t value)
{
    uint32_t base = EF_U32(0x00164608u) + 0x4000u;

    EF_U8(0x00146e4bu) = 1;
    EF_U32(0x00164604u) = base;
    EF_U32(0x0041f3d4u) = base;
    EF_U32(0x0015b754u) = value;
    EF_U8(0x0041f3ecu) = 1;
    guest_fn_fc39c(value, EF_U32(0x0015b744u), EF_U32(0x0015b748u), (base & 0xffff00ffu) | 0x100u);
}
