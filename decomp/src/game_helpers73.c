/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Letter table stepping and group refresh. */

/* 0x11d44, 156 bytes. Advance the letter table walk; emits up to 18 letters per step for the active mode. */
void step_letter_table_11d44(void)
{
    int32_t mode;
    uint8_t code = EF_U8(0x00122c53u);

    if (code == 1)
        mode = 0;
    else if (code == 2)
        mode = 1;
    else if (code == 4)
        mode = 2;
    else
        mode = -1;
    if (EF_U16(0x0012288au) == 0)
        return;
    if ((EF_S32(0x00122888u) >> 16) == 1) {
        if ((int16_t)mode >= 0 && mode < 4) {
            int16_t bx = (int16_t)EF_U16(0x00121ec8u + (uint32_t)mode * 2u);
            int16_t i;

            for (i = 0; i < 0x12; i++) {
                EF_U8(0x0012288cu) = (uint8_t)(i + 0x41);
                guest_fn_11e7c_x(bx);
                bx++;
            }
        }
        EF_U16(0x0012288au) = (uint16_t)(EF_U16(0x0012288au) + 1);
        return;
    }
    if (EF_U8(0x0041f7adu) == 0) {
        guest_fn_12140_x();
        EF_U16(0x0012288au) = 0;
    }
}

/* 0x12210, 142 bytes. Refresh a group's entries: its flagged head, then every member from the cx-th list on. */
void refresh_group_entries_12210(int16_t group)
{
    ef_addr32 rec = (uint32_t)(int32_t)group * 29u;
    int16_t n = (int8_t)EF_U8(rec + 0x00121fc0u);
    int16_t skip;
    int32_t bx;

    if (n == 0)
        return;
    skip = (int16_t)(n - 1);
    if ((EF_U16(rec + 0x00121fc2u) & 2u) != 0 && EF_U8(rec + 0x00121fb4u) == 0)
        guest_fn_11d20_x(group, 1);
    bx = 0;
    if (skip != 0) {
        do {
            int32_t v = EF_S32(0x00121f0cu + (uint32_t)(int32_t)(int16_t)bx * 2u) >> 16;

            bx++;
            if (v == -1)
                skip--;
        } while (skip != 0);
    }
    for (;;) {
        int32_t v = EF_S32(0x00121f0cu + (uint32_t)(int32_t)(int16_t)bx * 2u) >> 16;

        if (v == -1)
            break;
        bx++;
        guest_fn_11d20_x(v, 0);
    }
}
