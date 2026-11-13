/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#include "ef_abi.h"
#include "ef_mem.h"

static void report_name_error(ef_addr32 message)
{
#ifdef __WATCOMC__
    typedef void (__cdecl *Reporter)(ef_addr32);
#else
    typedef void (*Reporter)(ef_addr32);
#endif
    ((Reporter)(uintptr_t)EF_U32(0x132a74u))(message);
}

/* 0xaa3dc, 176 bytes. Walk the linked list at 0x4191fc for as many entries as the signed word at 0x4194c4 says: copy each entry's name (+9) into the 21-byte slots at 0x419218, record its flag byte (+8) at 0x41948e+index, and for flagged entries allocate a 21-byte block (stored at 0x418708 + 4 * index) preset with the 5 bytes from 0x18d8e4, for unflagged ones store 0. Reports errors through 0x132a74 when the list ends early or an allocation fails. */
void load_list_names_aa3dc(void)
{
    uint32_t slot = 0x00419218u;
    uint32_t node = EF_U32(0x004191fcu);
    uint32_t idx = 0u;
    while ((int32_t)(int16_t)EF_U16(0x004194c4u) > (int32_t)idx) {
        uint32_t cell = 0x00418708u + idx * 4u;
        /* ORIGINAL BUG: a returning diagnostic does not stop null-node use. */
        if (node == 0u)
            report_name_error(0x0018d8a8u);
        copy_string_9d749(slot, node + 9u);
        if (EF_U8(node + 8u) == 0u) {
            EF_U32(cell) = 0u;
        } else {
            EF_U32(cell) = guest_fn_c3f70(0x15u);
            /* ORIGINAL BUG: a returning diagnostic leaves the null allocation. */
            if (EF_U32(cell) == 0u)
                report_name_error(0x0018d8c0u);
            EF_U32(EF_U32(cell)) = EF_U32(0x0018d8e4u);
            EF_U8(EF_U32(cell) + 4u) = EF_U8(0x0018d8e8u);
        }
        idx++;
        slot += 0x15u;
        EF_U8(0x0041948du + idx) = EF_U8(node + 8u);
        node = EF_U32(node);
    }
}
