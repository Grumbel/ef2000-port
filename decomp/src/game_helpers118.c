/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Free slot run search in a 9-byte entry table. */

/* 0xb58a8, 253 bytes. Search the object's slot table (9-byte entries, count at +0x48, flag byte at +3 meaning used) from start for a slot whose following window holds at least need free slots; falls back to the first free slot, or the last slot. */
int32_t find_free_slot_run_b58a8(ef_addr32 obj, int16_t start, int16_t need)
{
    int16_t result = -1;
    int16_t first_free = 1;
    int16_t j = start;

    if (start >= 0 && start < EF_S16(obj + 0x48u)) {
        for (;;) {
            ef_addr32 tbl = EF_U32(obj + 0x4au);

            if (EF_U8(tbl + (uint32_t)(int32_t)j * 9u + 3u) == 0) {
                int32_t half = (int32_t)need / 2;
                int16_t end;
                int16_t k;
                int16_t cnt = 0;

                if (first_free != 0) {
                    result = j;
                    first_free = 0;
                }
                if (half > 2)
                    half = 2;
                end = (int16_t)(half + (int32_t)need);
                k = j;
                while (k < end && k < EF_S16(obj + 0x48u)) {
                    if (EF_U8(EF_U32(obj + 0x4au) + (uint32_t)(int32_t)k * 9u + 3u) == 0)
                        cnt++;
                    k++;
                }
                if (cnt >= need)
                    return j;
            }
            j++;
            if (j >= EF_S16(obj + 0x48u))
                break;
        }
    }
    if (result == -1 || result >= EF_S16(obj + 0x48u))
        return (int32_t)EF_S16(obj + 0x48u) - 1;
    return result;
}
