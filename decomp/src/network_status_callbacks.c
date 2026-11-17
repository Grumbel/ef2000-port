/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 349ec, 23 bytes: retain resolved widget and refresh network loop UI. */
void net_main_loop_349ec(ef_addr32 widget)
{
    EF_U32(0x19f320u) = find_widget_checked_a78bc(widget, 0x182e60u);
    refresh_list_widget_348e4();
}

/* 35128, 27 bytes: staged network-initialization transition. */
void net_open_initialise_35128(void)
{
    EF_U8(0x19f59bu) = 0xffu;
    close_device_a3e04();
    clear_session_34ea4();
    set_pending_name_ac7a8(0x182f24u);
}
