/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Palette fade-in, path building and glyph string drawing wrappers. */

/* 0xde5f4, 84 bytes. Fade the palette in from black over 80 steps. */
void fade_palette_in_slow_de5f4(ef_addr32 palette)
{
    uint8_t buf[0x300];
    uint32_t level;
    uint32_t i;

    for (level = 1; level <= 0x50; level++) {
        for (i = 0; i < 0x300; i++)
            buf[i] = (uint8_t)((uint32_t)EF_U8(palette + i) * level / 0x50u);
        guest_fn_dc2e0();
        guest_fn_d663c((uint32_t)(uintptr_t)buf);
    }
}

/* 0xe7cec, 59 bytes. Build a path from the default directory plus two parts, appending the third part unless it equals the string at 0x190c50. */
void build_path_with_name_e7cec(ef_addr32 part_a, ef_addr32 part_b, ef_addr32 name, ef_addr32 buffer)
{
    build_default_path_9c980(buffer);
    guest_fn_9ca57(buffer, part_a);
    guest_fn_9ca57(buffer, part_b);
    if (guest_fn_a4380(name, 0x00190c50u) != 0)
        append_two_e7cd0(buffer, name);
}

/* 0xea17c, 36 bytes. Build a path from a record's first name, an extra part and the record's second name. */
void build_path_from_record_ea17c(ef_addr32 buffer, ef_addr32 record, ef_addr32 extra)
{
    build_path_9c9c0(buffer, EF_U32(record + 4u));
    guest_fn_9ca57(buffer, extra);
    guest_fn_9ca57(buffer, EF_U32(record + 8u));
}

/* 0xeb008, 61 bytes. Draw a NUL-terminated string glyph by glyph (6 pixels apart, 6-bit codes). */
void draw_glyph_string_eb008(uint32_t ctx, uint32_t x, uint32_t y, ef_addr32 text)
{
    while (EF_U8(text) != 0) {
        guest_fn_eaf80_x(ctx, x, y, (uint32_t)(EF_U8(text) & 0x3fu));
        text++;
        x += 6;
    }
}
