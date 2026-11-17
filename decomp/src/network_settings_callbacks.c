/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* Individual calls keep their original entry addresses visible. Values in
 * EAX/EDX/EBX are only those established by local instructions; the host
 * hook is a semantic probe, not a substitute for the original registers. */
static uint32_t guest(uint32_t entry, uint32_t eax, uint32_t edx, uint32_t ebx)
{
    return net_ui_guest_call_086(entry, eax, edx, ebx);
}

/* Shared network-refresh tail at 0x33630. Several callbacks CALL it, while
 * others JMP into its prologue. The latter are not additional stack calls
 * in the original: this marker preserves the unresolved shared dependency. */
static void refresh_network(void)
{
    guest(0x33630u, 0u, 0u, 0u);
}

static void notify_network_change(void)
{
    guest(0xa2bdcu, 0u, 0u, 0u);
}

/* 335c0, 89 bytes. sscanf(widget->value, "%d", &local) must yield one
 * assignment of an integer in [1,4]. The original DEC AL deliberately
 * maps rejection (local=0) to the byte 0xff. */
void net_new_baud_335c0(ef_addr32 widget)
{
    int32_t parsed = 0;
    guest(0x34ea4u, widget, widget, 0u);
    guest(0xa3da0u, 0u, 0u, 0u);
    if (net_ui_parse_decimal_086(EF_U32(widget + 0x27u), 0x182bbcu,
                                  &parsed) != 1 ||
        parsed < 1 || parsed > 4)
        parsed = 0;
    EF_U8(0x15f07fu) = (uint8_t)(parsed - 1);
    notify_network_change();
    refresh_network();
}
