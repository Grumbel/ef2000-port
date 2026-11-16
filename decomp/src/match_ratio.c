/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

#define MATCH_U16(a) (*(volatile uint16_t *)ef_guest_memory_ptr(a))
#define MATCH_U32(a) (*(volatile uint32_t *)ef_guest_memory_ptr(a))
#define MATCH_S32(a) (*(volatile int32_t *)ef_guest_memory_ptr(a))

/* The two dword counters are consumed as signed words by the original.
 * Retain x87 precision, exceptions and single final float rounding. */
static void store_match_ratio(const uint32_t *counts);
#if defined(__WATCOMC__) && !defined(__GNUC__) && !defined(__clang__)
#pragma aux store_match_ratio = \
    "fild word ptr [eax]" \
    "fild word ptr [eax+4]" \
    "fdivp st(1),st" \
    "fld qword ptr ds:[001809b3h]" \
    "fxch st(1)" \
    "fmul st,st(1)" \
    "fsubp st(1),st" \
    "fstp dword ptr ds:[00197670h]" \
    parm [eax] modify []
#else
static void store_match_ratio(const uint32_t *counts)
{
    long double ratio = (long double)(int16_t)counts[0]
        / (long double)(int16_t)counts[1];
    long double scale = *(double *)ef_guest_memory_ptr(0x001809b3u);
    *(float *)ef_guest_memory_ptr(0x00197670u) = (float)(scale - ratio * scale);
}
#endif

/* 0xc964,218 bytes. No inputs, EAX scratch, six registers preserved,
 * plain RET. Sole caller c27f discards EAX. */
void update_match_ratio_c964(void)
{
    int16_t group_count = (int16_t)MATCH_U16(0x001a53c0u);
    int32_t record_count = MATCH_S32(0x001379b0u);
    uint32_t counts[2];
    uint32_t group_index;
    uint32_t record_index;
    uint32_t offset;
    ef_addr32 rec;
    uint16_t selected;
    uint16_t record_group;

    counts[0] = 0u;
    counts[1] = 0u;
    for (group_index = 0u; (int16_t)group_index < group_count; group_index++) {
        offset = (uint32_t)(int32_t)(int16_t)group_index * 2u;
        if (MATCH_U16(0x00197320u + offset) != 0u)
            counts[1] += MATCH_U16(0x001a53b0u + offset);
    }
    for (record_index = 0u; (int32_t)(int16_t)record_index < record_count;
         record_index++) {
        rec = MATCH_U32(0x001379bcu)
            + (uint32_t)(int32_t)(int16_t)record_index * 54u;
        selected = MATCH_U16(0x00197696u);
        record_group = MATCH_U16(rec + 22u);
        if (record_group != selected)
            continue;
        for (group_index = 0u; (int16_t)group_index < group_count; group_index++) {
            offset = (uint32_t)(int32_t)(int16_t)group_index * 2u;
            record_group = MATCH_U16(rec + 24u);
            if (record_group == MATCH_U16(0x001a5280u + offset)
                && MATCH_U16(0x00197320u + offset) != 0u
                && (MATCH_S32(rec + 10u) >> 16) == 1)
                counts[0]++;
        }
    }
    store_match_ratio(counts);
    MATCH_U32(0x001379b0u) = (uint32_t)record_count;
    MATCH_U16(0x001a53c0u) = (uint16_t)group_count;
}
