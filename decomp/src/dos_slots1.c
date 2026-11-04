/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* DOS interrupt hook slots. */

/* 0x7b4, 104 bytes. Release hook slot idx (14-byte records at 0x1960c4): restore its vector, clear the pointer and push idx on the free list at 0x196133. */
void release_hook_slot_7b4(uint32_t idx)
{
    uint32_t rec = 0x001960c4u + idx * 14u;
    uint32_t count;
    if (EF_U32(rec) == 0u)
        return;
    dos_call_2505_3d0(EF_U32(rec), EF_U32(rec + 4u));
    guest_fn_3f8_x(EF_U32(rec), 0u, EF_U32(rec + 8u), EF_U16(rec + 0xcu));
    count = EF_U32(0x00196140u) + 1u;
    EF_U32(rec) = 0u;
    EF_U8(0x00196133u + count) = (uint8_t)idx;
    EF_U32(0x00196140u) = count;
}

/* 0x106dc4, 103 bytes. Wait on a ring record until its data arrives (0), the end marker 0x4f (0) or 0x45 (8) is popped, or the timeout expires (3). */
uint32_t wait_ring_data_106dc4(ef_addr32 rec, uint32_t timeout)
{
    uint32_t start = tail_jump_106be0(rec);
    for (;;) {
        if (entry_empty_118520(EF_U32(rec)) == 0u) {
            int32_t key = ring_pop_1184c4(EF_U32(rec));
            if (EF_U32(0x0017d364u) != 0u)
                stdout_printf_cd12e(0x00194a80u, key);
            if (key == 0x4f)
                return 0u;
            if (key == 0x45)
                return 8u;
        }
        if (elapsed_at_least((int32_t)start, (int32_t)timeout) != 0u)
            return 3u;
    }
}
