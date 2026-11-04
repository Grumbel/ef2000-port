/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Record reactivation around a cell for the entry bookkeeping. */

/* 0xc08d4, 270 bytes. For each record at the object's position with the given type, reactivate state-7 records (counting them in the object's tables, capped at 0x31) and state-2 records. */
void reactivate_records_at_object_c08d4(uint8_t type, ef_addr32 object, ef_addr32 list)
{
    ef_addr32 rec = EF_U32(list);

    while (rec != 0 && EF_U8(object + 0x32u) < 0x31) {
        if (EF_U8(rec + 0xbu) == 0) {
            if (EF_U32(rec + 0x3bu) == EF_U32(object + 0x11u)
                && EF_U32(rec + 0x3fu) == EF_U32(object + 0x15u)
                && EF_U32(rec + 0x43u) == EF_U32(object + 0x19u)
                && EF_U8(rec + 0xeu) == type
                && EF_U8(rec + 0xdu) == 7) {
                uint32_t n;
                uint32_t v;

                EF_U8(rec + 0xdu) = 0;
                EF_U8(rec + 0x15u) = 100;
                n = (uint32_t)EF_U8(object + 0x32u) + 1u;
                if (n > 0x31)
                    n = 0x31;
                EF_U8(object + 0x32u) = (uint8_t)n;
                v = (uint32_t)EF_U8(EF_U32(object + 0x33u) + EF_U8(rec + 0xcu)) + 1u;
                if (v > 0x31)
                    v = 0x31;
                EF_U8(EF_U32(object + 0x33u) + EF_U8(rec + 0xcu)) = (uint8_t)v;
            }
        } else if (EF_U8(rec + 0xbu) == 2) {
            if (EF_U32(rec + 0x3bu) == EF_U32(object + 0x11u)
                && EF_U32(rec + 0x3fu) == EF_U32(object + 0x15u)
                && EF_U32(rec + 0x43u) == EF_U32(object + 0x19u)
                && EF_U8(rec + 0xeu) == type) {
                EF_U8(rec + 0xdu) = 0;
                EF_U8(rec + 0x15u) = 100;
            }
        }
        rec = EF_U32(rec + 4u);
    }
}

/* 0xc09e4, 202 bytes. Refill an object's sub-entries of kind 0x35/0x37, then reactivate matching non-idle records up to the free capacity of its cell (0x30 minus the cell's record count). */
void refill_object_cell_c09e4(ef_addr32 object, ef_addr32 list)
{
    ef_addr32 rec = EF_U32(list);
    int16_t i = 0;
    int16_t limit;
    int16_t done = 0;

    while (i < EF_S16(object + 0x2au)) {
        ef_addr32 sub = EF_U32(object + 0x2cu) + (uint32_t)(int32_t)i * 0x15u;
        uint8_t code = EF_U8(sub);

        if (code == 0x35 || code == 0x37)
            EF_U8(sub + 5u) = 100;
        i++;
    }
    limit = (int16_t)(0x30 - (int16_t)count_records_at_cell_bfb2c((int16_t)EF_S16(object + 0x16u), (int16_t)EF_S16(object + 0x1au), list));
    if (limit < 0)
        limit = 0;
    while (rec != 0 && done < limit) {
        if (EF_U32(rec + 0x3bu) == EF_U32(object + 0x16u)
            && EF_U32(rec + 0x3fu) == EF_U32(object + 0x1au)
            && EF_U8(rec + 0xeu) == EF_U8(object + 9u)
            && EF_U8(rec + 0xdu) != 0) {
            EF_U8(rec + 0xdu) = 0;
            EF_U8(rec + 0x15u) = 100;
            done++;
            EF_U16(rec + 9u) = 0;
        }
        rec = EF_U32(rec + 4u);
    }
}
