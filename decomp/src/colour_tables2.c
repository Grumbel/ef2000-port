/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Colour record fillers (jump-table driven). */

/* 0xba2c0, 249 bytes. Fill a colour record for a key 1..7 (other keys use the default shade). */
void fill_colour_record_ba2c0(ef_addr32 rec, uint8_t key)
{
    switch ((uint8_t)(key - 1u)) {
    case 0:
        EF_U8(rec) = 0x6;
        EF_U8(rec + 1u) = 0x6;
        EF_U8(rec + 2u) = 0x16;
        EF_U8(rec + 3u) = 0x16;
        EF_U8(rec + 4u) = 0x16;
        EF_U8(rec + 5u) = 0x16;
        EF_U8(rec + 6u) = 0x16;
        EF_U8(rec + 7u) = 0x16;
        break;
    case 1:
        EF_U8(rec) = 0xd;
        EF_U8(rec + 1u) = 0xd;
        EF_U8(rec + 2u) = 0xd;
        EF_U8(rec + 3u) = 0xd;
        EF_U8(rec + 4u) = 0xe;
        EF_U8(rec + 5u) = 0xe;
        EF_U8(rec + 6u) = 0x6;
        EF_U8(rec + 7u) = 0x6;
        break;
    case 3:
        EF_U8(rec) = 0x6;
        EF_U8(rec + 1u) = 0x6;
        EF_U8(rec + 2u) = 0x7;
        EF_U8(rec + 3u) = 0x7;
        EF_U8(rec + 4u) = 0x7;
        EF_U8(rec + 5u) = 0x7;
        EF_U8(rec + 6u) = 0x7;
        EF_U8(rec + 7u) = 0x7;
        break;
    case 4:
        EF_U8(rec) = 0x5;
        EF_U8(rec + 1u) = 0x5;
        EF_U8(rec + 2u) = 0xf;
        EF_U8(rec + 3u) = 0xf;
        EF_U8(rec + 4u) = 0xf;
        EF_U8(rec + 5u) = 0xf;
        EF_U8(rec + 6u) = 0x6;
        EF_U8(rec + 7u) = 0x6;
        break;
    case 5:
        EF_U8(rec) = 0xc;
        EF_U8(rec + 1u) = 0xc;
        EF_U8(rec + 2u) = 0xc;
        EF_U8(rec + 3u) = 0xc;
        EF_U8(rec + 4u) = 0xc;
        EF_U8(rec + 5u) = 0xc;
        EF_U8(rec + 6u) = 0x6;
        EF_U8(rec + 7u) = 0x6;
        break;
    case 6:
        EF_U8(rec) = 0x6;
        EF_U8(rec + 1u) = 0x6;
        EF_U8(rec + 2u) = 0x18;
        EF_U8(rec + 3u) = 0x18;
        EF_U8(rec + 4u) = 0x18;
        EF_U8(rec + 5u) = 0x18;
        EF_U8(rec + 6u) = 0x18;
        EF_U8(rec + 7u) = 0x18;
        break;
    default:
        EF_U8(rec) = 0x6;
        EF_U8(rec + 1u) = 0x6;
        EF_U8(rec + 2u) = 0x7;
        EF_U8(rec + 3u) = 0x7;
        EF_U8(rec + 4u) = 0x1a;
        EF_U8(rec + 5u) = 0x1a;
        EF_U8(rec + 6u) = 0x1a;
        EF_U8(rec + 7u) = 0x1a;
        break;
    }
}

