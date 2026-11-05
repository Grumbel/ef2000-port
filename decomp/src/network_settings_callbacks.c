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

/* 3361c, 20 bytes before the shared 33630 tail: no local return. */
void net_new_parity_3361c(ef_addr32 widget)
{
    guest(0x34ea4u, widget, 0u, 0u);
    guest(0xa3da0u, 0u, 0u, 0u);
    guest(0xdf2e4u, 0u, 0u, 0u);
    notify_network_change();
    refresh_network();
}

/* 336c0, 45 bytes: toggle the FIFO-length option unless both guard bytes
 * indicate that doing so is prohibited; then JMP to the shared refresh. */
void net_new_fifo_len_336c0(ef_addr32 widget)
{
    guest(0x34ea4u, widget, 0u, 0u);
    guest(0xa3da0u, 0u, 0u, 0u);
    if (EF_U8(0x13578cu) == 0u || EF_U8(0x15f098u) == 0u)
        EF_U8(0x15f099u) ^= 1u;
    notify_network_change();
    refresh_network();
}

/* 336f0, 47 bytes: an early jump to 33618 is a RETURN, not a refresh. */
void net_open_ipx_settings_336f0(ef_addr32 widget)
{
    if (EF_U8(0x15f099u) == 0u)
        return;
    guest(0x34ea4u, widget, 0u, 0u);
    guest(0xa3da0u, 0u, 0u, 0u);
    EF_U8(0x15f09au) = (uint8_t)((EF_U8(0x15f09au) + 1u) & 3u);
    notify_network_change();
    refresh_network();
}

/* 33720 and 33738, distinct, 21-byte widget-text store callbacks. */
static void store_widget_text(ef_addr32 widget, ef_addr32 dest)
{
    guest(0x9d749u, dest, EF_U32(widget + 0x27u), 0u);
    notify_network_change();
}

void net_new_number_33720(ef_addr32 widget)
{
    store_widget_text(widget, 0x15f57du);
}

void net_new_pt_33738(ef_addr32 widget)
{
    store_widget_text(widget, 0x15f53du);
}

/* 33760, 74 bytes: toggle the current initialization-string slot, format
 * the selected string, write it into the target widget and redraw. The
 * formatter callback stands in for the original local sprintf buffer. */
void net_new_init_string_33760(ef_addr32 widget)
{
    uint8_t choice = (uint8_t)(EF_U8(0x15f688u) ^ 1u);
    ef_addr32 label;
    EF_U8(0x15f688u) = choice;
    label = net_ui_format_option_086(0x182bc0u,
              EF_U32(0x125958u + 4u * choice));
    guest(0xa7588u, widget, label, 0u);
    notify_network_change();
}

void net_open_modem_status_337ac(ef_addr32 widget)
{
    store_widget_text(widget, 0x15f4fdu);
}

/* 337c4, 37 bytes: zero-extended byte sent to e1540, whose AL is saved. */
void net_new_port_337c4(ef_addr32 widget)
{
    uint32_t port;
    guest(0xa3da0u, widget, 0u, 0u);
    port = guest(0xe1540u, EF_U8(0x13579au), 0u, 0u);
    EF_U8(0x13579au) = (uint8_t)port;
    guest(0x34ea4u, port, 0u, 0u);
    notify_network_change();
    refresh_network();
}
