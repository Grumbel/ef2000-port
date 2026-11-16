/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xa6438, 616 bytes. EAX parent, EDX child; all non-EAX GPRs
 * preserved, plain RET. Sole caller a66bc retains ECX parent/EBX child.
 * The CS override only selects a constant jump table in the original. */
void position_widget_child_a6438(ef_addr32 parent, ef_addr32 child)
{
    uint32_t kind = EF_U8(child + 0x8cu) & 31u;
    uint32_t value, inset, offset;
    ef_addr32 owner;

    switch (kind) {
    case 1:
        EF_U16(child + 4u) = 14;
        EF_U16(child + 6u) = 14;
        /* Fall through: retain the loads before writing child Y. */
    case 2:
    case 3:
        value = EF_U16(parent + 0x45u);
        value -= EF_U16(child + 4u);
        EF_U16(child + 2u) = 3;
        EF_U16(child) = (uint16_t)(value - (kind == 1u ? 3u : kind == 2u ? 17u : 32u));
        break;
    case 4:
    case 5:
    case 6:
    case 13:
    case 14:
    case 15:
        value = EF_U16(parent + 0x71u) + EF_U16(parent + 0x75u) - 15u;
        EF_U16(child) = (uint16_t)value;
        if (kind == 4u) {
            value = EF_U16(parent + 0x73u) + EF_U16(parent + 0x77u) - 15u;
            EF_U16(child + 2u) = (uint16_t)value;
        } else if (kind == 5u) {
            value = EF_U16(parent + 0x73u);
            EF_U16(child + 2u) = (uint16_t)value;
        } else if (kind == 6u) {
            inset = 0;
            if ((EF_U8(parent + 0x3bu) & 1u) != 0u ||
                (EF_U8(parent + 0x3au) & 8u) != 0u)
                inset = 15;
            /* Original dword reads include both coordinates. */
            value = EF_U32(parent + 0x71u) >> 16;
            value += EF_U32(parent + 0x75u) >> 16;
            EF_U16(child + 2u) = (uint16_t)(value - 15u - inset);
        } else {
            offset = (kind - 13u) * 2u;
            value = EF_U16(parent + 0x7fu + offset);
            EF_U16(child + 4u) = 15;
            EF_U16(child + 2u) = (uint16_t)value;
            value = EF_U16(parent + 0x8bu + offset);
            EF_U16(child + 6u) = (uint16_t)value;
        }
        break;
    case 7:
        value = EF_U16(parent + 0x71u);
        EF_U16(child) = (uint16_t)value;
        value = EF_U16(parent + 0x73u) + EF_U16(parent + 0x77u) - 15u;
        EF_U16(child + 2u) = (uint16_t)value;
        break;
    case 8:
        value = EF_U32(parent + 0x6fu) >> 16;
        value += EF_U32(parent + 0x73u) >> 16;
        inset = (EF_U8(parent + 0x3bu) & 1u) != 0u ? 15u : 0u;
        EF_U16(child) = (uint16_t)(value - 15u - inset);
        value = EF_U16(parent + 0x73u) + EF_U16(parent + 0x77u) - 15u;
        EF_U16(child + 2u) = (uint16_t)value;
        break;
    case 9:
        owner = EF_U32(0x0041edfcu + (uint32_t)EF_U8(child + 0x50u) * 4u);
        value = EF_U16(owner + 0x45u);
        EF_U16(owner + 0xc5u) = (uint16_t)value;
        EF_U16(child) = 3;
        EF_U16(child + 2u) = 1;
        EF_U16(child + 6u) = 18;
        break;
    case 10:
    case 11:
    case 12:
        offset = (kind - 10u) * 2u;
        value = EF_U16(parent + 0x79u + offset);
        EF_U16(child) = (uint16_t)value;
        value = EF_U16(parent + 0x73u) + EF_U16(parent + 0x77u) - 15u;
        EF_U16(child + 2u) = (uint16_t)value;
        value = EF_U16(parent + 0x85u + offset);
        EF_U16(child + 6u) = 15;
        EF_U16(child + 4u) = (uint16_t)value;
        break;
    }
}
