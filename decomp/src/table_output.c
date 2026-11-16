/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* The original variants differ only in the first name dictionary. */
static void dump_named_table(ef_addr32 rec, uint32_t out, ef_addr32 names)
{
    ef_addr32 third = EF_U32(0x0013336cu + (uint32_t)EF_U8(rec + 3u) * 4u);
    ef_addr32 second = EF_U32(0x00133304u + (uint32_t)EF_U8(rec + 2u) * 4u);
    ef_addr32 first = EF_U32(names + (uint32_t)EF_U8(rec) * 4u);
    uint32_t identifier = EF_U8(rec + 1u);
    int32_t last, middle, initial;
    uint32_t index = 0u;
    int16_t current;
    ef_addr32 item;

    print_to_handle_b1bb0(out, 0x0018e260u, identifier, first, second, third);
    print_coordinate_record_b2a2c(rec + 8u, out);
    last = EF_S32(rec + 0x1cu) >> 16;
    middle = EF_S32(rec + 0x18u) >> 16;
    initial = EF_S32(rec + 0x1au) >> 16;
    print_to_handle_b1bb0(out, 0x0018e1acu, initial, middle, last);
    if (EF_S16(rec + 0x1cu) > 0) {
        do {
            current = (int16_t)index;
            item = EF_U32(rec + 0x20u) + (uint32_t)(int32_t)current * 25u;
            index++;
            print_move_record_b2504(item, current, out);
        } while ((int16_t)index < EF_S16(rec + 0x1cu));
    }
    print_to_handle_b1bb0(out, 0x0018e1c4u);
}

/* 0xb2810,166 bytes. EAX record, EDX output; EAX/EDX scratch,
 * EBX/ECX/ESI/EDI/EBP preserved, RET. Sole caller b2cb9. */
void dump_table_a_b2810(ef_addr32 rec, uint32_t out)
{
    dump_named_table(rec, out, 0x00133488u);
}

/* 0xb28b8,166 bytes. Same contract; sole caller b2cd1. */
void dump_table_b_b28b8(ef_addr32 rec, uint32_t out)
{
    dump_named_table(rec, out, 0x001333f8u);
}
