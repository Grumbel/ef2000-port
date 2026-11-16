/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xd1d40, 102 bytes. EAX width, EDX height; EBX/ECX/EDX and
 * ESI/EDI/EBP preserved, plain RET. Callers d1dde and d1e31. */
void init_video_d1d40(uint32_t width, uint32_t height)
{
    uint32_t step;
    uint32_t count;

    EF_U32(0x0015b748u) = height;
    EF_U32(0x0015b744u) = width;
    EF_U32(0x00420950u) = 0x000fedfcu;
    query_video_fc6c8(width);
    /* Reload after the video query; the original pushes width first. */
    step = EF_U32(0x0015b744u);
    count = EF_U32(0x0015b748u);
    build_row_table_fc39c(count, step);
    guest_fn_c3ae8();
    fill_table_fef26();
    apply_palette_d663c(0x00420a90u);
    free_buffers_c37ec();
    init_work_buffers_c3770();
    guest_callptr_eax_x(EF_U32(0x0015b750u), 0u);
    clear_buffer_dc2c0();
    guest_callptr_eax_x(EF_U32(0x0015b74cu), 0u);
}
