/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Rectangle outline, VGA palette upload and record message helpers. */

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