/* 0xba460, 257 bytes. Fill a colour record for a key 1..7 (other keys use the default shade). */
void fill_colour_record_ba460(ef_addr32 rec, uint8_t key)
{
    switch ((uint8_t)(key - 1u)) {
    case 1:
        EF_U8(rec) = 0x2;
        EF_U8(rec + 1u) = 0x2;
        EF_U8(rec + 2u) = 0xb;
        EF_U8(rec + 3u) = 0xb;
        EF_U8(rec + 4u) = 0xb;
        EF_U8(rec + 5u) = 0xb;
        EF_U8(rec + 6u) = 0x3;
        EF_U8(rec + 7u) = 0x3;
        break;
    case 2:
        EF_U8(rec) = 0x10;
        EF_U8(rec + 1u) = 0x10;
        EF_U8(rec + 2u) = 0x10;
        EF_U8(rec + 3u) = 0x10;
        EF_U8(rec + 4u) = 0x11;
        EF_U8(rec + 5u) = 0x11;
        EF_U8(rec + 6u) = 0x3;
        EF_U8(rec + 7u) = 0x3;
        EF_U8(rec + 8u) = 0x1e;
        break;
    case 3:
        EF_U8(rec) = 0x2;
        EF_U8(rec + 1u) = 0x2;
        EF_U8(rec + 2u) = 0x3;
        EF_U8(rec + 3u) = 0x3;
        EF_U8(rec + 4u) = 0x3;
        EF_U8(rec + 5u) = 0x3;
        EF_U8(rec + 6u) = 0x3;
        EF_U8(rec + 7u) = 0x3;
        break;
    case 4:
        EF_U8(rec) = 0x2;
        EF_U8(rec + 1u) = 0x2;
        EF_U8(rec + 2u) = 0x2;
        EF_U8(rec + 3u) = 0x2;
        EF_U8(rec + 4u) = 0xa;
        EF_U8(rec + 5u) = 0xa;
        EF_U8(rec + 6u) = 0x3;
        EF_U8(rec + 7u) = 0x3;
        break;
    case 5:
        EF_U8(rec) = 0x2;
        EF_U8(rec + 1u) = 0x2;
        EF_U8(rec + 2u) = 0x8;
        EF_U8(rec + 3u) = 0x8;
        EF_U8(rec + 4u) = 0x8;
        EF_U8(rec + 5u) = 0x8;
        EF_U8(rec + 6u) = 0x3;
        EF_U8(rec + 7u) = 0x3;
        EF_U8(rec + 8u) = 0x20;
        break;
    case 6:
        EF_U8(rec) = 0x14;
        EF_U8(rec + 1u) = 0x14;
        EF_U8(rec + 2u) = 0x14;
        EF_U8(rec + 3u) = 0x14;
        EF_U8(rec + 4u) = 0x14;
        EF_U8(rec + 5u) = 0x14;
        EF_U8(rec + 6u) = 0x3;
        EF_U8(rec + 7u) = 0x3;
        break;
    default:
        EF_U8(rec) = 0x12;
        EF_U8(rec + 1u) = 0x12;
        EF_U8(rec + 2u) = 0x12;
        EF_U8(rec + 3u) = 0x12;
        EF_U8(rec + 4u) = 0x13;
        EF_U8(rec + 5u) = 0x13;
        EF_U8(rec + 6u) = 0x3;
        EF_U8(rec + 7u) = 0x3;
        break;
    }
}

/* 0xba580, 257 bytes. Fill a colour record for a key 1..7 (other keys use the default shade). */
void fill_colour_record_ba580(ef_addr32 rec, uint8_t key)
{
    switch ((uint8_t)(key - 1u)) {
    case 1:
        EF_U8(rec) = 0x2;
        EF_U8(rec + 1u) = 0x2;
        EF_U8(rec + 2u) = 0x2;
        EF_U8(rec + 3u) = 0x2;
        EF_U8(rec + 4u) = 0xb;
        EF_U8(rec + 5u) = 0xb;
        EF_U8(rec + 6u) = 0xb;
        EF_U8(rec + 7u) = 0xb;
        break;
    case 2:
        EF_U8(rec) = 0x2;
        EF_U8(rec + 1u) = 0x2;
        EF_U8(rec + 2u) = 0x2;
        EF_U8(rec + 3u) = 0x2;
        EF_U8(rec + 4u) = 0x10;
        EF_U8(rec + 5u) = 0x10;
        EF_U8(rec + 6u) = 0x10;
        EF_U8(rec + 7u) = 0x10;
        EF_U8(rec + 8u) = 0x10;
        break;
    case 3:
        EF_U8(rec) = 0x2;
        EF_U8(rec + 1u) = 0x2;
        EF_U8(rec + 2u) = 0x3;
        EF_U8(rec + 3u) = 0x3;
        EF_U8(rec + 4u) = 0x3;
        EF_U8(rec + 5u) = 0x3;
        EF_U8(rec + 6u) = 0x3;
        EF_U8(rec + 7u) = 0x3;
        break;
    case 4:
        EF_U8(rec) = 0x2;
        EF_U8(rec + 1u) = 0x2;
        EF_U8(rec + 2u) = 0x3;
        EF_U8(rec + 3u) = 0x3;
        EF_U8(rec + 4u) = 0xa;
        EF_U8(rec + 5u) = 0xa;
        break;
    case 5:
        EF_U8(rec) = 0x2;
        EF_U8(rec + 1u) = 0x2;
        EF_U8(rec + 2u) = 0x15;
        EF_U8(rec + 3u) = 0x15;
        EF_U8(rec + 4u) = 0x8;
        EF_U8(rec + 5u) = 0x8;
        EF_U8(rec + 6u) = 0x8;
        EF_U8(rec + 7u) = 0x8;
        EF_U8(rec + 8u) = 0x20;
        break;
    case 6:
        EF_U8(rec) = 0x2;
        EF_U8(rec + 1u) = 0x2;
        EF_U8(rec + 2u) = 0x2;
        EF_U8(rec + 3u) = 0x2;
        EF_U8(rec + 4u) = 0x14;
        EF_U8(rec + 5u) = 0x14;
        EF_U8(rec + 6u) = 0x14;
        EF_U8(rec + 7u) = 0x14;
        EF_U8(rec + 8u) = 0x14;
        break;
    default:
        EF_U8(rec) = 0x2;
        EF_U8(rec + 1u) = 0x2;
        EF_U8(rec + 2u) = 0x2;
        EF_U8(rec + 3u) = 0x2;
        EF_U8(rec + 4u) = 0x12;
        EF_U8(rec + 5u) = 0x12;
        EF_U8(rec + 6u) = 0x12;
        EF_U8(rec + 7u) = 0x12;
        EF_U8(rec + 8u) = 0x12;
        break;
    }
}

