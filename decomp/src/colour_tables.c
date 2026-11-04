/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Three identical colour record fillers. */

/* Fill a colour record: bytes 0 and 1 = 5, bytes 4..7 = 7 and bytes 2 and 3 = the shade for the key (1: 0x17, 2: 0xd, 3: 0x1a, 4: 6, 5: 0xf, 6: 0xc, 7: 0x18, anything else 0x17). */
static void fill_colour_record_common(ef_addr32 rec, uint8_t key)
{
    uint8_t shade;

    EF_U8(rec) = 5;
    EF_U8(rec + 1u) = 5;
    EF_U8(rec + 4u) = 7;
    EF_U8(rec + 5u) = 7;
    EF_U8(rec + 6u) = 7;
    EF_U8(rec + 7u) = 7;
    switch ((uint8_t)(key - 1u)) {
    case 1: shade = 0x0d; break;
    case 2: shade = 0x1a; break;
    case 3: shade = 0x06; break;
    case 4: shade = 0x0f; break;
    case 5: shade = 0x0c; break;
    case 6: shade = 0x18; break;
    default: shade = 0x17; break;
    }
    EF_U8(rec + 2u) = shade;
    EF_U8(rec + 3u) = shade;
}

/* 0xba9c4, 107 bytes. Fill a colour record for a key. */
void fill_colour_record_ba9c4(ef_addr32 rec, uint8_t key)
{
    fill_colour_record_common(rec, key);
}

/* 0xbaa4c, 107 bytes. Fill a colour record for a key (identical copy). */
void fill_colour_record_baa4c(ef_addr32 rec, uint8_t key)
{
    fill_colour_record_common(rec, key);
}

/* 0xbaad4, 107 bytes. Fill a colour record for a key (identical copy). */
void fill_colour_record_baad4(ef_addr32 rec, uint8_t key)
{
    fill_colour_record_common(rec, key);
}
