/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xbc1d3, 180 bytes. EAX mode -> EAX flags, six registers preserved,
 * plain RET. Sole caller bc291 retains EDX mode and ECX stream. */
uint32_t parse_open_mode_bc1d3(ef_addr32 mode)
{
    volatile uint8_t *text = (volatile uint8_t *)ef_guest_memory_ptr(mode);
    uint8_t first = (uint8_t)to_lower_ascii(text[0]);
    uint8_t suffix;
    uint32_t flags = 0u;

    if (first != 'r' && first != 'w' && first != 'a') {
        store_through_ebd30(9u);
        return 0u;
    }
    suffix = text[1];
    if (suffix == '+') {
        flags = 3u;
        suffix = text[2];
        if (suffix == 'b')
            flags = 0x43u;
        else if (suffix != 't' && EF_U32(0x0016062du) == 0x200u)
            flags = 0x43u;
    } else if (text[1] == 'b') {
        flags = 0x40u;
        if (text[2] == '+')
            flags = 0x43u;
    } else if (text[1] == 't') {
        if (text[2] == '+')
            flags = 3u;
    } else if (EF_U32(0x0016062du) == 0x200u) {
        flags = 0x40u;
    }
    if (first == 'w')
        flags |= 2u;
    else if (first == 'a')
        flags |= 0x82u;
    else
        flags |= 1u;
    return flags;
}