/* 0xba6a0, 189 bytes. Fill a colour record for a key 1..7 (other keys use the default shade). */
void fill_colour_record_ba6a0(ef_addr32 rec, uint8_t key)
{
    switch ((uint8_t)(key - 1u)) {
    case 1:
        EF_U8(rec) = 0xb;
        EF_U8(rec + 1u) = 0xb;
        EF_U8(rec + 2u) = 0xb;
        EF_U8(rec + 3u) = 0xb;
        EF_U8(rec + 4u) = 0x3;
        EF_U8(rec + 5u) = 0x3;
        break;
    case 2:
        EF_U8(rec) = 0x2;
        EF_U8(rec + 1u) = 0x2;
        EF_U8(rec + 2u) = 0x11;
        EF_U8(rec + 3u) = 0x11;
        EF_U8(rec + 6u) = 0x1e;
        break;
    case 3:
        EF_U8(rec) = 0x2;
        EF_U8(rec + 1u) = 0x2;
        EF_U8(rec + 2u) = 0x3;
        EF_U8(rec + 3u) = 0x3;
        EF_U8(rec + 4u) = 0x3;
        EF_U8(rec + 5u) = 0x3;
        break;
    case 4:
        EF_U8(rec) = 0x2;
        EF_U8(rec + 1u) = 0x2;
        EF_U8(rec + 2u) = 0xa;
        EF_U8(rec + 3u) = 0xa;
        EF_U8(rec + 6u) = 0x20;
        break;
    case 5:
        EF_U8(rec) = 0x2;
        EF_U8(rec + 1u) = 0x2;
        EF_U8(rec + 2u) = 0x8;
        EF_U8(rec + 3u) = 0x8;
        EF_U8(rec + 4u) = 0x3;
        EF_U8(rec + 5u) = 0x3;
        EF_U8(rec + 6u) = 0x3;
        break;
    case 6:
        EF_U8(rec) = 0x14;
        EF_U8(rec + 1u) = 0x14;
        EF_U8(rec + 2u) = 0x14;
        EF_U8(rec + 3u) = 0x14;
        EF_U8(rec + 4u) = 0x3;
        EF_U8(rec + 5u) = 0x3;
        EF_U8(rec + 6u) = 0x14;
        break;
    default:
        EF_U8(rec) = 0x2;
        EF_U8(rec + 1u) = 0x2;
        EF_U8(rec + 2u) = 0x13;
        EF_U8(rec + 3u) = 0x13;
        EF_U8(rec + 6u) = 0x13;
        break;
    }
}

/* 0xba804, 185 bytes. Fill a colour record for a key 1..7 (other keys use the default shade). */
void fill_colour_record_ba804(ef_addr32 rec, uint8_t key)
{
    switch ((uint8_t)(key - 1u)) {
    case 1:
        EF_U8(rec) = 0xb;
        EF_U8(rec + 1u) = 0xb;
        EF_U8(rec + 2u) = 0xb;
        EF_U8(rec + 3u) = 0xb;
        EF_U8(rec + 4u) = 0xb;
        EF_U8(rec + 5u) = 0xb;
        EF_U8(rec + 6u) = 0xb;
        break;
    case 2:
        EF_U8(rec) = 0x10;
        EF_U8(rec + 1u) = 0x10;
        EF_U8(rec + 2u) = 0x10;
        EF_U8(rec + 3u) = 0x10;
        EF_U8(rec + 4u) = 0x10;
        EF_U8(rec + 5u) = 0x10;
        EF_U8(rec + 6u) = 0x10;
        break;
    case 3:
        EF_U8(rec) = 0x2;
        EF_U8(rec + 1u) = 0x2;
        EF_U8(rec + 2u) = 0x2;
        EF_U8(rec + 3u) = 0x2;
        EF_U8(rec + 4u) = 0x3;
        EF_U8(rec + 5u) = 0x3;
        break;
    case 5:
        EF_U8(rec) = 0x2;
        EF_U8(rec + 1u) = 0x2;
        EF_U8(rec + 2u) = 0x8;
        EF_U8(rec + 3u) = 0x8;
        EF_U8(rec + 4u) = 0x8;
        EF_U8(rec + 5u) = 0x8;
        break;
    case 6:
        EF_U8(rec) = 0x2;
        EF_U8(rec + 1u) = 0x2;
        EF_U8(rec + 2u) = 0x14;
        EF_U8(rec + 3u) = 0x14;
        EF_U8(rec + 4u) = 0x14;
        EF_U8(rec + 5u) = 0x14;
        EF_U8(rec + 6u) = 0x14;
        break;
    default:
        EF_U8(rec) = 0x12;
        EF_U8(rec + 1u) = 0x12;
        EF_U8(rec + 2u) = 0x12;
        EF_U8(rec + 3u) = 0x12;
        EF_U8(rec + 4u) = 0x12;
        EF_U8(rec + 5u) = 0x12;
        EF_U8(rec + 6u) = 0x12;
        break;
    }
}

