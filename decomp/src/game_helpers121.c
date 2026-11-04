/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Record shutdown and handle seek helpers. */

/* 0x1068b8, 70 bytes. Deactivate a record that is not yet state 1 or 2: set state 2, run the callbacks at 0x84c and 0x850 (cdecl, record on the stack). */
void deactivate_record_1068b8(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec)
{
    uint32_t state;
    ef_addr32 callback;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (rec == 0)
        return;
    state = EF_U32(rec + 4u);
    if (state == 1u || state == 2u)
        return;
    callback = EF_U32(rec + 0x84cu);
    EF_U32(rec + 4u) = 2;
    if (callback != 0)
        guest_callptr1_x(EF_U32(rec + 0x84cu), rec);
    if (EF_U32(rec + 0x850u) != 0)
        guest_callptr1_x(EF_U32(rec + 0x850u), rec);
}

/* 0xeb698, 71 bytes. Seek the context's handle to the given position; on a position change first reread the 8-byte header into context+0x148 and remember the position at +0x150. */
void seek_record_eb698(ef_addr32 pos, ef_addr32 ctx)
{
    if (pos != EF_U32(ctx + 0x150u)) {
        seek_start_a7ab4(0, 0, 0, 0, EF_U32(ctx), pos);
        EF_U16(ctx + 0x148u) = 0;
        read_handle_a79d4(0, 0, 0, 0, EF_U32(ctx), 8, ctx + 0x148u);
        EF_U32(ctx + 0x150u) = pos;
    }
    seek_start_a7ab4(0, 0, 0, 0, EF_U32(ctx), pos);
}

/* 0xa3424, 73 bytes. Copy the 0x21-byte name of table entry (index << 5) at 0x13587c into the buffer of the slot it names and flag that slot 1|2. */
void copy_name_and_flag_a3424(uint16_t index)
{
    ef_addr32 entry = EF_U32(0x0013587cu + ((uint32_t)index << 5));
    uint32_t slot = EF_U16(entry);
    guest_fn_e9a24(0x21, entry + 2u, EF_U32(0x001368bcu + slot * 4u));
    EF_U8(EF_U32(0x001368bcu + slot * 4u) + 0x1cu) |= 1;
    EF_U8(EF_U32(0x001368bcu + slot * 4u) + 0x1cu) |= 2;
}
