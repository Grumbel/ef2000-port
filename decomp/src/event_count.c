/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x9aa78, 182 bytes. DX kind only; six registers preserved, EAX scratch.
 * Sole caller 9a683 also sets EAX, but the original never reads it. */
void count_event_9aa78(int16_t kind)
{
    uint16_t slot = EF_U16(0x0040d028u);
    ef_addr32 counter = 0u;
    uint32_t offset = (uint32_t)(int32_t)(int16_t)slot * 2u;

    switch (kind) {
    case 0:    counter = 0x0040cfdeu; break;
    case 0x13: counter = 0x0040cfe4u; break;
    case 0x1d: counter = 0x0040cfeau; break;
    case 0x26: counter = 0x0040cff0u; break;
    case 0x31: counter = 0x0040cff6u; break;
    case 0x49: counter = 0x0040cffcu; break;
    case 0x4f: counter = 0x0040d002u; break;
    }
    if (counter != 0u) {
        counter += offset;
        EF_U16(counter) = (uint16_t)(EF_U16(counter) + 1u);
    }
    /* Preserve original writeback, including when counter aliases slot. */
    EF_U16(0x0040d028u) = slot;
}
