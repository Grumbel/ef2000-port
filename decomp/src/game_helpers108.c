/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Hit testing against 0x4a-byte entries and list flattening. */

/* 0xcfa24, 103 bytes. Return the first of count 0x4a-byte entries whose bounding box contains (x,y) and which passes the cf960 test, else 0. */
ef_addr32 find_entry_at_point_cfa24(uint32_t count, ef_addr32 base, int32_t x, int32_t y)
{
    ef_addr32 e = base;

    if (count == 0)
        return 0;
    do {
        if (x >= (EF_S32(e + 0x10u) >> 16) && x <= (EF_S32(e + 0x12u) >> 16)
            && y >= (EF_S32(e + 0x14u) >> 16) && y <= (EF_S32(e + 0x16u) >> 16)
            && guest_fn_cf960_x(e + 0x1eu, x, y) != 0)
            return e;
        e += 0x4au;
        count--;
    } while (count != 0);
    return 0;
}

/* 0xcbf20, 126 bytes. Concatenate the data blocks (length at +9, bytes at *(+0xc)) of a record list into one new buffer; the first block's length is stored at out and defines the buffer size (length squared). */
ef_addr32 flatten_record_blocks_cbf20(ef_addr32 out, ef_addr32 list)
{
    ef_addr32 rec = EF_U32(list);
    int16_t size = (int16_t)EF_U16(rec + 9u);
    ef_addr32 buffer;
    int16_t offset = 0;

    EF_U16(out) = 0;
    EF_U16(out) = (uint16_t)size;
    buffer = guest_fn_cc6c0_z((uint32_t)((int32_t)size * (int32_t)size), 0x0018f354u, (uint32_t)(int32_t)size);
    while (rec != 0) {
        int16_t i = 0;

        if ((int16_t)EF_U16(rec + 9u) > 0) {
            do {
                EF_U8(buffer + (uint32_t)(int32_t)offset) = EF_U8(EF_U32(rec + 0xcu) + (uint32_t)(int32_t)i);
                i++;
                offset++;
            } while (i < (int16_t)EF_U16(rec + 9u));
        }
        rec = EF_U32(rec + 4u);
    }
    return buffer;
}
