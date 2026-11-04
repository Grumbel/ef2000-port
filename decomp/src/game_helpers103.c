/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* 16-bit distance approximation and record ready test. */

/* 0xb3830, 343 bytes. Approximate 3D distance from three 16-bit components: 15/16 of the largest absolute value plus 3/8 of the other two. */
int32_t approx_distance3_16_b3830(int16_t a, int16_t b, int16_t c)
{
    int16_t x = (int16_t)abs_a28f2((int32_t)a);
    int16_t y = (int16_t)abs_a28f2((int32_t)b);
    int16_t z = (int16_t)abs_a28f2((int32_t)c);
    int16_t t;

    if (x < y) { t = x; x = y; y = t; }
    if (x < z) { t = x; x = z; z = t; }
    if (y < z) { t = y; y = z; z = t; }
    return (((int32_t)y + (int32_t)z) * 3) / 8 + ((int32_t)x * 15) / 16;
}

/* 0xbcc2c, 332 bytes. Check that an object has a ready sub-entry (matching its type's code) and set the flag at +0x36; returns 1 or 0. */
uint32_t object_has_ready_entry_bcc2c(ef_addr32 obj)
{
    uint32_t type = EF_U8(obj + 0xbu);
    int16_t i;

    if (type == 0x31 || type == 0x49) {
        i = 0;
        if ((int16_t)EF_S16(obj + 0x2au) > 0) {
            do {
                ef_addr32 sub = EF_U32(obj + 0x2cu) + (uint32_t)(int32_t)i * 0x15u;
                uint32_t code = EF_U8(sub);
                int match;

                if (type == 0x31)
                    match = (code == 0x37);
                else
                    match = (code == 0x4b || code == 0x4c || code == 0x4d || code == 0x4a || code == 0x4e);
                if (match && (uint16_t)record_is_strong(sub) != 0) {
                    EF_U16(obj + 0x36u) = 1;
                    return 1;
                }
                i++;
            } while (i < EF_S16(obj + 0x2au));
        }
        EF_U16(obj + 0x36u) = 0;
        return 1;
    }
    if (type == 0xb || type == 0x56) {
        uint8_t want = map_code_bcc04((uint8_t)type);

        if (want != 0) {
            i = 0;
            if (EF_S16(obj + 0x2au) > 0) {
                do {
                    ef_addr32 sub = EF_U32(obj + 0x2cu) + (uint32_t)(int32_t)i * 0x15u;

                    if (EF_U8(sub) == want && (uint16_t)record_is_strong(sub) != 0) {
                        EF_U16(obj + 0x36u) = 1;
                        return 1;
                    }
                    i++;
                } while (i < EF_S16(obj + 0x2au));
            }
        }
    }
    EF_U16(obj + 0x36u) = 0;
    return 0;
}
