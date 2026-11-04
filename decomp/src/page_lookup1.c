/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Named page entry lookups. */

/* Look a name up and, through the page of the object at obj_global, fetch its five-word entry; then call target with it. A missing name or entry is reported through 0x132a74 and the words are left at zero. */
static void lookup_entry_and_call(ef_addr32 name, uint32_t obj_global, uint32_t page_off, uint32_t miss_fmt,
                                  void (*target)(int32_t, int32_t, int32_t, int32_t, int32_t))
{
    int16_t w0 = 0, w1 = 0, w2 = 0, w3 = 0, w4 = 0;
    uint32_t key = lookup_name_value_a1a40(name);
    if (key == 0u) {
        guest_callptr2_x(EF_U32(0x00132a74u), miss_fmt, name);
    } else {
        uint32_t obj = EF_U32(obj_global);
        uint32_t entry = 0u;
        if (EF_U16(obj + page_off) != 0u && EF_U16(obj + page_off) != 0xffffu)
            entry = find_page_pair_a1a90((int16_t)EF_S16(obj + page_off), (int32_t)key);
        if (entry != 0u) {
            w0 = (int16_t)EF_U16(entry);
            w1 = (int16_t)EF_U16(entry + 2u);
            w2 = (int16_t)EF_U16(entry + 4u);
            w3 = (int16_t)EF_U16(entry + 6u);
            w4 = (int16_t)EF_U16(entry + 8u);
        } else {
            guest_callptr1_x(EF_U32(0x00132a74u), 0x0018ab60u);
        }
    }
    target(w0, w1, w2, w3, w4);
}

/* 0x95660, 161 bytes. Look up a named entry for the object at 0x214458 and pass it to 0x955c0. */
void lookup_entry_95660(ef_addr32 name)
{
    lookup_entry_and_call(name, 0x00214458u, 0x20u, 0x0018ab78u, guest_fn_955c0_x);
}

/* 0x957c8, 161 bytes. Same for the object at 0x20f15c (page word at +0x2e), passing the entry to 0x95704. */
void lookup_entry_957c8(ef_addr32 name)
{
    lookup_entry_and_call(name, 0x0020f15cu, 0x2eu, 0x0018aba4u, guest_fn_95704_x);
}
