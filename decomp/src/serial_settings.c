/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x337ec, 156 bytes. EAX=root; saved EBX/ECX/EDX, other callees
 * preserve ESI/EDI/EBP. Read the channel only after showing the socket. */
void show_serial_settings_337ec(ef_addr32 root)
{
    char text[8];
    ef_addr32 buffer = (ef_addr32)(uintptr_t)text;
    ef_addr32 socket = find_widget_checked_a78bc(root, 0x00182bc4u);
    ef_addr32 channel = find_widget_checked_a78bc(root, 0x00182bd0u);
    uint32_t value = EF_U16(0x001357b8u);

    if (value == 0u || value == 0xffffu)
        (void)guest_sprintf_a4342(buffer, 0x00182bdcu);
    else
        (void)guest_sprintf_a4342(buffer, 0x00182be4u, value);
    show_widget_a7588(socket, buffer);
    (void)guest_sprintf_a4342(buffer, 0x00182becu, (uint32_t)EF_U8(0x001357b6u));
    show_widget_a7588(channel, buffer);
}
