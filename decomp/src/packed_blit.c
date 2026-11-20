/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define RASTER_ROW_TABLE 0x00163200u

/* Copy up to four bytes of WORD to DST, leaving the zero bytes out. */
static void put_word_bytes(ef_addr32 dst, uint32_t word, int32_t count)
{
    int32_t i;

    for (i = 0; i < count; ++i) {
        uint8_t value = (uint8_t)(word >> (i * 8));

        if (value != 0u)
            EF_U8(dst + (uint32_t)i) = value;
    }
}

/* One row of WIDTH source bytes at SRC to the destination at SRC - DELTA.
 * Zero bytes are transparent. Runs of zero dwords are skipped a dword at
 * a time; after a non-zero dword the bytes are copied one by one until
 * the next zero byte, which returns to dword skipping at that byte. The
 * byte loop tests only the low word of the count, like the original
 * jcxz. */
static void blit_row(ef_addr32 src, uint32_t delta, int32_t count)
{
    for (;;) {
        uint32_t word = EF_U32(src);
        ef_addr32 dst;

        if (word == 0u) {
            src += 4u;
            count -= 4;
            if (count <= 0)
                return;
            continue;
        }
        dst = src - delta;
        count -= 4;
        if (count <= 0) {
            put_word_bytes(dst, word, count + 4);
            return;
        }
        put_word_bytes(dst, word, 4);
        src += 4u;
        dst += 4u;
        for (;;) {
            uint8_t value = EF_U8(src);

            --count;
            ++src;
            if (value == 0u)
                break;
            EF_U8(dst) = value;
            ++dst;
            if ((uint16_t)count == 0u)
                return;
        }
    }
}

/* 0xcd9a6, 368 bytes (8 stack arguments, ret 0x20). Blit HEIGHT rows of
 * WIDTH bytes from (SX, SY) of BITMAP (STRIDE bytes per row) to (X, Y)
 * of the raster framebuffer, with zero as the transparent colour. The
 * row pointers live in g_blit_src_row and g_blit_dst_row; the
 * destination advances by the low word of g_video_width. HEIGHT 0 runs
 * 2^32 rows, as in the original. */
void packed_blit_cd9a6(uint32_t stride, uint32_t y, uint32_t x, uint32_t height, uint32_t width, uint32_t sy,
                       uint32_t sx, ef_addr32 bitmap)
{
    uint32_t rows = height;

    g_blit_src_row = bitmap + sx + sy * stride;
    g_blit_dst_row = g_raster_framebuffer + x + EF_U32(RASTER_ROW_TABLE + y * 4u);
    do {
        blit_row(g_blit_src_row, g_blit_src_row - g_blit_dst_row, (int32_t)width);
        g_blit_src_row += stride;
        g_blit_dst_row += (uint16_t)g_video_width;
    } while (--rows != 0u);
}