/* 0xba8dc, 201 bytes. Fill a colour record for a key 1..7 (other keys use the default shade). */
void fill_colour_record_ba8dc(ef_addr32 rec, uint8_t key)
{
    switch ((uint8_t)(key - 1u)) {
    case 1:
        EF_U8(rec) = 0xb;
        EF_U8(rec + 1u) = 0xb;
        EF_U8(rec + 2u) = 0xb;
        EF_U8(rec + 3u) = 0xb;
        EF_U8(rec + 4u) = 0xb;
        EF_U8(rec + 5u) = 0xb;
        EF_U8(rec + 6u) = 0xb;
        break;
    case 2:
        EF_U8(rec) = 0x10;
        EF_U8(rec + 1u) = 0x10;
        EF_U8(rec + 2u) = 0x10;
        EF_U8(rec + 3u) = 0x10;
        EF_U8(rec + 4u) = 0x10;
        EF_U8(rec + 5u) = 0x10;
        EF_U8(rec + 6u) = 0x10;
        break;
    case 3:
        EF_U8(rec) = 0x2;
        EF_U8(rec + 1u) = 0x2;
        EF_U8(rec + 2u) = 0x2;
        EF_U8(rec + 3u) = 0x2;
        EF_U8(rec + 4u) = 0x3;
        EF_U8(rec + 5u) = 0x3;
        break;
    case 4:
        EF_U8(rec) = 0x2;
        EF_U8(rec + 1u) = 0x2;
        EF_U8(rec + 2u) = 0xa;
        EF_U8(rec + 3u) = 0xa;
        break;
    case 5:
        EF_U8(rec) = 0x2;
        EF_U8(rec + 1u) = 0x2;
        EF_U8(rec + 2u) = 0x8;
        EF_U8(rec + 3u) = 0x8;
        EF_U8(rec + 4u) = 0x8;
        EF_U8(rec + 5u) = 0x8;
        break;
    case 6:
        EF_U8(rec) = 0x2;
        EF_U8(rec + 1u) = 0x2;
        EF_U8(rec + 2u) = 0x14;
        EF_U8(rec + 3u) = 0x14;
        EF_U8(rec + 4u) = 0x14;
        EF_U8(rec + 5u) = 0x14;
        EF_U8(rec + 6u) = 0x14;
        break;
    default:
        EF_U8(rec) = 0x12;
        EF_U8(rec + 1u) = 0x12;
        EF_U8(rec + 2u) = 0x12;
        EF_U8(rec + 3u) = 0x12;
        EF_U8(rec + 4u) = 0x12;
        EF_U8(rec + 5u) = 0x12;
        EF_U8(rec + 6u) = 0x12;
        break;
    }
}

/* 0xbab5c, 85 bytes. Fill a colour record for a key 1..7 (other keys use the default shade). */
void fill_colour_record_bab5c(ef_addr32 rec, uint8_t key)
{
    switch ((uint8_t)(key - 1u)) {
    case 1:
        EF_U8(rec) = 0xd;
        EF_U8(rec + 1u) = 0xd;
        EF_U8(rec + 2u) = 0xd;
        EF_U8(rec + 3u) = 0xd;
        break;
    case 3:
        EF_U8(rec) = 0x6;
        EF_U8(rec + 1u) = 0x6;
        EF_U8(rec + 2u) = 0x6;
        EF_U8(rec + 3u) = 0x6;
        break;
    case 6:
        EF_U8(rec) = 0x18;
        EF_U8(rec + 1u) = 0x18;
        EF_U8(rec + 2u) = 0x18;
        EF_U8(rec + 3u) = 0x18;
        break;
    default:
        EF_U8(rec) = 0x16;
        EF_U8(rec + 1u) = 0x16;
        EF_U8(rec + 2u) = 0x16;
        EF_U8(rec + 3u) = 0x16;
        break;
    }
}

