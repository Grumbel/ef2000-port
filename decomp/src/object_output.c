/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xb2e98,176 bytes. EAX record, EDX output; EAX/EDX scratch,
 * five other registers preserved, RET. Sole caller b2e55. */
void print_object_b2e98(ef_addr32 rec, uint32_t out)
{
    uint32_t flag = EF_U8(rec + 0x2fu);
    uint32_t high = EF_U32(rec + 0x2bu);
    uint32_t low = EF_U32(rec + 0x27u);
    int32_t second = EF_S32(rec + 0x21u) >> 16;
    int32_t first = EF_S32(rec + 7u) >> 16;
    uint32_t type = EF_U8(rec + 0x0cu);
    ef_addr32 name = EF_U32(rec + 0x0du);
    ef_addr32 type_name = EF_U32(0x00132fb4u + type * 4u);
    ef_addr32 category = EF_U32(0x00132f5cu + (uint32_t)EF_U8(rec + 0x0bu) * 4u);
    uint32_t index = 0u;
    ef_addr32 item;
    int32_t active;

    /* Two raw dwords occupy the original double's cdecl argument slot. */
    print_to_handle_b1bb0(out, 0x0018e454u, category, type_name, name,
        first, second, low, high, flag);
    print_coordinate_record_b2a2c(rec + 0x11u, out);
    active = (int16_t)count_active_entries(rec);
    print_to_handle_b1bb0(out, 0x0018e478u, active);
    if (EF_S16(rec + 0x48u) > 0) {
        do {
            item = EF_U32(rec + 0x4au)
                + (uint32_t)(int32_t)(int16_t)index * 9u;
            if (EF_U8(item + 3u) != 0u)
                print_point_b2d8c(item, out);
            index++;
        } while ((int16_t)index < EF_S16(rec + 0x48u));
    }
    print_to_handle_b1bb0(out, 0x0018e488u);
}
