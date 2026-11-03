/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Display-list, reset and video-mode helpers, 0xd17d0..0xfc600. */

/* 0xd17d0, 144 bytes. Emit the fixed command sequence of the 0x18f6e4 display list. */
void emit_display_list_d17d0(void)
{
    call_d16d0_d16fc(0x0018f6e4u);
    guest_fn_d170c(0x14);
    guest_fn_d1860(EF_S16(0x0015b744u));
    guest_fn_d1860(EF_S16(0x0015b748u));
    guest_fn_d1860(0);
    guest_fn_d1860(0);
    guest_fn_d175c(8);
    guest_fn_d175c(0);
    guest_fn_d175c(1);
    guest_fn_d175c(0);
    guest_fn_d1860(0);
    guest_fn_d175c(5);
    guest_fn_d175c(6);
    guest_fn_d1860(EF_S16(0x0015b744u));
    guest_fn_d1860(EF_S16(0x0015b748u));
}

/* 0xe4264, 116 bytes. Reset the engine state unless already in state 0 or 2. */
void reset_engine_e4264(void)
{
    uint8_t state = EF_U8(0x0015f930u);

    if (state == 0 || state == 2)
        return;
    if (EF_U32(0x001590a4u) != 0)
        EF_U32(0x001590a4u) = 0;
    EF_U32(0x0015f90cu) = 0;
    reset_parser_state_9d938();
    reset_buffer_e2a1c();
    update_ring_b3d74();
    guest_fn_cd7a0();
    guest_fn_b3fb0(0x00431ef0u, 0xff, 0xc8);
    EF_U16(0x0015eea0u) = 0;
    EF_U32(0x0015f938u) = 0;
    EF_U8(0x0015f930u) = 2;
}

/* 0xf8010, 132 bytes. Merge the two 2-bit fields of the mask at 0x46438c into the object flag bytes. */
void merge_flag_fields_f8010(void)
{
    uint8_t low;
    uint8_t high;
    ef_addr32 object;
    uint8_t value;

    if (EF_U8(0x0041f2e2u) != 2u)
        guest_fn_f6124();
    object = EF_U32(0x0041ecccu);
    low = (uint8_t)((EF_U8(0x0046438cu) & 3u) << 4);
    EF_U8(object + 0x1cu) = (uint8_t)((EF_U8(object + 0x1cu) & 0xcfu) | low);
    high = (uint8_t)(((EF_U32(0x0046438cu) & 0xcu) >> 2) & 3u);
    high = (uint8_t)(high << 6);
    EF_U8(object + 0x1cu) = (uint8_t)((EF_U8(object + 0x1cu) & 0x3fu) | high);
    value = (uint8_t)((EF_U8(0x0041ec44u) & 0xcfu) | low);
    EF_U8(0x0041ec44u) = (uint8_t)((value & 0x3fu) | high);
}

/* 0xfc600, 155 bytes. Set a VESA mode, with the linear-framebuffer bit first if supported. */
uint32_t set_video_mode_fc600(uint32_t mode)
{
    if ((query_video_mode_fc478(mode) & 0xffu) == 0)
        return 0;
    if ((EF_U8(EF_U32(0x00164610u)) & 0x80u) != 0) {
        EF_U32(0x00164634u) = 0x4f02u;
        EF_U32(0x00164628u) = mode | 0x4000u;
        guest_fn_8b8(0x10, 0x00164618u);
        if ((EF_U32(0x00164634u) & 0xffffu) == 0x4fu)
            return 1;
    }
    EF_U32(0x00164628u) = mode;
    EF_U32(0x00164634u) = 0x4f02u;
    guest_fn_8b8(0x10, 0x00164618u);
    EF_U8(EF_U32(0x00164610u)) &= 0x7fu;
    return (EF_U32(0x00164634u) & 0xffffu) == 0x4fu;
}
