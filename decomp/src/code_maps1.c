/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Jump-table code mappers. */

/* 0x3aa8, 61 bytes. Map a code (1-based) to a constant via a jump table (codes outside the range give 26). */
uint8_t map_code_3aa8(uint32_t code)
{
    switch ((uint8_t)(code - 1u)) {
    case 0: return 0;
    case 1: return 1;
    case 2: return 2;
    case 4: return 3;
    case 5: return 4;
    case 6: return 5;
    case 7: return 6;
    case 8: return 7;
    case 9: return 8;
    case 10: return 9;
    case 13: return 10;
    case 14: return 11;
    case 15: return 12;
    default: return 26;
    }
}

/* 0x3b1c, 59 bytes. Map a code to a constant via a jump table (codes outside the range give 26). */
uint8_t map_code_3b1c(uint32_t code)
{
    switch ((uint8_t)(code)) {
    case 0: return 13;
    case 1: return 14;
    case 2: return 15;
    case 3: return 16;
    case 4: return 17;
    case 5: return 18;
    case 6: return 19;
    case 7: return 20;
    case 8: return 21;
    case 9: return 22;
    case 10: return 23;
    case 11: return 24;
    case 12: return 25;
    default: return 26;
    }
}

/* 0x74448, 53 bytes. Map a code to a constant via a jump table (codes outside the range give 1). */
uint32_t map_code_74448(uint32_t code)
{
    switch ((uint8_t)(code)) {
    case 1: return 2;
    case 17: return 2;
    case 18: return 2;
    case 19: return 4;
    case 20: return 2;
    case 21: return 2;
    case 24: return 6;
    case 25: return 2;
    case 26: return 10;
    case 27: return 4;
    case 28: return 2;
    case 29: return 4;
    case 30: return 10;
    case 31: return 2;
    case 32: return 3;
    case 33: return 3;
    case 34: return 6;
    case 35: return 6;
    case 36: return 4;
    default: return 1;
    }
}

/* 0xba0e4, 43 bytes. Map a code (1-based) to a constant via a jump table (codes outside the range give 0). */
uint8_t map_code_ba0e4(uint32_t code)
{
    switch ((uint8_t)(code - 1u)) {
    case 0: return 3;
    case 1: return 3;
    case 2: return 3;
    case 3: return 3;
    case 5: return 3;
    case 6: return 2;
    case 7: return 3;
    case 8: return 3;
    case 9: return 3;
    case 11: return 7;
    case 12: return 3;
    case 13: return 1;
    case 14: return 1;
    case 15: return 1;
    case 16: return 1;
    case 17: return 1;
    case 19: return 3;
    case 20: return 3;
    case 21: return 3;
    case 22: return 3;
    case 23: return 3;
    case 24: return 2;
    case 25: return 7;
    case 26: return 1;
    case 27: return 3;
    case 29: return 1;
    case 30: return 1;
    case 31: return 3;
    case 32: return 1;
    case 33: return 1;
    case 34: return 1;
    case 35: return 3;
    case 36: return 1;
    case 38: return 1;
    case 39: return 1;
    case 40: return 3;
    case 41: return 3;
    case 43: return 1;
    case 44: return 1;
    case 45: return 1;
    case 46: return 1;
    case 47: return 1;
    case 49: return 3;
    case 50: return 1;
    case 51: return 1;
    case 52: return 1;
    case 53: return 7;
    case 54: return 7;
    case 55: return 1;
    case 57: return 6;
    case 58: return 6;
    case 59: return 6;
    case 60: return 6;
    case 61: return 6;
    case 62: return 6;
    case 63: return 6;
    case 64: return 2;
    case 65: return 2;
    case 66: return 2;
    case 71: return 2;
    case 73: return 1;
    case 74: return 1;
    case 75: return 1;
    case 76: return 1;
    case 77: return 1;
    case 79: return 3;
    case 80: return 3;
    case 81: return 1;
    case 82: return 3;
    case 83: return 1;
    case 84: return 1;
    case 89: return 2;
    case 90: return 2;
    case 91: return 2;
    case 92: return 2;
    case 93: return 2;
    case 94: return 2;
    case 95: return 2;
    case 96: return 2;
    case 97: return 2;
    case 98: return 2;
    case 99: return 2;
    case 100: return 4;
    case 101: return 4;
    case 102: return 4;
    case 103: return 4;
    case 104: return 5;
    default: return 0;
    }
}

/* 0xbbdc4, 106 bytes. Map an index through the mapper chosen by kind (0..3: 0xbbbe4, 0xbbb34, 0xbbae4, 0xbbd8c) and look the result up in the 11-byte rows at 0x134087; kinds above 3 give 0. */
uint32_t lookup_mapped_code_bbdc4(uint32_t kind, uint32_t index, uint32_t row)
{
    uint8_t r;
    switch ((uint8_t)kind) {
    case 0: r = map_code_flag_bbbe4((uint8_t)index); break;
    case 1: r = map_code_5_6_bbb34((uint8_t)index); break;
    case 2: r = map_code_9_10_bbae4((uint8_t)index); break;
    case 3: r = map_code_class_bbd8c((uint8_t)index); break;
    default: return 0u;
    }
    return EF_U8(0x00134087u + (row & 0xffu) * 11u + r);
}
