/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Ninth x87 batch: ratio and best-weapon scans. */

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

/* 0xc964, 218 bytes. Recompute the ratio of matching entries and store it as a float. */
void update_match_ratio_c964(void)
{
    int16_t count_total = EF_S16(0x001a53c0u);
    uint32_t base = EF_U32(0x001379b0u);
    int16_t total = 0;
    int16_t hits = 0;
    int16_t i;
    int16_t di;
    double ratio;

    for (i = 0; i < count_total; i++) {
        if (EF_U16(0x00197320u + (uint32_t)(int32_t)i * 2u) != 0)
            total = (int16_t)(total + EF_U16(0x001a53b0u + (uint32_t)(int32_t)i * 2u));
    }
    for (di = 0; (int32_t)di < (int32_t)base; di++) {
        ef_addr32 rec = EF_U32(0x001379bcu) + (uint32_t)(int32_t)di * 0x36u;

        if (EF_U16(rec + 0x16u) != EF_U16(0x00197696u))
            continue;
        for (i = 0; i < count_total; i++) {
            uint32_t off = (uint32_t)(int32_t)i * 2u;

            if (EF_U16(rec + 0x18u) == EF_U16(0x001a5280u + off) && EF_U16(0x00197320u + off) != 0 &&
                (EF_S32(rec + 0xau) >> 16) == 1)
                hits++;
        }
    }
    ratio = (double)hits / (double)total;
    *(float *)ef_guest_memory_ptr(0x00197670u) = (float)(EF_F64(0x001809b3u) - ratio * EF_F64(0x001809b3u));
    EF_U32(0x001379b0u) = base;
    EF_U16(0x001a53c0u) = (uint16_t)count_total;
}

/* 0x16bac, 205 bytes. Pick the best-rated weapon among a record's carried ones, returned scaled. */
double pick_best_weapon_16bac(int32_t record_index, ef_addr32 kind_out, ef_addr32 id_out)
{
    int32_t best = -1;
    uint32_t k;

    for (k = 0; k != 14; k += 2) {
        ef_addr32 rec = EF_U32(0x0019bb58u) + (uint32_t)record_index * 0xcau + k;
        uint32_t id = EF_U16(rec + 0x50u);
        ef_addr32 obj;
        uint32_t n;

        if (id == 0xffu || (uint16_t)id == 0)
            continue;
        obj = guest_fn_7489c((int16_t)id);
        if (obj == 0)
            continue;
        for (n = 0; n < 20; n++) {
            ef_addr32 slot = obj + 0x19bu + n * 3u;
            int32_t kind = (int8_t)EF_U8(slot);
            int32_t rating;

            if (EF_S16(slot + 1u) <= 0)
                continue;
            if ((uint32_t)kind < 1u)
                continue;
            if ((uint32_t)kind > 5u && ((uint32_t)kind < 0x11u || (uint32_t)kind > 0x13u))
                continue;
            rating = EF_S32((uint32_t)kind * 0x91u + 0x0012bca3u);
            if (rating > best) {
                best = rating;
                EF_U32(kind_out) = (uint32_t)kind;
                EF_U16(id_out) = (uint16_t)id;
            }
        }
    }
    return (double)best * EF_F64(0x001817f3u);
}
