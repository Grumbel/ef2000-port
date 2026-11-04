/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Plane interleave and file loading. */

/* 0xcdc90, 134 bytes. Interleave four planes of rows x width bytes from a packed source into every fourth byte of the destination (non-zero source bytes only), stepping destination rows by the stride at 0x1631f4. */
void interleave_planes_cdc90(ef_addr32 dst, ef_addr32 src)
{
    uint32_t width = EF_U32(0x004201a0u);
    uint32_t plane, row, x;
    for (plane = 0; (int32_t)plane < 4; plane++) {
        uint32_t line = dst + plane;
        for (row = 0; (int32_t)row < (int32_t)EF_U32(0x004201a4u); row++) {
            uint32_t d = line;
            for (x = 0; (int32_t)x < (int32_t)width; x++) {
                uint8_t c = EF_U8(src + x);
                if (c != 0u)
                    EF_U8(d) = c;
                d += 4u;
            }
            src += width;
            line += EF_U32(0x001631f4u);
        }
    }
    EF_U32(0x004201a0u) = width;
}

/* 0xe94ec, 134 bytes. Load a text file through the size/read handlers at 0x132ab4/0x132ab8 into a new buffer at 0x1600a0, trimming a trailing ^Z; returns 0 ok, 1 empty, 2 out of memory. */
uint32_t load_text_file_e94ec(ef_addr32 name, uint32_t flag)
{
    uint32_t n, buf, end, last;
    EF_U32(0x00464390u) = name;
    EF_U8(0x00464439u) = (uint8_t)flag;
    if (EF_U8(0x001600a4u) == 1u)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x0019135cu);
    n = (uint16_t)guest_callptr_eax_ret_x(EF_U32(0x00132ab4u), name);
    if (n == 0u)
        return 1u;
    n++;
    buf = arena_alloc_e2a94(n + 0x20u);
    EF_U32(0x001600a0u) = buf;
    if (buf == 0u)
        return 2u;
    guest_callptr_eaxedx_x(EF_U32(0x00132ab8u), name, buf);
    end = EF_U32(0x001600a0u) + n;
    last = EF_U8(end - 2u);
    EF_U8(end - 1u) = 0u;
    if (last == 0x1au)
        EF_U8(end - 2u) = 0u;
    EF_U8(0x001600a4u) = 1u;
    return 0u;
}
