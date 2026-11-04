/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Palette fades, text width and entry count helpers. */

/* Scale the 0x300-byte palette at src by level/denominator into a stack buffer and upload it. */
static void fade_step(ef_addr32 src, uint8_t level, uint32_t denominator)
{
    uint8_t buf[0x300];
    uint32_t i;

    for (i = 0; i < 0x300; i++)
        buf[i] = (uint8_t)((uint32_t)EF_U8(src + i) * level / denominator);
    guest_fn_dc2e0();
    guest_fn_d663c((uint32_t)(uintptr_t)buf);
}

/* 0xde580, 113 bytes. Fade the palette in from black over 40 steps. */
void fade_palette_in_de580(ef_addr32 palette)
{
    uint8_t level = 1;

    do {
        fade_step(palette, level, 0x28);
        level++;
    } while (level <= 0x28);
}

/* 0xde648, 121 bytes. Fade the palette out to black over 40 steps, then clear and reset it. */
void fade_palette_out_de648(ef_addr32 palette)
{
    uint8_t level = 0x28;

    do {
        fade_step(palette, level, 0x28);
        level--;
    } while (level > 0);
    clear_buffer_dc2c0();
    guest_fn_d65e8();
}

/* 0xde6c4, 121 bytes. Same as de648 but over 80 steps. */
void fade_palette_out_slow_de6c4(ef_addr32 palette)
{
    uint8_t level = 0x50;

    do {
        fade_step(palette, level, 0x50);
        level--;
    } while (level > 0);
    clear_buffer_dc2c0();
    guest_fn_d65e8();
}

/* 0xe1df0, 123 bytes. Pixel width of up to len characters of a string for the given font (spaces use the font's space width). */
int32_t text_width_e1df0(ef_addr32 text, int16_t len, uint8_t font)
{
    int32_t width = 0;
    int32_t i = 0;

    if (text == 0)
        return 0;
    while (i < (int32_t)len) {
        uint32_t ch = EF_U8(text);
        uint32_t w;
        if (ch == 0)
            break;
        if (ch == 0x20) {
            w = EF_U8(0x001378d9u + (uint32_t)font * 10u);
        } else {
            w = EF_U8(0x001378dau + (uint32_t)font * 10u)
                + EF_U8(0x004198d2u + ((uint32_t)font << 11) + ch * 8u);
        }
        width += (int32_t)w;
        text++;
        i++;
    }
    return width;
}

/* 0xfab80, 112 bytes. Half the number of non-type-1 sub-entries of an object, clamped to 1..2. */
int32_t half_active_entry_count_fab80(ef_addr32 object)
{
    int16_t count = 0;
    int16_t i = 0;
    int32_t half;

    while (i < (int16_t)EF_U8(object + 0x12u)) {
        if (EF_U8(EF_U32(object + 0x13u) + (uint32_t)(int32_t)i * 0x7cu + 3u) != 1)
            count++;
        i++;
    }
    half = (int32_t)count / 2;
    if (half < 1)
        half = 1;
    if (half > 2)
        half = 2;
    return half;
}