/* 0xbabd0, 162 bytes. Fill a colour record for a key 1..7 (other keys use the default shade). */
void fill_colour_record_babd0(ef_addr32 rec, uint8_t key)
{
    switch ((uint8_t)(key - 1u)) {
    case 1:
        EF_U8(rec) = 0xd;
        EF_U8(rec + 1u) = 0xd;
        EF_U8(rec + 2u) = 0xe;
        EF_U8(rec + 4u) = 0xe;
        EF_U8(rec + 3u) = 0x6;
        EF_U8(rec + 5u) = 0x6;
        break;
    case 2:
        EF_U8(rec) = 0x1a;
        EF_U8(rec + 1u) = 0x1a;
        EF_U8(rec + 2u) = 0x1a;
        EF_U8(rec + 3u) = 0x1a;
        EF_U8(rec + 4u) = 0x1a;
        EF_U8(rec + 5u) = 0x1a;
        break;
    case 3:
        EF_U8(rec) = 0x5;
        EF_U8(rec + 1u) = 0x5;
        EF_U8(rec + 2u) = 0x6;
        EF_U8(rec + 3u) = 0x6;
        EF_U8(rec + 4u) = 0x6;
        EF_U8(rec + 5u) = 0x6;
        break;
    case 5:
        EF_U8(rec) = 0xc;
        EF_U8(rec + 1u) = 0xc;
        EF_U8(rec + 2u) = 0x6;
        EF_U8(rec + 3u) = 0x6;
        EF_U8(rec + 4u) = 0x6;
        EF_U8(rec + 5u) = 0x6;
        break;
    case 6:
        EF_U8(rec + 2u) = 0x18;
        EF_U8(rec + 3u) = 0x18;
        EF_U8(rec + 4u) = 0x18;
        EF_U8(rec + 5u) = 0x18;
        break;
    default:
        EF_U8(rec) = 0x17;
        EF_U8(rec + 1u) = 0x17;
        EF_U8(rec + 2u) = 0x16;
        EF_U8(rec + 3u) = 0x16;
        EF_U8(rec + 4u) = 0x16;
        EF_U8(rec + 5u) = 0x16;
        break;
    }
}

/* 0xbac90, 193 bytes. Fill a colour record for a key 1..7 (other keys use the default shade). */
void fill_colour_record_bac90(ef_addr32 rec, uint8_t key)
{
    switch ((uint8_t)(key - 1u)) {
    case 1:
        EF_U8(rec) = 0xd;
        EF_U8(rec + 1u) = 0xd;
        EF_U8(rec + 2u) = 0xe;
        EF_U8(rec + 3u) = 0xe;
        EF_U8(rec + 4u) = 0x6;
        EF_U8(rec + 5u) = 0x6;
        break;
    case 2:
        EF_U8(rec) = 0x1a;
        EF_U8(rec + 1u) = 0x1a;
        EF_U8(rec + 2u) = 0x1a;
        EF_U8(rec + 3u) = 0x1a;
        EF_U8(rec + 4u) = 0x1a;
        EF_U8(rec + 5u) = 0x1a;
        break;
    case 3:
        EF_U8(rec) = 0x5;
        EF_U8(rec + 1u) = 0x5;
        EF_U8(rec + 2u) = 0x6;
        EF_U8(rec + 3u) = 0x6;
        EF_U8(rec + 4u) = 0x7;
        EF_U8(rec + 5u) = 0x7;
        break;
    case 4:
        EF_U8(rec) = 0x5;
        EF_U8(rec + 1u) = 0x5;
        EF_U8(rec + 2u) = 0xf;
        EF_U8(rec + 3u) = 0xf;
        EF_U8(rec + 4u) = 0x6;
        EF_U8(rec + 5u) = 0x6;
        break;
    case 5:
        EF_U8(rec) = 0xc;
        EF_U8(rec + 1u) = 0xc;
        EF_U8(rec + 2u) = 0xc;
        EF_U8(rec + 3u) = 0xc;
        EF_U8(rec + 4u) = 0x6;
        EF_U8(rec + 5u) = 0x6;
        break;
    case 6:
        EF_U8(rec) = 0x18;
        EF_U8(rec + 1u) = 0x18;
        EF_U8(rec + 2u) = 0x18;
        EF_U8(rec + 3u) = 0x18;
        EF_U8(rec + 4u) = 0x18;
        EF_U8(rec + 5u) = 0x18;
        break;
    default:
        EF_U8(rec) = 0x16;
        EF_U8(rec + 1u) = 0x16;
        EF_U8(rec + 2u) = 0x16;
        EF_U8(rec + 3u) = 0x16;
        EF_U8(rec + 4u) = 0x16;
        EF_U8(rec + 5u) = 0x16;
        break;
    }
}

