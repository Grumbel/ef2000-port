/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Number formatting, directory iteration and selection helpers. */

/* 0x8b328, 99 bytes. Format a number right-aligned into a field of the given width, with ',' every 3 digits. */
ef_addr32 format_number_commas_8b328(ef_addr32 dest, int32_t width, int32_t value)
{
    ef_addr32 p = dest + (uint32_t)width;
    int32_t left = width - 1;
    uint32_t digits = 0;

    if (left < 0)
        return dest;
    p--;
    EF_U8(p) = 0;
    if (left > 0) {
        do {
            uint8_t ch;

            digits++;
            if ((digits & 3u) == 0) {
                ch = ',';
            } else {
                ch = (uint8_t)(value % 10 + 0x30);
                value = value / 10;
            }
            left--;
            p--;
            EF_U8(p) = ch;
        } while (left > 0 && value > 0);
    }
    guest_string_copy(dest, p);
    return dest;
}

/* 0x8b684, 89 bytes. Start (mode 1) or continue a directory scan; returns 1 when no more entries. */
uint8_t scan_directory_entry_8b684(int16_t mode, ef_addr32 name, ef_addr32 suffix, ef_addr32 buf)
{
    uint16_t r;

    if (mode == 1) {
        guest_string_copy(buf, name);
        guest_string_append(buf, suffix);
        r = (uint16_t)guest_fn_decde_x(buf, 0, 0x0020f434u);
    } else {
        r = (uint16_t)guest_fn_decfb_x(0x0020f434u);
    }
    if (r == 0) {
        guest_string_copy(buf, name);
        guest_string_append(buf, 0x0020f452u);
    }
    return r == 0;
}

/* 0x9bfe3, 65 bytes. Add an offset to a segment:offset pair, renormalising so the offset stays in 0..0xffff. */
uint32_t add_far_offset_9bfe3(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t far_ptr, int32_t delta)
{
    uint32_t seg = far_ptr >> 16;
    int32_t off = (int32_t)((far_ptr & 0xffffu) + (uint32_t)delta);

    (void)u0; (void)u1; (void)u2; (void)u3;
    /* ORIGINAL BUG: the unsigned test also loops for negative offsets, so a negative delta walks the offset down through the 32-bit wrap. */
    while ((uint32_t)off > 0xffffu) {
        seg++;
        off -= 16;
    }
    while (off < 0) {
        seg--;
        off += 16;
    }
    return (seg << 16) | (uint32_t)off;
}

/* 0x9fdd4, 72 bytes. Select the entry with the smallest rating below 10 and activate it; -1 slot ends early. */
int32_t select_lowest_rated_entry_9fdd4(void)
{
    int16_t best = 10;
    int16_t best_index = 0;
    int16_t i;

    for (i = 0; i < 10; i++) {
        ef_addr32 rec = (uint32_t)(int32_t)i * 0x4eu;
        int16_t rating;

        if ((EF_S32(rec + 0x00416e44u) >> 16) == -1)
            return i;
        rating = EF_S16(rec + 0x00416e46u);
        if (best > rating) {
            best = rating;
            best_index = i;
        }
    }
    guest_fn_9fe1c_x(best_index);
    return best_index;
}

/* 0xa9878, 88 bytes. Build a status line from three fragments, store the title, and refresh. */
void build_status_line_a9878(ef_addr32 name)
{
    char buf[0x8c];

    guest_string_copy((uint32_t)(uintptr_t)buf, guest_fn_a7b70_x());
    guest_string_append((uint32_t)(uintptr_t)buf, 0x0018d788u);
    guest_string_append((uint32_t)(uintptr_t)buf, name);
    guest_string_append((uint32_t)(uintptr_t)buf, 0x0018d790u);
    guest_string_copy((uint32_t)(uintptr_t)buf + 0x64u, name + 9u);
    guest_fn_cd554_x();
    guest_fn_cd4d4_x();
}
