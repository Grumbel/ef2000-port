/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* File size, timed value ramp and MIDI channel volume sender. */

/* 0x115989, 61 bytes. Size of an open file: remember the position, seek to the end, seek back; -1 when the position query fails. */
uint32_t file_size_115989(uint32_t handle)
{
    uint32_t pos = guest_fn_fa95f(handle, 0, 1);
    uint32_t size;

    if (pos == 0xffffffffu)
        return pos;
    size = guest_fn_fa95f(handle, 0, 2);
    guest_fn_fa95f(handle, pos, 0);
    return size;
}

/* 0x1141b0, 95 bytes. Start a ramp of the value at +0x48 to a target over time; sets the per-step delta at +0x54. */
void start_value_ramp_1141b0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 obj, int32_t target, int32_t time)
{
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (obj == 0)
        return;
    EF_U32(EF_U32(obj) + 0x14u) += 1u;
    EF_S32(obj + 0x4cu) = target;
    if (EF_S32(obj + 0x48u) != target) {
        if (time == 0) {
            EF_S32(obj + 0x48u) = target;
        } else {
            int32_t span = (int32_t)abs_a28f2(EF_S32(obj + 0x48u) - target);
            EF_S32(obj + 0x50u) = 0;
            EF_S32(obj + 0x54u) = (time * 1000) / span;
        }
    }
    EF_U32(EF_U32(obj) + 0x14u) -= 1u;
}

/* 0x112bd8, 54 bytes. Send controller 7 (volume) for each of the 16 channels whose stored value is not -1. */
void send_channel_volumes_112bd8(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 player)
{
    uint32_t ch;

    (void)u0; (void)u1; (void)u2; (void)u3;
    for (ch = 0; ch < 16; ch++) {
        uint32_t v = EF_U32(player + 0x394u + ch * 4u);
        if (v != 0xffffffffu)
            guest_fn_1123a4_x(player, ch | 0xb0u, 7, v, 0);
    }
}
