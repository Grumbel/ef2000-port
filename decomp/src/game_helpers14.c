/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Sorting, record reading and text measuring helpers, 0x946c..0x159a0. */

/* 0x946c, 142 bytes. Bubble-sort the (key, value) word pairs at 0x197370 by key. */
void sort_pairs_946c(void)
{
    int16_t count = (int16_t)EF_U16(0x0019769au);
    int32_t pass;

    if (count > 0) {
        for (pass = 0; (int16_t)count > (int16_t)pass; pass++) {
            int16_t j;

            for (j = 0; (int32_t)j < (int32_t)count - 1; j++) {
                ef_addr32 p = 0x00197370u + (uint32_t)((int32_t)j * 4);
                int16_t a = (int16_t)EF_U16(p);
                int16_t b = (int16_t)EF_U16(p + 4u);

                if (a > b) {
                    uint16_t value_a = EF_U16(p + 2u);
                    uint16_t value_b = EF_U16(p + 6u);

                    EF_U16(p + 4u) = (uint16_t)a;
                    EF_U16(p) = (uint16_t)b;
                    EF_U16(p + 6u) = value_a;
                    EF_U16(p + 2u) = value_b;
                }
            }
        }
    }
    EF_U16(0x0019769au) = (uint16_t)count;
}

/* 0x9e38, 103 bytes. Find the 25-byte entry of kind 8 (the last one) and copy its fields out. */
uint8_t find_kind8_entry_9e38(ef_addr32 object, ef_addr32 out_a, ef_addr32 out_b, ef_addr32 out_c)
{
    uint8_t found = 0;
    int16_t i;

    for (i = 0; i < (int16_t)EF_U16(EF_U32(object + 0x13u) + 0x1cu); i++) {
        ef_addr32 base = EF_U32(EF_U32(object + 0x13u) + 0x20u);
        ef_addr32 entry = base + (uint32_t)((int32_t)i * 0x19);

        if (EF_U8(entry + 0x18u) == 8) {
            EF_U16(out_a) = EF_U16(entry + 0x10u);
            EF_U16(out_b) = EF_U16(EF_U32(EF_U32(object + 0x13u) + 0x20u) + (uint32_t)((int32_t)i * 0x19) + 0x12u);
            EF_U16(out_c) = 0;
            found = 1;
        }
    }
    return found;
}

/* 0xed80, 137 bytes. Read one record from the input stream into the 0x1c-byte record at dest. */
uint32_t read_record_ed80(ef_addr32 dest)
{
    guest_fn_e810();
    guest_fn_e89c();
    guest_fn_e89c();
    EF_U32(dest) = guest_fn_e980();
    EF_U32(dest + 4u) = guest_fn_e980();
    EF_U16(dest + 0xcu) = (uint16_t)guest_fn_e980();
    EF_U16(dest + 8u) = (uint16_t)guest_fn_e95c();
    EF_U16(dest + 0xau) = (uint16_t)guest_fn_e95c();
    EF_U8(dest + 0x18u) = (uint8_t)guest_fn_ed30();
    EF_U16(dest + 0x10u) = (uint16_t)guest_fn_e95c();
    EF_U16(dest + 0x12u) = (uint16_t)guest_fn_e95c();
    EF_U16(dest + 0x14u) = (uint16_t)guest_fn_e95c();
    if ((int16_t)EF_U16(dest + 0xcu) > 0x3e8)
        EF_U16(dest + 0xcu) = (uint16_t)((int16_t)EF_U16(dest + 0xcu) / 100);
    return 1;
}

/* 0x159a0, 115 bytes. Pixel width of a string: 3 for a space, glyph width + 1 otherwise. */
uint32_t text_width_159a0(ef_addr32 text)
{
    uint32_t width = 0;
    int16_t i;

    for (i = 0; ; i++) {
        uint8_t c = EF_U8(text + (uint32_t)(int32_t)i);
        int32_t glyph;

        if (c == 0)
            break;
        if (c == 0x20) {
            width += 3;
            continue;
        }
        if (c & 0x80u)
            glyph = (int32_t)EF_U32(0x00122b05u + (uint32_t)(c & 0x1fu)) >> 24;
        else
            glyph = (int32_t)EF_U32(0x00122ac5u + (uint32_t)(c & 0x3fu)) >> 24;
        if (glyph != -1)
            width += (uint32_t)EF_U8(0x00122b28u + (uint32_t)glyph) + 1u;
    }
    return width;
}
