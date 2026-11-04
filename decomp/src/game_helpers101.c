/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Complementary blend and the command line token reader. */

/* 0xa2334, 254 bytes. Like a2244 but the cosine weight is taken at the complementary angle and the weights are swapped. */
void blend_by_complement_weight_a2334(ef_addr32 rec)
{
    int32_t angle = EF_S32(rec + 2u) >> 16;
    uint32_t idx = ((uint32_t)(0x4000 - angle) & 0xffffu) >> 3;
    int32_t w = ((int32_t)EF_S16(0x00141e08u + idx * 2u) * 0x4000) / 0x8000;
    int32_t a = (int16_t)(0x4000 - (0x4000 - w));
    int32_t b = (int16_t)(0x4000 - w);
    uint32_t k;

    for (k = 0; k < 3; k++) {
        int32_t first = ((int32_t)EF_S16(rec + 0xeu + k * 2u) * a) / 0x4000;
        int32_t second = ((int32_t)EF_S16(rec + 0x14u + k * 2u) * b) / 0x4000;

        EF_U16(rec + 8u + k * 2u) = (uint16_t)(first + second);
    }
}

/* 0x9d278, 263 bytes. Read the next token (blank/comma separated, optionally quoted, at most 0x50 bytes) from the text at ctx using the cursor at +0x556 into the buffer at +0x400; a leading ';' blanks the token. */
void read_next_token_9d278(ef_addr32 ctx)
{
    uint32_t n = 0;
    uint8_t c;

    for (;;) {
        c = EF_U8(ctx + (uint32_t)(int32_t)EF_S16(ctx + 0x556u));
        if (c != 0x20 && c != 0x2c && c != 0x09)
            break;
        EF_U16(ctx + 0x556u) = (uint16_t)(EF_U16(ctx + 0x556u) + 1u);
    }
    if (c == 0x22) {
        EF_U16(ctx + 0x556u) = (uint16_t)(EF_U16(ctx + 0x556u) + 1u);
        for (;;) {
            ef_addr32 p = ctx + (uint32_t)(int32_t)EF_S16(ctx + 0x556u);
            int16_t cur;

            if (EF_U8(p) == 0x22 || EF_U8(p) == 0 || (int32_t)n >= 0x50)
                break;
            cur = EF_S16(ctx + 0x556u);
            EF_U16(ctx + 0x556u) = (uint16_t)(cur + 1);
            EF_U8(ctx + n + 0x400u) = EF_U8(ctx + (uint32_t)(int32_t)cur);
            n++;
        }
        if (EF_U8(ctx + (uint32_t)(int32_t)EF_S16(ctx + 0x556u)) == 0x22)
            EF_U16(ctx + 0x556u) = (uint16_t)(EF_U16(ctx + 0x556u) + 1u);
    } else {
        for (;;) {
            uint8_t b = EF_U8(ctx + (uint32_t)(int32_t)EF_S16(ctx + 0x556u));
            int16_t cur;

            if (b == 0x20 || b == 0x09 || b == 0 || b == 0x2c || (int32_t)n >= 0x50)
                break;
            cur = EF_S16(ctx + 0x556u);
            EF_U16(ctx + 0x556u) = (uint16_t)(cur + 1);
            EF_U8(ctx + n + 0x400u) = EF_U8(ctx + (uint32_t)(int32_t)cur);
            n++;
        }
    }
    EF_U8(ctx + n + 0x400u) = 0;
    if (EF_U8(ctx + 0x400u) == 0x3b)
        EF_U8(ctx + 0x400u) = 0;
}