/* 0xbad70, 193 bytes. Fill a colour record for a key 1..7 (other keys use the default shade). */
void fill_colour_record_bad70(ef_addr32 rec, uint8_t key)
{
    switch ((uint8_t)(key - 1u)) {
    case 1:
        EF_U8(rec) = 0xb;
        EF_U8(rec + 1u) = 0xb;
        EF_U8(rec + 2u) = 0x9;
        EF_U8(rec + 3u) = 0x9;
        EF_U8(rec + 4u) = 0x3;
        EF_U8(rec + 5u) = 0x3;
        break;
    case 2:
        EF_U8(rec) = 0x11;
        EF_U8(rec + 1u) = 0x11;
        EF_U8(rec + 2u) = 0x11;
        EF_U8(rec + 3u) = 0x11;
        EF_U8(rec + 4u) = 0x11;
        EF_U8(rec + 5u) = 0x11;
        break;
    case 3:
        EF_U8(rec) = 0x2;
        EF_U8(rec + 1u) = 0x2;
        EF_U8(rec + 2u) = 0x3;
        EF_U8(rec + 3u) = 0x3;
        EF_U8(rec + 4u) = 0x4;
        EF_U8(rec + 5u) = 0x4;
        break;
    case 4:
        EF_U8(rec) = 0x2;
        EF_U8(rec + 1u) = 0x2;
        EF_U8(rec + 2u) = 0xa;
        EF_U8(rec + 3u) = 0xa;
        EF_U8(rec + 4u) = 0x3;
        EF_U8(rec + 5u) = 0x3;
        break;
    case 5:
        EF_U8(rec) = 0x8;
        EF_U8(rec + 1u) = 0x8;
        EF_U8(rec + 2u) = 0x8;
        EF_U8(rec + 3u) = 0x8;
        EF_U8(rec + 4u) = 0x3;
        EF_U8(rec + 5u) = 0x3;
        break;
    case 6:
        EF_U8(rec) = 0x14;
        EF_U8(rec + 1u) = 0x14;
        EF_U8(rec + 2u) = 0x14;
        EF_U8(rec + 3u) = 0x14;
        EF_U8(rec + 4u) = 0x14;
        EF_U8(rec + 5u) = 0x14;
        break;
    default:
        EF_U8(rec) = 0x12;
        EF_U8(rec + 1u) = 0x12;
        EF_U8(rec + 2u) = 0x12;
        EF_U8(rec + 3u) = 0x12;
        EF_U8(rec + 4u) = 0x12;
        EF_U8(rec + 5u) = 0x12;
        break;
    }
}

/* 0xbae78, 169 bytes. Fill a colour record for a key 1..7 (other keys use the default shade). */
void fill_colour_record_bae78(ef_addr32 rec, uint8_t key)
{
    switch ((uint8_t)(key - 1u)) {
    case 1:
        EF_U8(rec) = 0xb;
        EF_U8(rec + 1u) = 0xb;
        EF_U8(rec + 2u) = 0xb;
        EF_U8(rec + 3u) = 0xb;
        EF_U8(rec + 4u) = 0xb;
        EF_U8(rec + 5u) = 0xb;
        break;
    case 2:
        EF_U8(rec) = 0x11;
        EF_U8(rec + 1u) = 0x11;
        EF_U8(rec + 2u) = 0x11;
        EF_U8(rec + 3u) = 0x11;
        EF_U8(rec + 4u) = 0x11;
        EF_U8(rec + 5u) = 0x1f;
        break;
    case 4:
        EF_U8(rec) = 0xa;
        EF_U8(rec + 1u) = 0xa;
        EF_U8(rec + 2u) = 0x3;
        EF_U8(rec + 3u) = 0x3;
        EF_U8(rec + 4u) = 0x3;
        EF_U8(rec + 5u) = 0x3;
        break;
    case 5:
        EF_U8(rec) = 0x8;
        EF_U8(rec + 1u) = 0x8;
        EF_U8(rec + 2u) = 0x8;
        EF_U8(rec + 3u) = 0x8;
        EF_U8(rec + 4u) = 0x8;
        EF_U8(rec + 5u) = 0x20;
        break;
    case 6:
        EF_U8(rec) = 0x14;
        EF_U8(rec + 1u) = 0x14;
        EF_U8(rec + 2u) = 0x14;
        EF_U8(rec + 3u) = 0x14;
        EF_U8(rec + 4u) = 0x14;
        EF_U8(rec + 5u) = 0x14;
        break;
    default:
        EF_U8(rec) = 0x13;
        EF_U8(rec + 1u) = 0x13;
        EF_U8(rec + 2u) = 0x13;
        EF_U8(rec + 3u) = 0x13;
        EF_U8(rec + 4u) = 0x13;
        EF_U8(rec + 5u) = 0x13;
        break;
    }
}

