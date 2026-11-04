/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"

/* Scan code classification. */

/* 0xbeb20, 353 bytes. Map a keyboard scan code to the 1..23 index of the matching function key, 0 for none. */
uint32_t scan_code_to_function_key_beb20(uint8_t code)
{
    switch (code) {
    case 0x39: return 1;
    case 0x49: return 2;
    case 0x56: return 3;
    case 0x26: return 4;
    case 0x0b: return 5;
    case 0x13: return 6;
    case 0x05: return 7;
    case 0x2b: return 8;
    case 0x44: return 9;
    case 0x45: return 10;
    case 0x46: return 11;
    case 0x47: return 12;
    case 0x4f: return 13;
    case 0x1d: return 14;
    case 0x00: return 15;
    case 0x57: return 16;
    case 0x58: return 17;
    case 0x59: return 18;
    case 0x65: return 19;
    case 0x66: return 20;
    case 0x67: return 21;
    case 0x68: return 22;
    case 0x69: return 23;
    default: return 0;
    }
}
