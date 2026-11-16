/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xacac0, 166 bytes. EAX name; other registers preserved, plain RET.
 * Retain the existing symbol name; this selects a named table entry. */
void select_video_mode_acac0(ef_addr32 name)
{
    ef_addr32 entry;

    if (ascii_stricmp_9d524(name, 0x0018dc7cu) == 0) {
        EF_U16(0x0041eedau) = 0u;
        EF_U16(0x0041eedeu) = 1u;
        EF_U16(0x0041eedcu) = (uint16_t)find_name_index_ca780(name);
    } else if (ascii_stricmp_9d524(name, 0x0018dc84u) == 0) {
        EF_U16(0x0041eedau) = 1u;
        EF_U16(0x0041eedeu) = 0u;
        select_default_video_aca14();
    } else {
        EF_U16(0x0041eedau) = 1u;
        EF_U16(0x0041eedeu) = 1u;
        EF_U16(0x0041eedcu) = (uint16_t)find_name_index_ca780(name);
    }
    entry = (uint32_t)EF_U16(0x0041eedcu) * 25u;
    /* Unlike aca14, this stores the first word before reading the second. */
    EF_U16(0x0041eed4u) = EF_U16(0x00137293u + entry);
    EF_U16(0x0041eec8u) = EF_U16(0x00137295u + entry);
}