/* 0xbaf48, 125 bytes. Fill a colour record for a key 1..7 (other keys use the default shade). */
void fill_colour_record_baf48(ef_addr32 rec, uint8_t key)
{
    switch ((uint8_t)(key - 1u)) {
    case 2:
        EF_U8(rec) = 0x1a;
        EF_U8(rec + 1u) = 0x1a;
        EF_U8(rec + 2u) = 0x1a;
        EF_U8(rec + 3u) = 0x1a;
        EF_U8(rec + 4u) = 0x1a;
        EF_U8(rec + 5u) = 0x1a;
        EF_U8(rec + 6u) = 0x1a;
        EF_U8(rec + 7u) = 0x1a;
        break;
    case 4:
        EF_U8(rec) = 0xf;
        EF_U8(rec + 1u) = 0xf;
        break;
    case 6:
        EF_U8(rec) = 0x18;
        EF_U8(rec + 1u) = 0x18;
        EF_U8(rec + 2u) = 0x18;
        EF_U8(rec + 3u) = 0x18;
        EF_U8(rec + 4u) = 0x18;
        EF_U8(rec + 5u) = 0x18;
        EF_U8(rec + 6u) = 0x18;
        EF_U8(rec + 7u) = 0x18;
        break;
    default:
        EF_U8(rec) = 0x17;
        EF_U8(rec + 1u) = 0x17;
        EF_U8(rec + 2u) = 0x17;
        EF_U8(rec + 3u) = 0x17;
        EF_U8(rec + 4u) = 0x17;
        EF_U8(rec + 5u) = 0x17;
        EF_U8(rec + 6u) = 0x17;
        EF_U8(rec + 7u) = 0x17;
        break;
    }
}

/* 0xbafe4, 169 bytes. Fill a colour record for a key 1..7 (other keys use the default shade). */
void fill_colour_record_bafe4(ef_addr32 rec, uint8_t key)
{
    switch ((uint8_t)(key - 1u)) {
    case 1:
        EF_U8(rec) = 0xd;
        EF_U8(rec + 1u) = 0xd;
        EF_U8(rec + 2u) = 0xd;
        EF_U8(rec + 3u) = 0xd;
        EF_U8(rec + 4u) = 0xd;
        EF_U8(rec + 5u) = 0xd;
        break;
    case 2:
        EF_U8(rec) = 0x1a;
        EF_U8(rec + 1u) = 0x1a;
        EF_U8(rec + 2u) = 0x1a;
        EF_U8(rec + 3u) = 0x1a;
        EF_U8(rec + 4u) = 0x1a;
        EF_U8(rec + 5u) = 0x1a;
        break;
    case 4:
        EF_U8(rec) = 0xf;
        EF_U8(rec + 1u) = 0xf;
        EF_U8(rec + 2u) = 0xf;
        EF_U8(rec + 3u) = 0xf;
        EF_U8(rec + 4u) = 0xf;
        EF_U8(rec + 5u) = 0xf;
        break;
    case 5:
        EF_U8(rec) = 0xc;
        EF_U8(rec + 1u) = 0xc;
        EF_U8(rec + 2u) = 0xc;
        EF_U8(rec + 3u) = 0xc;
        EF_U8(rec + 4u) = 0xc;
        EF_U8(rec + 5u) = 0xc;
        break;
    case 6:
        EF_U8(rec) = 0x18;
        EF_U8(rec + 1u) = 0x18;
        EF_U8(rec + 2u) = 0x18;
        EF_U8(rec + 3u) = 0x18;
        EF_U8(rec + 4u) = 0x18;
        EF_U8(rec + 5u) = 0x18;
        break;
    default:
        EF_U8(rec) = 0x17;
        EF_U8(rec + 1u) = 0x17;
        EF_U8(rec + 2u) = 0x17;
        EF_U8(rec + 3u) = 0x17;
        EF_U8(rec + 4u) = 0x17;
        EF_U8(rec + 5u) = 0x17;
        break;
    }
}

