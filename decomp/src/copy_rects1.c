/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Rectangle copies between the screen and the backing buffer. */

/* 0x109e08, 55 bytes. Copy count rows of len bytes at row offset table[idx] + x from the screen (0x15b754) into the buffer returned by 0xc382c. */
void copy_rows_from_screen_109e08(uint32_t x, uint32_t idx, uint32_t len, int32_t count)
{
    uint32_t offset = EF_U32(0x00163200u + idx * 4u) + x;
    copy_rows_109dd0(guest_fn_c382c() + offset, EF_U32(0x0015b754u) + offset, len, count);
}

/* 0xe5114, 88 bytes. Copy every rectangle of the list at 0x43399e (8-byte entries, count in the word at 0x433eac) from the screen into the backing buffer. */
void save_rect_list_e5114(void)
{
    uint32_t i;
    for (i = 0; i < 0xffffu && (uint16_t)i < EF_U16(0x00433eacu); i++) {
        uint32_t e = 0x0043399eu + i * 8u;
        copy_rows_from_screen_109e08((uint32_t)(int32_t)EF_S16(e + 2u), (uint32_t)(int32_t)EF_S16(e + 4u),
                                     (uint32_t)(int32_t)EF_S16(e + 6u), (int32_t)EF_S16(e + 8u));
    }
}

/* 0xe516c, 88 bytes. Copy every rectangle of the list at 0x43399e from the backing buffer back to the screen. */
void restore_rect_list_e516c(void)
{
    uint32_t i;
    for (i = 0; i < 0xffffu && (uint16_t)i < EF_U16(0x00433eacu); i++) {
        uint32_t e = 0x0043399eu + i * 8u;
        copy_rows_at_109e40((uint32_t)(int32_t)EF_S16(e + 2u), (uint32_t)(int32_t)EF_S16(e + 4u),
                            (uint32_t)(int32_t)EF_S16(e + 6u), (int32_t)EF_S16(e + 8u));
    }
}
