/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Record dump and group state helpers. */

/* 0xb23bc, 226 bytes. Dump one 0x13-byte record to a handle: an index or fallback value, its kind name, an optional dword and six signed words. */
void dump_record_b23bc(ef_addr32 rec, int16_t index, uint32_t handle, int16_t fallback)
{
    uint32_t kind;
    uint32_t extra;

    if (index != -1)
        print_to_handle_b1bb0(handle, 0x0018e0ccu, EF_U32(0x00132bbcu + (uint32_t)(int32_t)index * 4u));
    else
        print_to_handle_b1bb0(handle, 0x0018e0d0u, (int32_t)fallback);
    kind = EF_U8(rec);
    if (kind == 0xfeu)
        print_to_handle_b1bb0(handle, 0x0018e0d8u);
    else if (kind == 0xffu)
        print_to_handle_b1bb0(handle, 0x0018e0f4u);
    else
        print_to_handle_b1bb0(handle, 0x0018e108u, EF_U32(0x00132fb4u + kind * 4u));
    print_to_handle_b1bb0(handle, 0x0018e114u);
    extra = EF_U32(rec + 1u);
    if (extra != 0)
        print_to_handle_b1bb0(handle, 0x0018dee0u, extra);
    print_to_handle_b1bb0(handle, 0x0018e118u);
    print_to_handle_b1bb0(handle, 0x0018e11cu, (int32_t)EF_S16(rec + 7u), (int32_t)EF_S16(rec + 9u),
                          (int32_t)EF_S16(rec + 0xbu), (int32_t)EF_S16(rec + 0xdu),
                          (int32_t)EF_S16(rec + 0xfu), (int32_t)EF_S16(rec + 0x11u));
}

/* 0xb059c, 236 bytes. Classify every entry of the list: entries of kind 0x31 whose type is 2 get state 2 if some other qualifying entry matches them, otherwise state 1; all others take the type byte of their group node as state (word +0x4e). */
void classify_entries_b059c(ef_addr32 list_holder, ef_addr32 head_a, ef_addr32 list_b)
{
    ef_addr32 node = EF_U32(list_holder);

    while (node != 0) {
        ef_addr32 group = find_node_by_byte_b(EF_U8(node + 0xbu), head_a);
        if (EF_U8(group + 0x2eu) == 2 && EF_U8(node + 0xcu) == 0x31) {
            ef_addr32 other = EF_U32(list_holder);
            int16_t free_state = 1;
            while (other != 0 && free_state != 0) {
                if ((uint16_t)group_flag_negative_cc1ac(EF_U8(node + 0xbu), EF_U8(other + 0xbu), head_a) != 0
                    && EF_U8(other + 0xcu) == 0x31
                    && (uint16_t)guest_fn_b04e8_x(node, other) != 0
                    && (uint16_t)list_has_match_b0554(node, other, list_b) != 0) {
                    EF_U16(node + 0x4eu) = 2;
                    free_state = 0;
                }
                other = EF_U32(other + 4u);
            }
            if (free_state != 0)
                EF_U16(node + 0x4eu) = 1;
        } else {
            EF_U16(node + 0x4eu) = EF_U8(group + 0x2eu);
        }
        node = EF_U32(node + 4u);
    }
}
