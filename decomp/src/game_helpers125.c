/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Rectangle outline, VGA palette upload and record message helpers. */

/* 0xe9a24 takes three stack arguments and removes them on return. */
static void palette_copy_e9a24(uint32_t length, ef_addr32 source, ef_addr32 destination)
{
    __asm {
        push destination
        push source
        push length
        mov eax, 0x000e9a24
        call eax
    }
}

/* 0x1f7d4, 102 bytes. Draw a rectangle outline with four tactical lines of marker a; x spans b..d, y spans c..e (e on the stack). */
void draw_tactical_rect_1f7d4(int16_t a, int16_t b, int16_t c, int16_t d, int16_t e)
{
    draw_tactical_line(c, d, c, b, a);
    draw_tactical_line(e, d, c, d, a);
    draw_tactical_line(e, b, e, d, a);
    draw_tactical_line(c, b, e, b, a);
}

/* 0xfeec8, 94 bytes. Upload a 256-entry RGB palette to the VGA DAC (ports 0x3c8/0x3c9), sending only entries that differ from the shadow copy at 0x17cfc0 and moving the write index only when needed. */
void upload_vga_palette_feec8(ef_addr32 palette)
{
    ef_addr32 shadow = 0x0017cfc0u;
    uint8_t index = 0;
    uint8_t dac = 0;

    guest_fn_117b0c(0x3c8u, 0);
    do {
        uint32_t src = EF_U32(palette);
        uint32_t diff = (EF_U32(shadow) ^ src) & 0x00ffffffu;
        if (diff != 0) {
            EF_U32(shadow) ^= diff;
            if (index != dac) {
                guest_fn_117b0c(0x3c8u, index);
                dac = index;
            }
            guest_fn_117b0c(0x3c9u, src & 0xffu);
            guest_fn_117b0c(0x3c9u, (src >> 8) & 0xffu);
            guest_fn_117b0c(0x3c9u, (src >> 16) & 0xffu);
            dac++;
        }
        palette += 3u;
        shadow += 3u;
        index++;
    } while (index != 0);
}

/* 0x1052f0, 67 bytes. Once per record: send message 0x401 with the two words at +0x14 and +0x18 (cdecl, record on the stack). */
void send_record_size_message_1052f0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec)
{
    uint16_t buf[6] = { 0, 0, 0, 0, 0, 0 };
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(rec + 0x54u) == 0) {
        buf[2] = EF_U16(rec + 0x14u);
        buf[3] = EF_U16(rec + 0x18u);
        guest_fn_101f88(EF_U32(rec), 0x401u, (uint32_t)(uintptr_t)buf, 0);
        EF_U32(rec + 0x54u) = 1;
    }
}

/* 0x111df0, 77 bytes. If a pending count is set at +0x1a8, send message 0x502 with it and clear the count and +0x1ac (cdecl, record on the stack). */
void send_pending_count_message_111df0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec)
{
    uint16_t buf[6] = { 0, 0, 0, 0, 0, 0 };
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_S32(rec + 0x1a8u) > 0) {
        buf[2] = EF_U16(rec + 0x1a8u);
        guest_fn_101f88(EF_U32(rec), 0x502u, (uint32_t)(uintptr_t)buf, 0);
        EF_U32(rec + 0x1a8u) = 0;
        EF_U32(rec + 0x1acu) = 0;
    }
}

/* 0xd663c, 53 bytes. Remap a 256-entry palette, upload it to the VGA DAC and keep a copy of the source at 0x420a90. */
void apply_palette_d663c(ef_addr32 palette)
{
    uint8_t buf[0x300];
    remap_palette_d65a8((uint32_t)(uintptr_t)buf, palette);
    upload_vga_palette_feec8((uint32_t)(uintptr_t)buf);
    palette_copy_e9a24(0x300u, palette, 0x00420a90u);
}

/* 0xd710c, 44 bytes. If a record's mode (+0x30) is 1, register its stack range (+8, length +0x10 minus 1) and set its field to 0x40 (cdecl, record on the stack). */
void finish_record_range_d710c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec)
{
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (EF_U32(rec + 0x30u) == 1u) {
        stack_range_104920(EF_U32(rec + 8u), EF_U32(rec + 0x10u) - 1u);
        set_field_1067cc(rec, 0x40u);
    }
}
