/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xac16c, 211 bytes. No inputs; EAX scratch, other registers preserved.
 * Call sites ac158/ac791. The two device-flag reads straddle motion polling. */
void clear_ui_state_ac16c(void)
{
    uint8_t device;

    clear_pool_ab8c4();
    clear_record_flags_ca5e4();
    EF_U16(0x0041eeb6u) = 0u;
    EF_U16(0x0041eed8u) = 0u;
    EF_U16(0x0041eed0u) = 0u;
    EF_U16(0x0041eeceu) = 0u;
    EF_U32(0x0041ecd0u) = 0u;
    clear_widget_table_a4e98();
    fill_bytes_b3fb0(0x0041e960u, 0u, 0x150u);
    device = EF_U8(0x0041f2e1u);
    EF_U16(0x0041eeccu) = 0u;
    if (device != 0u)
        mouse_read_motion_ce02c(0x0041eeb0u, 0x0041eeb0u);
    device = EF_U8(0x0041f2e1u);
    EF_U8(0x0013725du) = 0u;
    EF_U8(0x0013725cu) = 0u;
    EF_U8(0x0041f2e5u) = 0u;
    select_video_mode_acac0(device != 0u ? 0x0018db9cu : 0x0018dba4u);
    EF_U8(0x0041f2e4u) = 1u;
    EF_U32(0x0041ec18u) = 0u;
    EF_U8(0x0041f2deu) = 0u;
    EF_U8(0x0041f833u) = 0u;
    EF_U8(0x0041f2e6u) = 0u;
    fill_bytes_b3fb0(0x0041f284u, 0u, 0x59u);
}
