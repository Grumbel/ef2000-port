/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"
#include "ef_structs/widget_links.h"

/* 0x335c0, 89 bytes. EAX=widget; both setup calls preserve its saved EDX.
 * Read the text after shutdown. Rejected input deliberately stores 0xff. */
void net_new_baud_335c0(ef_addr32 widget)
{
    int32_t parsed;
    clear_session_34ea4();
    shutdown_device_a3da0();
    if (guest_fn_df26f_x(EF_U32(widget + 0x27u), 0x00182bbcu,
                        (ef_addr32)(uintptr_t)&parsed) != 1 ||
        parsed < 1 || parsed > 4)
        parsed = 0;
    EF_U8(0x0015f07fu) = (uint8_t)((uint32_t)parsed - 1u);
    write_settings_file_a2bdc();
    refresh_network_controls_33630();
}
