/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xaca14, 115 bytes. No inputs, EAX scratch, other registers preserved.
 * Keep the existing symbol name; selected entries are named Plane1..3. */
void select_default_video_aca14(void)
{
    uint32_t kind;
    ef_addr32 name;
    ef_addr32 entry;
    uint16_t first;
    uint16_t second;

    if (EF_U16(0x0041eedeu) != 0u)
        return;
    kind = EF_U32(0x0041eec0u) >> 24;
    if (kind == 1u)
        name = 0x0018dc64u;
    else if (kind == 2u)
        name = 0x0018dc6cu;
    else
        name = 0x0018dc74u;
    EF_U16(0x0041eedcu) = (uint16_t)find_name_index_ca780(name);
    entry = (uint32_t)EF_U16(0x0041eedcu) * 25u;
    first = EF_U16(0x00137293u + entry);
    second = EF_U16(0x00137295u + entry);
    EF_U16(0x0041eed4u) = first;
    EF_U16(0x0041eec8u) = second;
}