/* 0xbb0ac, 225 bytes. Fill a colour record for a key 1..7 (other keys use the default shade). */
void fill_colour_record_bb0ac(ef_addr32 rec, uint8_t key)
{
    switch ((uint8_t)(key - 1u)) {
    case 1:
        EF_U8(rec) = 0xd;
        EF_U8(rec + 1u) = 0xd;
        EF_U8(rec + 2u) = 0xd;
        EF_U8(rec + 3u) = 0xd;
        EF_U8(rec + 4u) = 0xd;
        EF_U8(rec + 5u) = 0xd;
        EF_U8(rec + 6u) = 0xd;
        EF_U8(rec + 7u) = 0xd;
        EF_U8(rec + 8u) = 0xd;
        EF_U8(rec + 9u) = 0xd;
        break;
    case 2:
        EF_U8(rec) = 0x1a;
        EF_U8(rec + 1u) = 0x1a;
        EF_U8(rec + 2u) = 0x1a;
        EF_U8(rec + 3u) = 0x1a;
        EF_U8(rec + 4u) = 0x1a;
        EF_U8(rec + 5u) = 0x1a;
        EF_U8(rec + 6u) = 0x1a;
        EF_U8(rec + 7u) = 0x1a;
        EF_U8(rec + 8u) = 0x1a;
        EF_U8(rec + 9u) = 0x1a;
        break;
    case 5:
        EF_U8(rec) = 0xc;
        EF_U8(rec + 1u) = 0xc;
        EF_U8(rec + 2u) = 0xc;
        EF_U8(rec + 3u) = 0xc;
        EF_U8(rec + 4u) = 0xc;
        EF_U8(rec + 5u) = 0xc;
        EF_U8(rec + 6u) = 0xc;
        EF_U8(rec + 7u) = 0xc;
        EF_U8(rec + 8u) = 0xc;
        EF_U8(rec + 9u) = 0xc;
        break;
    case 6:
        EF_U8(rec) = 0x18;
        EF_U8(rec + 1u) = 0x18;
        EF_U8(rec + 2u) = 0x18;
        EF_U8(rec + 3u) = 0x18;
        EF_U8(rec + 4u) = 0x18;
        EF_U8(rec + 5u) = 0x18;
        EF_U8(rec + 6u) = 0x18;
        EF_U8(rec + 7u) = 0x18;
        EF_U8(rec + 8u) = 0x18;
        EF_U8(rec + 9u) = 0x18;
        break;
    default:
        EF_U8(rec) = 0x17;
        EF_U8(rec + 1u) = 0x17;
        EF_U8(rec + 2u) = 0x17;
        EF_U8(rec + 3u) = 0x17;
        EF_U8(rec + 4u) = 0x17;
        EF_U8(rec + 5u) = 0x17;
        EF_U8(rec + 6u) = 0x17;
        EF_U8(rec + 7u) = 0x17;
        EF_U8(rec + 8u) = 0x17;
        EF_U8(rec + 9u) = 0x17;
        break;
    }
}

/* 0xbb1ac, 225 bytes. Fill a colour record for a key 1..7 (other keys use the default shade). */
void fill_colour_record_bb1ac(ef_addr32 rec, uint8_t key)
{
    switch ((uint8_t)(key - 1u)) {
    case 1:
        EF_U8(rec) = 0xb;
        EF_U8(rec + 1u) = 0xb;
        EF_U8(rec + 2u) = 0xb;
        EF_U8(rec + 3u) = 0xb;
        EF_U8(rec + 4u) = 0xb;
        EF_U8(rec + 5u) = 0xb;
        EF_U8(rec + 6u) = 0xb;
        EF_U8(rec + 7u) = 0xb;
        EF_U8(rec + 8u) = 0xb;
        EF_U8(rec + 9u) = 0xb;
        break;
    case 2:
        EF_U8(rec) = 0x11;
        EF_U8(rec + 1u) = 0x11;
        EF_U8(rec + 2u) = 0x11;
        EF_U8(rec + 3u) = 0x11;
        EF_U8(rec + 4u) = 0x11;
        EF_U8(rec + 5u) = 0x11;
        EF_U8(rec + 6u) = 0x11;
        EF_U8(rec + 7u) = 0x11;
        EF_U8(rec + 8u) = 0x11;
        EF_U8(rec + 9u) = 0x11;
        break;
    case 5:
        EF_U8(rec) = 0x8;
        EF_U8(rec + 1u) = 0x8;
        EF_U8(rec + 2u) = 0x8;
        EF_U8(rec + 3u) = 0x8;
        EF_U8(rec + 4u) = 0x8;
        EF_U8(rec + 5u) = 0x8;
        EF_U8(rec + 6u) = 0x8;
        EF_U8(rec + 7u) = 0x8;
        EF_U8(rec + 8u) = 0x8;
        EF_U8(rec + 9u) = 0x8;
        break;
    case 6:
        EF_U8(rec) = 0x14;
        EF_U8(rec + 1u) = 0x14;
        EF_U8(rec + 2u) = 0x14;
        EF_U8(rec + 3u) = 0x14;
        EF_U8(rec + 4u) = 0x14;
        EF_U8(rec + 5u) = 0x14;
        EF_U8(rec + 6u) = 0x14;
        EF_U8(rec + 7u) = 0x14;
        EF_U8(rec + 8u) = 0x14;
        EF_U8(rec + 9u) = 0x14;
        break;
    default:
        EF_U8(rec) = 0x13;
        EF_U8(rec + 1u) = 0x13;
        EF_U8(rec + 2u) = 0x13;
        EF_U8(rec + 3u) = 0x13;
        EF_U8(rec + 4u) = 0x13;
        EF_U8(rec + 5u) = 0x13;
        EF_U8(rec + 6u) = 0x13;
        EF_U8(rec + 7u) = 0x13;
        EF_U8(rec + 8u) = 0x13;
        EF_U8(rec + 9u) = 0x13;
        break;
    }
}
