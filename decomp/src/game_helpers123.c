/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Palette log and frame slot helpers. */

/* 0xd1894, 142 bytes. Log a 256-entry palette: header text, dword 0x300, then three values per entry (a fixed 10/20/30 when mode == 2, otherwise bytes from the source, shifted left 2 when scale is non-zero). */
void log_palette_d1894(ef_addr32 source, int16_t mode)
{
    int16_t i;
    int32_t scale = (int32_t)mode;
    int32_t r, g, b;

    guest_fn_d16fc(0x0018f6ecu);
    guest_fn_d170c(0x300u);
    for (i = 0; i < 0x100; i++) {
        if (mode == 2) {
            r = 10;
            g = 20;
            b = 30;
        } else {
            r = EF_U8(source);
            g = EF_U8(source + 1u);
            b = EF_U8(source + 2u);
            source += 3u;
        }
        if ((int16_t)scale != 0) {
            r <<= 2;
            g <<= 2;
            b <<= 2;
        }
        log_value_d175c((uint32_t)(int32_t)(int16_t)r);
        log_value_d175c((uint32_t)(int32_t)(int16_t)g);
        log_value_d175c((uint32_t)(int32_t)(int16_t)b);
    }
}

/* 0xb5800, 165 bytes. Recompute the frame index of the current 25-byte slot (object+0x20 table, 1-based index at +0x1c) and then find a free slot run for it. */
void update_frame_slot_b5800(ef_addr32 owner, ef_addr32 obj, int16_t need)
{
    ef_addr32 slot;
    int32_t index;

    index = EF_S16(owner + 0x1cu) - 1;
    slot = EF_U32(owner + 0x20u) + (uint32_t)(index * 25);
    {
        int32_t frame = frame_index_b56d0(EF_S16(slot + 0x10u), EF_S16(slot + 0x12u), obj);
        index = EF_S16(owner + 0x1cu) - 1;
        EF_U16(EF_U32(owner + 0x20u) + (uint32_t)(index * 25) + 0xeu) = (uint16_t)frame;
    }
    {
        int32_t start = EF_S16(EF_U32(owner + 0x20u) + (uint32_t)((EF_S16(owner + 0x1cu) - 1) * 25) + 0xeu);
        int32_t result = find_free_slot_run_b58a8(obj, (int16_t)start, need);
        index = EF_S16(owner + 0x1cu) - 1;
        EF_U16(EF_U32(owner + 0x20u) + (uint32_t)(index * 25) + 0xeu) = (uint16_t)result;
    }
}
