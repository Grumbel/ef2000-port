/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Slot matching and layout margin computation. */

/* 0x1f64, 331 bytes. Select the active entry: a direct code match first, else the best flagged candidate. */
void select_active_entry_1f64(void)
{
    uint8_t code = EF_U8(0x00197080u);
    static const uint32_t flags[7] = {
        0x00197061u, 0x00197065u, 0x00197069u, 0x0019706du, 0x00197079u, 0x00197071u, 0x00197075u
    };
    int16_t i;
    int16_t level;
    uint32_t found = 0;

    for (i = 0; i < 2; i++) {
        ef_addr32 rec = 0x00196190u + (uint32_t)i * 0x34u;

        if (EF_U8(rec) == code) {
            EF_U8(0x00197086u) = (uint8_t)guest_fn_f60_x(EF_U8(rec + 1u));
            EF_U8(0x00197087u) = (uint8_t)i;
            EF_U8(0x00197085u) = 0xff;
            return;
        }
    }
    for (level = 10; level >= 0; level--) {
        uint32_t k;

        for (k = 0; k < 7; k++) {
            if (EF_U8(0x00196391u + k) == (uint16_t)level && EF_U32(flags[k]) != 0)
                found = k + 1;
        }
        if ((uint16_t)found != 0)
            break;
    }
    EF_U8(0x00197085u) = (uint8_t)found;
    EF_U8(0x00197087u) = 0xff;
    EF_U8(0x00197086u) = (uint8_t)guest_fn_f60_x(EF_U8((uint32_t)(int32_t)(int16_t)found * 0x33u + 0x001961f8u));
}

/* 0xa66cc, 316 bytes. Recompute the left/right and top/bottom padding of a layout box. */
void compute_box_padding_a66cc(ef_addr32 box, uint8_t grow)
{
    uint16_t x;
    uint16_t y;
    uint16_t maxx;
    uint16_t maxy;
    uint8_t mode;

    if (box == 0 || EF_U32(box + 0x27u) == 0)
        return;
    guest_fn_a63c0_x(box);
    if ((EF_U8(box + 0x1du) & 4u) != 0 && (EF_U8(box + 0x1du) & 8u) == 0)
        EF_U16(box + 0x2fu) = (uint16_t)(EF_U16(box + 0x2fu) + 0x10);
    if (grow != 0 && (EF_U8(box + 0x8cu) & 0x1fu) == 0) {
        if (EF_U16(box + 0x2fu) > EF_U16(box + 4u))
            EF_U16(box + 4u) = EF_U16(box + 0x2fu);
        if (EF_U16(box + 0x31u) > EF_U16(box + 6u))
            EF_U16(box + 6u) = EF_U16(box + 0x31u);
    }
    x = EF_U16(box + 0x2fu);
    maxx = EF_U16(box + 4u);
    if (x > maxx) {
        if ((int32_t)x - 8 < (int32_t)maxx)
            EF_U16(box + 0x2bu) = (uint16_t)(maxx - x + 8);
        else
            EF_U16(box + 0x2bu) = 0;
    } else {
        mode = (uint8_t)(EF_U8(box + 0x1cu) & 3u);
        if (mode == 0)
            EF_U16(box + 0x2bu) = (uint16_t)((int32_t)(maxx - x) >> 1);
        else if (mode == 1)
            EF_U16(box + 0x2bu) = 0;
        else if (mode == 2)
            EF_U16(box + 0x2bu) = (uint16_t)(maxx - x);
    }
    y = EF_U16(box + 0x31u);
    maxy = EF_U16(box + 6u);
    if (y > maxy) {
        EF_U16(box + 0x2du) = 0;
        return;
    }
    mode = (uint8_t)((EF_U8(box + 0x1cu) >> 2) & 3u);
    if (mode == 0)
        EF_U16(box + 0x2du) = (uint16_t)(((int32_t)(maxy - y) >> 1) + ((EF_U8(box + 0x8cu) & 0x1fu) == 9 ? 1 : 0) + 1);
    else if (mode == 1)
        EF_U16(box + 0x2du) = 0;
    else if (mode == 2)
        EF_U16(box + 0x2du) = (uint16_t)(maxy - y);
}
