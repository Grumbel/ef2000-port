/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"
#include "ef_structs/widget_links.h"

/* 0x344bc, 206 bytes. EAX=widget; validate only the first three characters.
 * Parser success is tested through AL before the signed full-width range. */
void change_network_channel_344bc(ef_addr32 widget)
{
    ef_addr32 text = EF_U32(widget + offsetof(ef_widget_link, text));
    uint32_t offset;
    int32_t parsed;
    uint8_t valid = 1;
    uint8_t character;
    uint8_t classification;

    for (offset = 0; offset < 3u; ++offset) {
        character = EF_U8(text + offset);
        if (character == 0)
            break;
        classification = EF_U8(0x00195af0u + (uint8_t)(character + 1u));
        if ((classification & 0x22u) == 0)
            valid = 0;
    }
    text = EF_U32(widget + offsetof(ef_widget_link, text));
    if (EF_U8(text) == 0)
        valid = 0;
    if (valid != 0 &&
        (uint8_t)guest_fn_df26f_x(EF_U32(widget + offsetof(ef_widget_link, text)),
                                0x00182bbcu, (ef_addr32)(uintptr_t)&parsed) == 1u &&
        parsed > 0 && parsed < 256 && parsed != EF_U8(0x001357b6u)) {
        clear_session_34ea4();
        shutdown_device_a3da0();
        EF_U8(0x001357b6u) = (uint8_t)parsed;
        write_settings_file_a2bdc();
    }
    guest_fn_33630();
}
