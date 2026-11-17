/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

static uint32_t guest(uint32_t entry, uint32_t eax, uint32_t edx, uint32_t ebx)
{
    return net_ui_guest_call_086(entry, eax, edx, ebx);
}

/* 349ec, 23 bytes: retain resolved widget and refresh network loop UI. */
void net_main_loop_349ec(ef_addr32 widget)
{
    EF_U32(0x19f320u) = guest(0xa78bcu, widget, 0x182e60u, 0u);
    guest(0x348e4u, 0u, 0u, 0u);
}

/* 35128, 27 bytes: staged network-initialization transition. */
void net_open_initialise_35128(void)
{
    EF_U8(0x19f59bu) = 0xffu;
    guest(0xa3e04u, 0u, 0u, 0u);
    guest(0x34ea4u, 0u, 0u, 0u);
    guest(0xac7a8u, 0x182f24u, 0u, 0u);
}
