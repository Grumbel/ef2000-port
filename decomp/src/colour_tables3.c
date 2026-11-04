/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Colour record fillers. */

/* 0xba77c, 105 bytes. Fill a six-byte colour record chosen by a 1-based key: two leading bytes from the key, four trailing bytes of 4, except key 6 which is all 8. */
void fill_colour_record_ba77c(ef_addr32 rec, uint32_t key)
{
    uint8_t lead;
    uint32_t i;
    switch ((uint8_t)(key - 1u)) {
    case 2: lead = 0x11u; break;
    case 3: lead = 0x03u; break;
    case 4: lead = 0x0au; break;
    case 5:
        for (i = 0; i < 6u; i++)
            EF_U8(rec + i) = 0x08u;
        return;
    case 6: lead = 0x14u; break;
    default: lead = 0x13u; break;
    }
    EF_U8(rec) = lead;
    EF_U8(rec + 1u) = lead;
    for (i = 2; i < 6u; i++)
        EF_U8(rec + i) = 0x04u;
}
