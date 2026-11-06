/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Row clearing and the 15-slot sound channel table. */

/* 0x110b70, 51 bytes. Zero len bytes at the start of each of count rows spaced stride bytes apart. */
void clear_rows_110b70(ef_addr32 dest, int16_t stride, int16_t len, int16_t count)
{
    if (count <= 0)
        return;
    do {
        fill_bytes_b3fb0(dest, 0, (uint32_t)(int32_t)len);
        count--;
        dest += (uint32_t)(int32_t)stride;
    } while (count > 0);
}

/* 0x111365, 71 bytes. Claim a free slot of the 15-entry table at 0x17d664, store the argument in 0x17d624 and return the slot offset (-1 when full). */
uint32_t EF_CDECL claim_table_slot_111365(uint32_t value)
{
    uint32_t slot = 0;

    guest_fn_101e70();
    for (;;) {
        if (EF_U32(0x0017d664u + slot) == 0) {
            EF_U32(0x0017d664u + slot) = 1;
            EF_U32(0x0017d624u + slot) = value;
            break;
        }
        slot += 4;
        if (slot >= 0x3c) {
            slot = 0xffffffffu;
            break;
        }
    }
    guest_fn_101e78();
    return slot;
}

/* 0x11141f, 48 bytes. Promote a table slot from state 1 to state 2. */
void EF_CDECL table_slot_1_to_2_11141f(uint32_t slot)
{
    guest_fn_101e70();
    if (slot != 0xffffffffu && EF_U32(0x0017d664u + slot) == 1)
        EF_U32(0x0017d664u + slot) = 2;
    guest_fn_101e78();
}

/* 0x111473, 48 bytes. Demote a table slot from state 2 to state 1. */
void EF_CDECL table_slot_2_to_1_111473(uint32_t slot)
{
    guest_fn_101e70();
    if (slot != 0xffffffffu && EF_U32(0x0017d664u + slot) == 2)
        EF_U32(0x0017d664u + slot) = 1;
    guest_fn_101e78();
}

/* 0x1114c7, 48 bytes. Set a slot's field at 0x17d6e4, clear its field at 0x17d6a4 and run 110f6f. */
void EF_CDECL set_table_slot_value_1114c7(uint32_t slot, uint32_t value)
{
    guest_fn_101e70();
    EF_U32(0x0017d6e4u + slot) = value;
    EF_U32(0x0017d6a4u + slot) = 0;
    guest_fn_110f6f_x();
    guest_fn_101e78();
}

/* 0x111f20, 45 bytes. Read a MIDI variable-length quantity (up to 4 bytes) and advance the cursor stored at the argument. */
uint32_t read_varlen_111f20(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 cursor)
{
    uint32_t value = 0;
    int32_t left = 4;

    (void)u0; (void)u1; (void)u2; (void)u3;
    do {
        uint32_t b = EF_U8(EF_U32(cursor));
        EF_U32(cursor) = EF_U32(cursor) + 1u;
        value = (value << 7) | (b & 0x7fu);
        if ((b & 0x80u) == 0)
            break;
        left--;
    } while (left != 0);
    return value;
}
