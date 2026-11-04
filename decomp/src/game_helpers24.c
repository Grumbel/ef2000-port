/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Font width, rotation and viewport helpers, 0xdc21c..0xdec4c. */

/* 0xdc21c, 52 bytes. Store two 16.16 coordinates (with a half-unit bias) from stack arguments. */
void set_view_origin_dc21c(uint32_t stack_a, uint32_t stack_b, uint32_t stack_y, uint32_t stack_x)
{
    (void)stack_a;
    (void)stack_b;
    EF_U16(0x0015bdb8u) = (uint16_t)stack_x;
    EF_U32(0x00164658u) = (stack_x << 16) | 0x8000u;
    EF_U16(0x0015bdbcu) = (uint16_t)stack_y;
    EF_U32(0x0016465cu) = (stack_y << 16) | 0x8000u;
}

/* 0xdc7e4, 78 bytes. Rotate (b, a) by the sine/cosine pair at 0x425f90 and store two words. */
void rotate_point_dc7e4(ef_addr32 out, int16_t a, int16_t b)
{
    int32_t sine = (int32_t)EF_U32(0x00425f90u) >> 16;
    int32_t cosine = (int32_t)EF_U32(0x00425f8eu) >> 16;
    int32_t x = ((int32_t)a * cosine - (int32_t)b * sine) >> 15;
    int32_t y = ((int32_t)b * cosine + (int32_t)a * sine) >> 15;

    EF_U16(out) = (uint16_t)x;
    EF_U16(out + 2u) = (uint16_t)y;
}

/* 0xdc890, 69 bytes. Halve three dwords until all fit in a signed 16-bit range. */
void halve_triple_to_fit16_dc890(ef_addr32 a, ef_addr32 b, ef_addr32 c)
{
    for (;;) {
        int32_t va;
        int32_t vb;
        int32_t vc;

        EF_S32(a) = EF_S32(a) >> 1;
        EF_S32(b) = EF_S32(b) >> 1;
        EF_S32(c) = EF_S32(c) >> 1;
        va = EF_S32(a);
        vb = EF_S32(b);
        vc = EF_S32(c);
        if (va < -0x8000 || va > 0x7fff)
            continue;
        if (vb < -0x8000 || vb > 0x7fff)
            continue;
        if (vc < -0x8000 || vc > 0x7fff)
            continue;
        return;
    }
}

/* 0xdced0, 88 bytes. Pixel width of a string in the font with 4-pixel blanks. */
uint32_t text_width_4_dced0(ef_addr32 text)
{
    uint32_t width = 0;
    int16_t i = 0;

    for (;;) {
        uint32_t c = EF_U8(text + (uint32_t)(int32_t)i);
        uint32_t glyph;

        if (c == 0)
            return width;
        i++;
        c &= 0x3fu;
        if (c == 0x20u || c == 0x09u) {
            width += 4u;
            continue;
        }
        glyph = EF_U8(0x0015e1beu + c);
        if (glyph == 0xffu)
            continue;
        width += (uint32_t)EF_U8(0x0015e1feu + glyph) + 1u;
    }
}

/* 0xdd10c, 87 bytes. Pixel width of a string in the font with 5-pixel blanks. */
uint32_t text_width_5_dd10c(ef_addr32 text)
{
    uint32_t width = 0;
    int16_t i = 0;

    for (;;) {
        uint32_t c = EF_U8(text + (uint32_t)(int32_t)i);
        uint32_t glyph;

        if (c == 0)
            return width;
        i++;
        c &= 0x3fu;
        if (c == 0x20u || c == 0x09u) {
            width += 5u;
            continue;
        }
        glyph = EF_U8(0x0015e1beu + c);
        if (glyph == 0xffu)
            continue;
        width += (uint32_t)EF_U8(0x0015e264u + glyph) + 1u;
    }
}

/* 0xdd210, 89 bytes. Pixel width of a string in the font with 6-pixel blanks and 2-pixel gaps. */
uint32_t text_width_6_dd210(ef_addr32 text)
{
    uint32_t width = 0;
    int16_t i = 0;

    for (;;) {
        uint32_t c = EF_U8(text + (uint32_t)(int32_t)i);
        uint32_t glyph;

        if (c == 0)
            return width;
        i++;
        c &= 0x3fu;
        if (c == 0x20u || c == 0x09u) {
            width += 6u;
            continue;
        }
        glyph = EF_U8(0x0015e1beu + c);
        if (glyph == 0xffu)
            continue;
        width += (uint32_t)EF_U8(0x0015e231u + glyph) + 2u;
    }
}

/* 0xdd650, 78 bytes. Pixel width of a string; blanks are tested before masking. */
uint32_t text_width_raw_dd650(ef_addr32 text)
{
    uint32_t width = 0;
    int16_t i = 0;

    for (;;) {
        uint32_t c = EF_U8(text + (uint32_t)(int32_t)i);

        if (c == 0)
            return width;
        if (c == 0x20u || c == 0x09u) {
            width += 5u;
        } else {
            int32_t glyph = (int8_t)EF_U8(0x0015e1beu + (c & 0x3fu));
            uint32_t w = (uint32_t)glyph & 0xffff0000u;

            /* ORIGINAL BUG: a missing glyph (-1) leaves the high word set in the width. */
            w |= EF_U8(0x0015e2c6u + (uint32_t)glyph);
            width += w + 1u;
        }
        i++;
    }
}

/* 0xdd988, 90 bytes. Copy the saved view state at 0x15ee80..0x15ee90 to eight output pointers. */
void get_view_state_dd988(ef_addr32 out_a, ef_addr32 out_b, ef_addr32 out_c, ef_addr32 out_d,
                          ef_addr32 stack_e, ef_addr32 stack_f, ef_addr32 stack_g, ef_addr32 stack_h)
{
    EF_U32(out_a) = EF_U32(0x0015ee80u);
    EF_U32(out_b) = EF_U32(0x0015ee90u);
    EF_U16(out_c) = EF_U16(0x0015ee84u);
    EF_U16(out_d) = EF_U16(0x0015ee86u);
    EF_U16(stack_e) = EF_U16(0x0015ee88u);
    EF_U16(stack_f) = EF_U16(0x0015ee8au);
    EF_U16(stack_g) = EF_U16(0x0015ee8cu);
    EF_U16(stack_h) = EF_U16(0x0015ee8eu);
}

/* 0xdec4c, 65 bytes. Read a byte from the clipped 8-bit plane, or -1 outside the window. */
uint32_t read_plane_pixel_dec4c(int32_t x, int32_t y)
{
    if (x < EF_S32(0x0015bdc0u) || x > EF_S32(0x0015bdc4u) ||
        y < EF_S32(0x0015bdc8u) || y > EF_S32(0x0015bdccu))
        return 0xffffffffu;
    return EF_U8(EF_U32(0x00163200u + (uint32_t)y * 4u) + (uint32_t)x + EF_U32(0x0015b754u));
}
