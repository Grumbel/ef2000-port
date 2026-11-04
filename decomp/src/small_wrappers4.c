/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Small record and display helpers. */

/* 0x11288c, 88 bytes. Release every slot of a record whose id (+0x558) matches, then send the pending-count message. */
void release_matching_slots_11288c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec, uint32_t id)
{
    uint32_t slot;
    (void)u0; (void)u1; (void)u2; (void)u3;
    for (slot = rec; slot != rec + 0x80u; slot += 4u) {
        if (id == EF_U32(slot + 0x558u)) {
            guest_fn_1123a4_x(rec, EF_U32(slot + 0x558u) | 0x80u, EF_U32(slot + 0x5d8u), 0u, 0u);
            EF_U32(slot + 0x558u) = 0xffffffffu;
        }
    }
    send_pending_count_message_111df0(0u, 0u, 0u, 0u, EF_U32(rec));
}

/* 0x77c34, 89 bytes. Shrink the pending line count at 0x1f8d52 to the limit at 0x127d92 and redraw the uppercased message line. */
void redraw_message_line_77c34(void)
{
    int32_t limit, count;
    if (EF_S16(0x001f8d52u) <= 0)
        return;
    limit = (int32_t)EF_U16(0x00127d92u);
    count = EF_S32(0x001f8d50u) >> 16;
    if (limit >= count)
        limit = count;
    EF_U16(0x001f8d52u) = (uint16_t)(count - limit);
    uppercase_string(0x0012ad94u);
    EF_U8(0x00127b1au) = 1u;
    guest_fn_48c90(0xfu, 0xau, 0xau, 0x0012ad94u);
}

/* 0xeb0b0, 89 bytes. Copy rows of bytes into the framebuffer at (x, y), advancing the destination by the row stride at 0x1631f4. */
void copy_rows_to_screen_eb0b0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t x, uint32_t y, uint32_t width, uint32_t rows, uint32_t src_stride, ef_addr32 src)
{
    uint32_t dst = EF_U32(0x0015b754u) + EF_U32(0x00163200u + (y & 0xffffu) * 4u) + (x & 0xffffu);
    uint32_t n = rows;
    (void)u0; (void)u1; (void)u2; (void)u3;
    if ((uint16_t)n == 0u)
        return;
    do {
        n--;
        guest_fn_e9a24(width & 0xffffu, src, dst);
        dst += EF_U32(0x001631f4u);
        src += src_stride & 0xffffu;
    } while ((uint16_t)n > 0u);
}
