/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

#include <string.h>

static uint16_t read16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t read32(const uint8_t *p)
{
    return (uint32_t)read16(p) | ((uint32_t)read16(p + 2) << 16);
}

static void write16(ef_addr32 address, uint16_t value)
{
    uint8_t *p = (uint8_t *)ef_guest_memory_ptr(address);
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void write_double(ef_addr32 address, double value)
{
    memcpy(ef_guest_memory_ptr(address), &value, sizeof(value));
}

/* 0x53308, 19 bytes. FILD reads the original low signed *word* of EAX,
 * while FMUL uses a 32-bit floating-point guest constant. ST0 is the output. */
double convert_signed_short_scale(int16_t value)
{
    return (double)value * (double)g_short_to_float_scale;
}

/* 0x855c8, 57 bytes. Unlike other grid consumers, the row argument is the
 * full 32-bit EAX. Count is an unsigned byte and clock stride is 25 bytes. */
uint8_t grid_clock_reaches_reference(int32_t row)
{
    const ef_world_grid_cell *cell = (const ef_world_grid_cell *)
        ef_guest_memory_ptr(g_world_grid_base + (uint32_t)row * 0xcau);
    return reference_clock_reaches(cell->clock_base +
                                   (uint32_t)cell->activity_countdown * 25u + 16u);
}

/* 0x88100, 50 bytes. Object+0x56 is first invalidated with 0x00ff (not
 * 0xffff); it becomes the child's signed index only when filtered lookup
 * returns exactly the same child pointer. Null parent leaves everything. */
void refresh_object_child_slot(ef_addr32 object)
{
    uint8_t *p;
    ef_addr32 child;
    int16_t index;
    if (object == 0) { return; }
    p = (uint8_t *)ef_guest_memory_ptr(object);
    child = read32(p + 0x30);
    write16(object + 0x56, 0x00ff);
    if (child == 0) { return; }
    index = (int16_t)read16((uint8_t *)ef_guest_memory_ptr(child + 0x16));
    if (lookup_filtered_indexed_object(index) == child) {
        write16(object + 0x56, (uint16_t)index);
    }
}

/* 0x860c0, 80 bytes. AX is sign extended for filtered lookup. A missing
 * object, or all nonzero slots coded 0x21/0x22, returns 20. */
uint8_t first_nonignored_activity_slot(int16_t object)
{
    ef_addr32 ptr = lookup_filtered_indexed_object(object);
    const uint8_t *base;
    unsigned i;
    if (!ptr) { return 20; }
    base = (const uint8_t *)ef_guest_memory_ptr(ptr);
    for (i = 0; i < 20; ++i) {
        const uint8_t *p = base + 0x19b + i * 3;
        if (read16(p + 1) != 0 && p[0] != 0x21 && p[0] != 0x22) {
            return (uint8_t)i;
        }
    }
    return 20;
}

/* 0x2949c, 79 bytes. The stored code is SAR 24 on a dword, hence signed;
 * the caller's DL code is zero-extended. Codes >= 0x80 never compare equal. */
uint8_t query_indexed_object_feature(int16_t object, uint8_t code)
{
    ef_addr32 ptr = lookup_filtered_indexed_object(object);
    const uint8_t *base = (const uint8_t *)ef_guest_memory_ptr(ptr);
    unsigned i;
    for (i = 0; i < 20; ++i) {
        const uint8_t *p = base + 0x19b + i * 3;
        if (read16(p + 1) != 0 && (int8_t)p[0] == (int32_t)code) {
            return (uint8_t)i;
        }
    }
    return 20;
}

/* 0x80abc, 87 bytes. IDIV 16 yields a signed remainder. The existing
 * clear_panel_strip draft clears its own metadata; this overwrites its
 * sentinel with the incremented cursor at +0 of the selected 12-byte band. */
uint16_t advance_panel_strip(void)
{
    uint16_t next = (uint16_t)(g_panel_sequence_cursor + 1u);
    int32_t band = (int16_t)next % 16;
    g_panel_sequence_cursor = next;
    (void)clear_panel_strip((int16_t)band);
    write16((uint32_t)(0x001fc8a2 + band * 12), next);
    return next;
}

/* 0x88c80, 80 bytes. Output order is child offsets +8,+0,+4, not struct
 * order. Two consecutive x87 multiplies use common double constants;
 * exact intermediate extended precision remains guest-differential work. */
void transform_child_coordinates(ef_addr32 object, ef_addr32 output)
{
    uint8_t *p = (uint8_t *)ef_guest_memory_ptr(object);
    ef_addr32 child = read32(p + 0x30);
    const uint8_t *c;
    const unsigned offsets[3] = {8, 0, 4};
    unsigned i;
    if (child == 0) { return; }
    c = (const uint8_t *)ef_guest_memory_ptr(child);
    g_last_transform_child = child;
    if (read32(c + 4) == 0x80000000u) { p[0x5a] = 1; }
    for (i = 0; i < 3; ++i) {
        double value = (double)(int32_t)read32(c + offsets[i]);
        write_double(output + i * 8u,
                     (value * g_transform_scale_a) * g_transform_scale_b);
    }
}

/* 0x88d4c, 55 bytes. The first word comes from high half of an unaligned
 * dword at +0xa and is negated; the second comes from signed word +0xe. */
void transform_signed_angle_pair(ef_addr32 object, ef_addr32 output_a,
                                 ef_addr32 output_b)
{
    const uint8_t *p = (const uint8_t *)ef_guest_memory_ptr(object);
    int32_t first = -(int32_t)(int16_t)read16(p + 0xc);
    int32_t second = (int16_t)read16(p + 0xe);
    write_double(output_a, ((double)first * g_transform_scale_c) * g_transform_scale_d);
    write_double(output_b, ((double)second * g_transform_scale_c) * g_transform_scale_d);
}
