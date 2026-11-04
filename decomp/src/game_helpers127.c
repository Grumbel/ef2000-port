/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Text dump of record lists to a handle. */

/* 0xb2f48, 238 bytes. Dump one group record: its id, type name, extent words and, when any member byte at +5 is not 0x64, the member bytes. */
void dump_group_b2f48(ef_addr32 group, uint32_t handle)
{
    int16_t i;
    int16_t differs = 0;

    for (i = 0; i < EF_S16(group + 0x2au); i++) {
        if (EF_U8(EF_U32(group + 0x2cu) + (uint32_t)((int32_t)i * 0x15) + 5u) != 0x64)
            differs = 1;
    }
    if (differs != 0) {
        print_to_handle_b1bb0(handle, 0x0018e490u, EF_U32(group + 0xcu),
                              EF_U32(0x00132f5cu + (uint32_t)EF_U8(group + 9u) * 4u),
                              (int32_t)EF_S16(group + 0x32u), (int32_t)EF_S16(group + 0x2au),
                              (int32_t)EF_S16(group + 0x2au));
        for (i = 0; i < EF_S16(group + 0x2au); i++)
            print_to_handle_b1bb0(handle, 0x0018e4b0u,
                                  (uint32_t)EF_U8(EF_U32(group + 0x2cu) + (uint32_t)((int32_t)i * 0x15) + 5u));
        print_to_handle_b1bb0(handle, 0x0018e060u);
    } else {
        print_to_handle_b1bb0(handle, 0x0018e4b8u, EF_U32(group + 0xcu),
                              EF_U32(0x00132f5cu + (uint32_t)EF_U8(group + 9u) * 4u),
                              (int32_t)EF_S16(group + 0x32u), (int32_t)EF_S16(group + 0x2au),
                              (int32_t)EF_S16(group + 0x2au));
    }
}

/* 0xb3038, 73 bytes. Dump a group list: heading with a title, the entry count, then each group via 0xb2f48. */
void dump_group_list_b3038(ef_addr32 list_head, uint32_t handle, uint32_t title)
{
    int16_t count = (int16_t)list_count(list_head);
    ef_addr32 node;

    print_to_handle_b1bb0(handle, 0x0018e4dcu, title);
    print_to_handle_b1bb0(handle, 0x0018e4e4u, (int32_t)count);
    for (node = EF_U32(list_head); node != 0; node = EF_U32(node + 4u))
        dump_group_b2f48(node, handle);
}

/* 0xb1c34, 77 bytes. Dump a numbered entry list: the format string, the entry count, then each entry (numbered from 0 in word +0x13) via 0xb1f80. */
void dump_entry_list_b1c34(ef_addr32 list_head, uint32_t handle, ef_addr32 fmt)
{
    int16_t count = (int16_t)list_count(list_head);
    ef_addr32 node;
    uint32_t number = 0;

    print_to_handle_b1bb0(handle, fmt);
    print_to_handle_b1bb0(handle, 0x0018de5cu, (int32_t)count);
    for (node = EF_U32(list_head); node != 0; node = EF_U32(node + 4u)) {
        EF_U16(node + 0x13u) = (uint16_t)number;
        guest_fn_b1f80_x(node, handle);
        number++;
    }
}

/* 0xb2e28, 59 bytes. Dump a point list: the format string, a value, then each entry via 0xb2e98. */
void dump_point_list_b2e28(uint32_t handle, ef_addr32 list_ptr, int16_t value, ef_addr32 fmt)
{
    ef_addr32 node = EF_U32(list_ptr);

    print_to_handle_b1bb0(handle, fmt);
    print_to_handle_b1bb0(handle, 0x0018e440u, (int32_t)value);
    for (; node != 0; node = EF_U32(node + 4u))
        guest_fn_b2e98_x(node, handle);
}
