/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Record deactivation bookkeeping. */

/* 0xc07a0, 305 bytes. Disable (state 7) every record at the object's position with the object's type, decrementing the object's counters. */
/* ORIGINAL BUG: for record kinds other than 0/1 the first counter is stored at +0x3d instead of +0x32. */
void deactivate_records_at_object_c07a0(ef_addr32 object, ef_addr32 list)
{
    ef_addr32 rec = EF_U32(list);

    while (rec != 0) {
        if (EF_U32(rec + 0x3bu) == EF_U32(object + 0x11u)
            && EF_U32(rec + 0x3fu) == EF_U32(object + 0x15u)
            && EF_U32(rec + 0x43u) == EF_U32(object + 0x19u)
            && EF_U8(rec + 0xeu) == EF_U8(object + 0xbu)
            && EF_U8(rec + 0xdu) != 1
            && EF_U8(rec + 0xdu) != 7) {
            uint32_t kind;
            int32_t v;
            int32_t c;
            ef_addr32 table;

            EF_U8(rec + 0xdu) = 7;
            EF_U8(rec + 0x15u) = 100;
            kind = EF_U8(rec + 0xbu);
            v = (int32_t)EF_U8(object + 0x32u) - 1;
            if (v < 0)
                v = 0;
            if (kind == 0 || kind == 1) {
                EF_U8(object + 0x32u) = (uint8_t)v;
                table = EF_U32(object + 0x33u);
            } else {
                EF_U8(object + 0x3du) = (uint8_t)v;
                table = EF_U32(object + 0x3eu);
            }
            c = (int32_t)EF_U8(table + EF_U8(rec + 0xcu)) - 1;
            if (c < 0)
                c = 0;
            EF_U8(table + EF_U8(rec + 0xcu)) = (uint8_t)c;
        }
        rec = EF_U32(rec + 4u);
    }
}
