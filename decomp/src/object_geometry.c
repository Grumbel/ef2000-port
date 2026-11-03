/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

#include <math.h>

static int16_t high_word(int32_t packed)
{
    return (int16_t)((uint32_t)packed >> 16);
}

/* 0x0006b3d0, size 0x4f. Original callers: 0x239ac, 0x294ec,
 * 0x6c710, 0x6d948, 0x7d2f8. AL is masked to a byte before indexing
 * a table with 0xe5-byte records. */
uint8_t object_kind_is_eligible(uint8_t kind)
{
    uint8_t mode = g_object_kind_modes[(uint32_t)kind * 0xe5u];
    if (mode == 2) {
        return (uint8_t)(kind == 0x12 || kind == 0x16);
    }
    if (mode == 3) {
        return 0;
    }
    if (mode == 4) {
        return (uint8_t)(kind != 0x1d && kind != 0x1e);
    }
    return 1;
}

/* 0x0006b4ac, size 0x9f. Callers 0x1851c/0x770d4 plus recovered
 * 0x6b54c/0x6b58c. The two feature queries deliberately use 0x1e and
 * 0x1d in this order, each skipping its contributions only at 0x14. */
double object_type_limit(ef_addr32 object)
{
    const ef_motion_object *obj = (const ef_motion_object *)ef_guest_memory_ptr(object);
    const ef_grid_type_record *type = &g_grid_type_records[obj->class_code];
    int16_t index = high_word(obj->index_packed);
    long double limit = type->limit_a;
    if (query_indexed_object_feature(index, 0x1e) != 0x14) {
        limit += type->limit_b;
    }
    if (query_indexed_object_feature(index, 0x1d) != 0x14) {
        limit += type->limit_multiplier;
        limit += type->limit_extra;
    }
    return (double)limit;
}

/* 0x0006b54c, size 0x3d. Independently called from 0x2a168,
 * 0x2a1c4 and 0x6d38c. Strict comparison: equal/NaN never updates. */
uint8_t lower_object_limit(ef_addr32 object)
{
    ef_motion_object *obj = (ef_motion_object *)ef_guest_memory_ptr(object);
    double limit = object_type_limit(object);
    if (obj->current_limit > limit) {
        obj->current_limit = limit;
        return 1;
    }
    return 0;
}

/* 0x0006b58c, size 0x1a. Original caller functions 0x2ce18,
 * 0x6d38c, 0x8c49c and 0x8cb60. A rejected lookup does nothing. */
void reset_object_limit(int16_t object)
{
    ef_addr32 address = lookup_filtered_indexed_object(object);
    if (address != 0) {
        ef_motion_object *obj = (ef_motion_object *)ef_guest_memory_ptr(address);
        obj->current_limit = object_type_limit(address);
    }
}

static int32_t clock_seconds(const uint8_t *clock)
{
    const uint16_t *word = (const uint16_t *)clock;
    return (int32_t)(int16_t)word[0] * 3600 +
           (int32_t)(int16_t)word[1] * 60 + (int16_t)word[2];
}

/* 0x0006b5a8, size 0x5d. Original callers 0x2ced4 and 0x6e59c
 * pass EAX/EDX as two-point pointers and EBX as a 6-byte clock.
 * 0x40774 converts three signed clock words to seconds. The elapsed
 * time is clamped to at least one second before the 2D distance divide. */
double object_speed_from_clock(ef_addr32 a, ef_addr32 b, ef_addr32 clock)
{
    const double *point_a = (const double *)ef_guest_memory_ptr(a);
    const double *point_b = (const double *)ef_guest_memory_ptr(b);
    int32_t elapsed = clock_seconds(g_reference_clock) -
                      clock_seconds((const uint8_t *)ef_guest_memory_ptr(clock));
    double dx = (double)((long double)point_a[0] - point_b[0]);
    double dy = (double)((long double)point_a[1] - point_b[1]);
    if (elapsed < 1) {
        elapsed = 1;
    }
    return (double)(sqrt((double)((long double)dx * dx + (long double)dy * dy)) /
                    (long double)elapsed);
}

/* 0x0006a5f0, size 0x79. Nine distinct caller functions. Both indexed
 * objects' +0x49/+0x51/+0x59 doubles are subtracted and stored, but the
 * original distance callee 0xb415c reads only the first two differences.
 * The third coordinate cannot affect the returned ST0 value. */
double indexed_object_distance(int16_t a, int16_t b)
{
    ef_addr32 aa = lookup_indexed_object(a);
    ef_addr32 bb = lookup_indexed_object(b);
    const uint8_t *first;
    const uint8_t *second;
    double diff[3];
    unsigned i;
    if (aa == 0 || bb == 0) {
        return 0;
    }
    first = (const uint8_t *)ef_guest_memory_ptr(aa);
    second = (const uint8_t *)ef_guest_memory_ptr(bb);
    for (i = 0; i < 3; ++i) {
        const double *x = (const double *)(first + 0x49 + i * 8);
        const double *y = (const double *)(second + 0x49 + i * 8);
        diff[i] = (double)((long double)*x - *y);
    }
    return sqrt((double)((long double)diff[0] * diff[0] +
                         (long double)diff[1] * diff[1]));
}
