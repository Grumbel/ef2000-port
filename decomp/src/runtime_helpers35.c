/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Video init, text width, transform helpers and buffer setup, 0xd1d40..0xfba6c. */

/* 0xd1d40, 102 bytes. Set the screen size and run the video start-up sequence. */
void init_video_d1d40(uint32_t width, uint32_t height)
{
    uint32_t r;

    EF_U32(0x0015b748u) = height;
    EF_U32(0x00420950u) = 0x000fedfcu;
    EF_U32(0x0015b744u) = width;
    r = guest_fn_fc6c8();
    guest_fn_fc39c(r, 0x000fedfcu, EF_U32(0x0015b744u), EF_U32(0x0015b748u));
    guest_fn_c3ae8();
    guest_fn_fef26();
    guest_fn_d663c(0x00420a90u);
    guest_fn_c37ec();
    guest_fn_c3770();
    ((void (*)(uint32_t))EF_U32(0x0015b750u))(0);
    guest_fn_dc2c0();
    ((void (*)(uint32_t))EF_U32(0x0015b74cu))(0);
}

/* 0xdeb87, 108 bytes. Fill count dwords at dst with value. */
void fill_dwords_deb87(ef_addr32 dst, uint32_t value, uint32_t count)
{
    while (count != 0) {
        EF_U32(dst) = value;
        dst += 4;
        count--;
    }
}

/* 0xe284c, 106 bytes. Width in pixels of a string in the given font. */
uint32_t text_width_e284c(ef_addr32 text, uint16_t font)
{
    uint32_t width = 0;

    if (text == 0)
        return 0;
    for (; EF_U8(text) != 0; text++) {
        uint8_t c = EF_U8(text);
        uint8_t glyph = EF_U8(0x004198d2u + ((uint32_t)font << 11) + (uint32_t)c * 8u);
        if (glyph != 0)
            width += (uint32_t)glyph + EF_U8(0x001378dau + (uint32_t)font * 10u);
        else if (c == 0x20)
            width += EF_U8(0x001378d9u + (uint32_t)font * 10u);
    }
    return width;
}

/* 0xe994c, 106 bytes. Build a rotation matrix and optionally mirror it. */
void build_rotation_e994c(uint32_t flags, uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t t;

    if (flags & 1)
        guest_fn_e95c0(a, b, c);
    else
        guest_fn_e977c(a, b, c);
    if (flags & 2) {
        t = EF_U32(0x0046444cu); EF_U32(0x0046444cu) = EF_U32(0x00464444u); EF_U32(0x00464444u) = t;
        t = EF_U32(0x00464458u); EF_U32(0x00464458u) = EF_U32(0x00464448u); EF_U32(0x00464448u) = t;
        t = EF_U32(0x0046445cu); EF_U32(0x0046445cu) = EF_U32(0x00464454u); EF_U32(0x00464454u) = t;
    }
}

/* 0xe99b8, 106 bytes. Transform a vector by the current matrix (result doubled). */
void transform_vector_e99b8(ef_addr32 out, int32_t x, int32_t y, int32_t z)
{
    EF_S32(out) = 2 * (EF_S32(0x0046444cu) * y + EF_S32(0x00464440u) * x + EF_S32(0x00464458u) * z);
    EF_S32(out + 4u) = 2 * (EF_S32(0x00464444u) * x + EF_S32(0x00464450u) * y + EF_S32(0x0046445cu) * z);
    EF_S32(out + 8u) = 2 * (EF_S32(0x00464448u) * x + EF_S32(0x00464454u) * y + EF_S32(0x00464460u) * z);
}

/* 0xf22af, 103 bytes. Set a file's buffer (setvbuf). */
int32_t set_file_buffer_f22af(ef_addr32 file, ef_addr32 buffer, uint32_t mode, uint32_t size)
{
    if (size > 0x7fffffffu)
        return -1;
    if (mode != 0x100u && mode != 0x200u && mode != 0x400u)
        return -1;
    if (buffer != 0 && size == 0)
        return -1;
    update_flags_fa7d2(file);
    if (size != 0)
        EF_U32(file + 0x14u) = size;
    EF_U32(file + 8u) = buffer;
    EF_U8(file + 0xdu) = (uint8_t)(EF_U8(file + 0xdu) & 0xf8);
    EF_U32(file) = buffer;
    EF_U32(file + 0xcu) |= mode;
    if (buffer == 0)
        guest_fn_10f071(file);
    return 0;
}

/* 0xf25b0, 107 bytes. Draw a 3-column by 6-row glyph into the frame buffer. */
void draw_glyph_f25b0(uint8_t value, uint32_t x, uint32_t row, uint32_t glyph)
{
    uint32_t stride = EF_U32(0x001631f4u);
    ef_addr32 dst = EF_U32(0x0015b754u) + EF_U32(0x00163200u + row * 4u) + x;
    ef_addr32 mask = 0x00160cc4u + glyph * 6u;
    int32_t i;

    for (i = 0; i < 6; i++) {
        uint8_t m = EF_U8(mask++);
        if (m & 4)
            EF_U8(dst) = value;
        if (m & 2)
            EF_U8(dst + 1u) = value;
        if (m & 1)
            EF_U8(dst + 2u) = value;
        dst += stride;
    }
    EF_U32(0x001631f4u) = stride;
}

/* 0xf36a4, 109 bytes. Advance the animation counter and wrap it. */
void step_animation_f36a4(void)
{
    guest_fn_f3610(EF_S32(0x00161e18u) >> 16);
    EF_U16(0x00161e16u) = (uint16_t)(EF_U16(0x00161e16u) + EF_U16(0x00161e18u));
    if ((int16_t)EF_U16(0x00161e16u) < 0) {
        EF_U16(0x00161e18u) = 0;
        EF_U16(0x00161e16u) = 0;
        return;
    }
    if ((EF_S32(0x00161e14u) >> 16) == 5) {
        guest_fn_f35f0();
        EF_U16(0x00161e18u) = 0;
        EF_U16(0x00161e16u) = 4;
    }
}

/* 0xfba6c, 105 bytes. Count matching type-2 entries across a shape list. */
uint32_t count_matching_entries_fba6c(uint32_t a, uint32_t b, ef_addr32 listp)
{
    uint32_t count = 0;
    ef_addr32 node;

    for (node = EF_U32(listp); node != 0; node = EF_U32(node + 4u)) {
        int16_t i;
        for (i = 0; i < (int16_t)EF_U8(node + 0x12u); i++) {
            ef_addr32 e = EF_U32(node + 0x13u) + (uint32_t)(int32_t)i * 0x7cu;
            if (EF_U32(e + 8u) == a && EF_U32(e + 0xcu) == b && EF_U8(node + 9u) == 2
                && (uint16_t)guest_fn_bcbc0(EF_U8(e)) != 0)
                count++;
        }
    }
    return count;
}
