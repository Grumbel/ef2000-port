/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Ready sub-entry counting. */

/* 0xbcda0, 246 bytes. Count the ready sub-entries of an object (see bcc2c); types 0xb/0x56 also clear the flag at +0x36. */
uint8_t count_ready_entries_bcda0(ef_addr32 obj)
{
    uint32_t type = EF_U8(obj + 0xbu);
    uint8_t n = 0;
    int16_t i;

    if (type == 0x31 || type == 0x49) {
        i = 0;
        if (EF_S16(obj + 0x2au) > 0) {
            do {
                ef_addr32 sub = EF_U32(obj + 0x2cu) + (uint32_t)(int32_t)i * 0x15u;
                uint32_t code = EF_U8(sub);

                if (type == 0x31) {
                    if (code == 0x37 && (uint16_t)record_is_strong(sub) != 0)
                        n++;
                } else if (code == 0x4a || code == 0x4b || code == 0x4c) {
                    n++;
                } else if (code == 0x4d && (uint16_t)record_is_strong(sub) != 0) {
                    n++;
                }
                i++;
            } while (i < EF_S16(obj + 0x2au));
        }
    } else if (type == 0xb || type == 0x56) {
        uint8_t want = (uint8_t)map_code_bcc04((uint8_t)type);

        if (want != 0) {
            i = 0;
            if (EF_S16(obj + 0x2au) > 0) {
                do {
                    ef_addr32 sub = EF_U32(obj + 0x2cu) + (uint32_t)(int32_t)i * 0x15u;

                    if (EF_U8(sub) == want && (uint16_t)record_is_strong(sub) != 0)
                        n++;
                    i++;
                } while (i < EF_S16(obj + 0x2au));
            }
            EF_U16(obj + 0x36u) = 0;
        }
    }
    return n;
}
