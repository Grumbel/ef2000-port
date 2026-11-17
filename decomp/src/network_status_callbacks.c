/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

static uint32_t guest(uint32_t entry, uint32_t eax, uint32_t edx, uint32_t ebx)
{
    return net_ui_guest_call_086(entry, eax, edx, ebx);
}

/* 345d4, 19 bytes: false shares 345d2 RET in the previous function;
 * true tail-jumps to a4d90 with the modem-ending dialog name. */
void net_new_end_string_345d4(void)
{
    if (EF_U8(0x13578cu) != 0u)
        guest(0xa4d90u, 0x182e10u, 0u, 0u);
}

/* 347ac, 13 bytes: AH is discarded, only zero-extended widget ID remains. */
void net_open_loops_347ac(ef_addr32 widget)
{
    guest(0xa4b80u, EF_U8(widget + 0x10u), 0u, 0u);
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

/* 35e68 and 35e90 are distinct named callbacks with identical 37-byte
 * bodies. Both connect the status widget, clear its editable state and
 * forward two zero-valued stack arguments to the RX status routine. */
static void open_rx_status(ef_addr32 widget)
{
    guest(0xa75b4u, widget, 0x182fa8u, 0u);
    guest(0xa7434u, widget, 0u, 0u);
    guest(0x35de0u, 0u, 0u, 0u);
}

void net_open_rx_status_35e68(ef_addr32 widget)
{
    open_rx_status(widget);
}

void net_set_nonentry_flag_35e90(ef_addr32 widget)
{
    open_rx_status(widget);
}
