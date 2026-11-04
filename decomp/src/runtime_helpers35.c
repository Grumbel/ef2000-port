/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Swapped dword log write, buffer holder init and channel refresh. */

/* 0xd170c, 80 bytes. Reverse the byte order of a dword and pass it to d16fc. */
void write_log_dword_swapped_d170c(uint32_t value)
{
    uint32_t swapped = (value >> 24) | ((value >> 8) & 0xff00u) | ((value << 8) & 0xff0000u) | (value << 24);

    guest_fn_d16fc((uint32_t)(uintptr_t)&swapped);
}

/* 0x117ed4, 62 bytes. Attach an object to a holder, allocating the holder's block when missing, and reset the object's header words (last word set to the size); returns 1 on success. */
uint32_t attach_buffer_117ed4(ef_addr32 obj, ef_addr32 holder, uint16_t size)
{
    EF_U32(holder) = obj;
    if (EF_U32(holder + 4u) == 0) {
        EF_U32(holder + 4u) = guest_fn_e8733(1, size);
        if (EF_U32(holder + 4u) == 0)
            return 0;
    }
    EF_U16(obj) = 0;
    EF_U16(obj + 2u) = 0;
    EF_U16(obj + 4u) = 0;
    EF_U16(obj + 6u) = size;
    return 1;
}

/* 0x1142cc, 75 bytes. When the value at the argument changes, resend the channel volumes of every active (state 4) channel of the player. */
void refresh_player_channels_1142cc(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 player, uint32_t value)
{
    uint32_t count;
    ef_addr32 ch;

    (void)u0; (void)u1; (void)u2; (void)u3;
    if (value == EF_U32(player + 0x1b0u))
        return;
    count = EF_U32(player + 0x1cu);
    EF_U32(player + 0x1b0u) = value;
    EF_U32(player + 0x14u) = EF_U32(player + 0x14u) + 1u;
    ch = EF_U32(player + 0x18u);
    while (count != 0) {
        if (EF_U32(ch + 4u) == 4)
            send_channel_volumes_112bd8(0, 0, 0, 0, ch);
        count--;
        ch += 0x718u;
    }
    EF_U32(player + 0x14u) = EF_U32(player + 0x14u) - 1u;
}
