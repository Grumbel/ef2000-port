/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

/* 0x88d30, 0x19 bytes, nine caller functions. Stores are interleaved with
 * loads, preserving the original order when output aliases either input. */
void subtract_scene_vectors(ef_addr32 output, ef_addr32 subtract, ef_addr32 from)
{
    for (unsigned i = 0; i < 3; ++i) {
        ef_addr32 off = i * 8u;
        double a = *(const double *)ef_guest_memory_ptr(from + off);
        double b = *(const double *)ef_guest_memory_ptr(subtract + off);
        *(double *)ef_guest_memory_ptr(output + off) = a - b;
    }
}

/* 0x88d84, 0x0c bytes, three caller functions. Signed integer at +0x26
 * becomes a double scaled by the original global. */
void convert_scene_scalar(ef_addr32 source, ef_addr32 output)
{
    int32_t value = *(const int32_t *)ef_guest_memory_ptr(source + 0x26u);
    *(double *)ef_guest_memory_ptr(output) = (double)value * g_scene_scalar_scale;
}

/* Both converters load signed dwords from [8,0,4], and emit doubles at
 * [0,8,16]. The first multiply uses x87 extended precision, so the host
 * C model remains only a structural draft pending a guest differential. */
static void convert_vector(ef_addr32 source, ef_addr32 output,
                           double first_scale, double second_scale)
{
    const unsigned input_offsets[3] = {8, 0, 4};
    for (unsigned i = 0; i < 3; ++i) {
        int32_t value = *(const int32_t *)ef_guest_memory_ptr(source + input_offsets[i]);
        *(double *)ef_guest_memory_ptr(output + i * 8u) =
            ((double)value * first_scale) * second_scale;
    }
}

/* 0x88c54, 0x2b bytes, eight caller functions. */
void convert_scene_vector(ef_addr32 source, ef_addr32 output)
{
    convert_vector(source, output, g_scene_vector_scale_a, g_scene_vector_scale_b);
}

/* 0x8fa58, 0x2b bytes, three caller functions. The guest's two scale
 * globals at 0x18aa0b / 0x18aa13 are deliberately unaligned. */
void convert_navigation_vector(ef_addr32 source, ef_addr32 output)
{
    convert_vector(source, output,
                   g_navigation_vector_scale_a, g_navigation_vector_scale_b);
}
