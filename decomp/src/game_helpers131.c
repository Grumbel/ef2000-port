/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include <stdarg.h>
#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Formatted text sizing and drawing, record block reads. */

/* 0x48c34, 91 bytes. Width of a formatted string: mode 1 uses the proportional font width, mode 2 six pixels per character, anything else eight (cdecl, variadic). */
int32_t formatted_text_width_48c34(uint32_t mode, ef_addr32 fmt, ...)
{
    char buf[0x50];
    va_list ap;

    va_start(ap, fmt);
    format_to_buffer_e3022((uint32_t)(uintptr_t)buf, fmt, (uint32_t)(uintptr_t)&ap[0]);
    va_end(ap);
    if ((uint8_t)mode == 1)
        return (int32_t)text_width_159a0((uint32_t)(uintptr_t)buf);
    if ((uint8_t)mode == 2)
        return (int32_t)guest_fn_b1b8b((uint32_t)(uintptr_t)buf) * 6;
    return (int32_t)(guest_fn_b1b8b((uint32_t)(uintptr_t)buf) << 3);
}

/* 0x3d308, 97 bytes. Draw a formatted string at character cell (col, row) with a one-pixel shadow: col * 8, row * 9 pixels (cdecl, variadic). */
void draw_formatted_text_3d308(int32_t col, int32_t row, ef_addr32 fmt, ...)
{
    char buf[0x50];
    va_list ap;
    int32_t x;
    int32_t y;

    va_start(ap, fmt);
    format_to_buffer_e3022((uint32_t)(uintptr_t)buf, fmt, (uint32_t)(uintptr_t)&ap[0]);
    va_end(ap);
    x = (int16_t)(col << 3);
    y = (int16_t)(row * 9);
    draw_text_e30e0(0, (uint32_t)(x + 1), (uint32_t)(y + 1), (uint32_t)(uintptr_t)buf);
    draw_text_e30e0(1, (uint32_t)x, (uint32_t)y, (uint32_t)(uintptr_t)buf);
}

/* 0xebb10, 69 bytes. Read an entry's block: find the directory entry for the key in the context, seek the context's handle to the entry position and read its length into the buffer; returns 1 on success, 0 when the context or entry is missing. */
uint8_t read_entry_block_ebb10(uint32_t key, ef_addr32 buffer, ef_addr32 ctx)
{
    ef_addr32 entry;

    if (ctx == 0)
        return 0;
    entry = guest_fn_eb520_x(key, ctx);
    if (entry == 0)
        return 0;
    seek_start_a7ab4(0, 0, 0, 0, EF_U32(ctx), EF_U32(entry + 4u));
    read_handle_a79d4(0, 0, 0, 0, EF_U32(ctx), EF_U32(entry + 8u), buffer);
    return 1;
}

/* 0xbeebc, 109 bytes. Run 0xbed1c once for every entry of a list (count from 0xbc5d0), passing the index as a byte in EBX and forwarding the other arguments; returns the last result, or the second argument when the list is empty (five stack arguments, callee cleans up). */
uint32_t for_each_entry_beebc(ef_addr32 a, int16_t arg2, ef_addr32 list, ef_addr32 arg4, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4)
{
    int16_t count = (int16_t)list_count(list);
    int16_t i = 0;
    uint32_t result = (uint32_t)(int32_t)arg2;

    if (count > 0) {
        do {
            result = guest_fn_bed1c_x(a, (uint32_t)(int32_t)arg2, (uint8_t)i, list, arg4, s0, s1, s2, s3, s4);
            i++;
        } while (i < count);
    }
    return result;
}

/* 0x48bb0, 131 bytes. Draw a formatted string at (x, y) with the renderer chosen by mode (1: 0x15a14, 2: 0xe5db4, otherwise 0xe605c); the first argument is a colour byte (cdecl, variadic). */
void draw_formatted_text_mode_48bb0(uint32_t color, uint32_t mode, int32_t x, int32_t y, ef_addr32 fmt, ...)
{
    char buf[0x50];
    va_list ap;

    va_start(ap, fmt);
    format_to_buffer_e3022((uint32_t)(uintptr_t)buf, fmt, (uint32_t)(uintptr_t)&ap[0]);
    va_end(ap);
    if ((uint8_t)mode == 1)
        guest_fn_15a14_x(color & 0xffu, (int16_t)x, (int16_t)y, (uint32_t)(uintptr_t)buf);
    else if ((uint8_t)mode == 2)
        guest_fn_e5db4_x(color & 0xffu, (int16_t)x, (int16_t)y, (uint32_t)(uintptr_t)buf);
    else
        guest_fn_e605c_x(color & 0xffu, (int16_t)x, (int16_t)y, (uint32_t)(uintptr_t)buf);
}

/* 0x7e01c, 136 bytes. Position the numbered child widgets of the container at 0x188034: hide children "0".."8" at (0, 0), then place children "lo".."hi" at (0x15, 0xf). */
void place_numbered_widgets_7e01c(int16_t lo, int16_t hi)
{
    ef_addr32 container = find_named_or_abort_a46cc(0x00188034u);
    char name[8];
    int16_t i;

    for (i = 0; i <= 8; i++) {
        signed_to_digits_cbd0f(i, (uint32_t)(uintptr_t)name, 10u);
        move_widget_a74bc(find_widget_checked_a78bc(container, (uint32_t)(uintptr_t)name), 0, 0);
    }
    for (i = lo; hi >= i; i++) {
        signed_to_digits_cbd0f(i, (uint32_t)(uintptr_t)name, 10u);
        move_widget_a74bc(find_widget_checked_a78bc(container, (uint32_t)(uintptr_t)name), 0x15, 0xf);
    }
}

/* 0x97d10, 125 bytes. Set the view origin to the centre of the box (x1, x2, y1, y2), latch its corners and run the 0xdc71b / 0xcdf48 box updates. */
void update_box_view_97d10(int16_t x1, int16_t x2, int16_t y1, int16_t y2)
{
    int32_t a = x1;
    int32_t b = x2;
    int32_t c = y1;
    int32_t d = y2;
    int32_t mid_x = (a + b) / 2;
    int32_t mid_y = (c + d) / 2;

    set_view_origin_dc21c((uint32_t)d, (uint32_t)b, (uint32_t)(int32_t)(int16_t)mid_y, (uint32_t)(int32_t)(int16_t)mid_x);
    guest_fn_ce328_w(a, b, c, d);
    guest_fn_dc71b_x((uint32_t)(int32_t)(int16_t)mid_y, (uint32_t)d, (uint32_t)c, (uint32_t)b, (uint32_t)a);
    guest_fn_cdf48((uint32_t)a, (uint32_t)b, (uint32_t)c, (uint32_t)d);
}
