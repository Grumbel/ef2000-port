/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"
#include "ef_structs/widget_links.h"

/* 0x34844, 160 bytes. EAX=widget. Original saves EBX/EDX/ESI.
 * The validator is called even for empty text and preserves EDX. */
void accept_network_mission_name_34844(ef_addr32 widget)
{
    uint8_t nonempty = EF_U8(EF_U32(widget + offsetof(ef_widget_link, text))) != 0;
    uint8_t valid = string_short_enough(EF_U32(widget + offsetof(ef_widget_link, text)));
    uint32_t offset;
    ef_addr32 source;

    if (nonempty != 0 && valid != 0) {
        offset = 0;
        do {
            source = EF_U32(widget + offsetof(ef_widget_link, text)) + offset;
            if ((EF_U8(0x00195af0u + (uint8_t)(EF_U8(source) + 1u)) & 2u) == 0)
                break;
            ++offset;
        } while (1);
        copy_string_9d749(0x0013579bu, source);
        offset = guest_string_length(0x0013579bu);
        /* ORIGINAL BUG: starts at NUL, rather than the last character. */
        while ((EF_U8(0x00195af0u + (uint8_t)(EF_U8(0x0013579bu + offset) + 1u)) & 2u) != 0) {
            EF_U8(0x0013579bu + offset) = 0;
            --offset;
        }
        shutdown_device_a3da0();
        clear_session_34ea4();
        write_settings_file_a2bdc();
    }
    guest_fn_33630();
}
