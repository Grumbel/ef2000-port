/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* Keep the original FILD/FMUL result in ST0 without a double spill. */
static double scale_weapon_rating(const int32_t *rating);
#if defined(__WATCOMC__) && !defined(__GNUC__) && !defined(__clang__)
#pragma aux scale_weapon_rating = "fild dword ptr [eax]" \
    "fmul qword ptr ds:[001817f3h]" \
    parm [eax] value [8087] modify []
#else
static double scale_weapon_rating(const int32_t *rating)
{
    return (double)*rating * *(double *)ef_guest_memory_ptr(0x001817f3u);
}
#endif

/* 0x16bac,205 bytes. EAX index, EDX kind output, EBX id output;
 * ST0 result, ECX/ESI/EDI/EBP preserved, plain RET. */
double pick_best_weapon_16bac(int32_t record_index, ef_addr32 kind_out,
                             ef_addr32 id_out)
{
    uint32_t record_offset = (uint32_t)record_index * 202u;
    uint32_t slot_offset;
    uint32_t index;
    ef_addr32 unit;
    ef_addr32 entry;
    uint16_t id;
    uint32_t kind;
    int32_t rating;
    int32_t best = -1;

    for (slot_offset = 0u; slot_offset != 14u; slot_offset += 2u) {
        id = EF_U16(EF_U32(0x0019bb58u) + record_offset + slot_offset + 80u);
        if (id == 255u || id == 0u)
            continue;
        unit = guest_fn_7489c((int32_t)(int16_t)id);
        if (unit == 0u)
            continue;
        entry = unit + 411u;
        for (index = 0u; index < 20u; index++, entry += 3u) {
            if (EF_S16(entry + 1u) <= 0)
                continue;
            kind = (uint32_t)(int32_t)(int8_t)EF_U8(entry);
            if (!((kind >= 1u && kind <= 5u)
                  || (kind >= 17u && kind <= 19u)))
                continue;
            rating = EF_S32(0x0012bca3u + kind * 145u);
            if (rating > best) {
                best = rating;
                EF_U32(kind_out) = kind;
                EF_U16(id_out) = id;
            }
        }
    }
    return scale_weapon_rating(&best);
}
