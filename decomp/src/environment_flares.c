/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/* Sun-flare environment script keywords for the hybrid build. Kept apart
 * from environment_world_090.c, which a host unit test links without the
 * guest globals and allocator these use. */

#include "ef_abi.h"
#include "ef_environment_090.h"
#include "ef_globals.h"
#include "ef_structs/sun_flare.h"
#include "ef_mem.h"

/* 0xdb4d4, 383 bytes, the "sun_flare" script keyword (keyword table
 * 0x15bc78; the tracker called it tmsun_flare). Read six "%hd" values
 * from the next script line, allocate an entry with room for the
 * allocation-time count of point pairs and push it on the flare list.
 * The count is then capped at 10 and that many points are placed
 * evenly around a circle of the radius value (sine table at 0x141e08,
 * cosine a quarter turn later). */
void env_sun_flare_db4d4(void)
{
    int16_t radius = 0;
    int16_t value2 = 0;
    int16_t colour = 0;
    int16_t value4 = 0;
    int16_t value5 = 0;
    int16_t count = 0;
    ef_addr32 flare;
    ef_addr32 points;
    int16_t i;

    guest_fn_df26f_x(env_take_line_090(), 0x0019035cu,       /* "%hd %hd %hd %hd %hd %hd" */
                     (uintptr_t)&radius, (uintptr_t)&value2, (uintptr_t)&colour,
                     (uintptr_t)&value4, (uintptr_t)&value5, (uintptr_t)&count);
    flare = guest_fn_e29e8((uint32_t)((int32_t)count * 2 + (int32_t)sizeof(ef_sun_flare)));
    EF_U32(flare + offsetof(ef_sun_flare, next)) = g_sun_flares;
    g_sun_flares = flare;
    if (count > 10)
        count = 10;
    colour = colour < 0 ? (int16_t)-colour : (int16_t)(colour + 0x20);
    EF_U8(flare + offsetof(ef_sun_flare, radius)) = (uint8_t)radius;
    EF_U8(flare + offsetof(ef_sun_flare, value2)) = (uint8_t)value2;
    EF_U8(flare + offsetof(ef_sun_flare, colour)) = (uint8_t)colour;
    EF_U16(flare + offsetof(ef_sun_flare, value4)) = (uint16_t)value4;
    EF_U16(flare + offsetof(ef_sun_flare, value5)) = (uint16_t)value5;
    EF_U16(flare + offsetof(ef_sun_flare, textured)) = 0u;
    EF_U8(flare + offsetof(ef_sun_flare, count)) = (uint8_t)count;
    points = flare + (uint32_t)sizeof(ef_sun_flare);
    for (i = 0; i < count; ++i) {
        uint32_t angle = (uint32_t)(uint16_t)(((int32_t)i * 0xffff) / (int32_t)count) >> 3;

        EF_U8(points + (uint32_t)i * 2u) = (uint8_t)(((int32_t)g_sine_table[angle] * radius) >> 15);
        EF_U8(points + (uint32_t)i * 2u + 1u) =
            (uint8_t)(((int32_t)-radius * (int32_t)g_sine_table[angle + 0x800u]) >> 15);
    }
}

/* 0xdb654, 218 bytes, the "tmsun_flare" script keyword (keyword table
 * 0x15bc80; the tracker called it sun_texture). Read five "%hd" values
 * and push a textured flare entry, without points, on the flare list.
 * A texture number past the sun textures read so far is reported
 * through the error hook, after the entry is already linked. */
void env_tmsun_flare_db654(void)
{
    int16_t radius;
    int16_t value2;
    int16_t texture;
    int16_t value4;
    int16_t value5;
    ef_addr32 flare;

    guest_fn_df26f_x(env_take_line_090(), 0x00190374u,       /* "%hd %hd %hd %hd %hd" */
                     (uintptr_t)&radius, (uintptr_t)&value2, (uintptr_t)&texture,
                     (uintptr_t)&value4, (uintptr_t)&value5);
    flare = guest_fn_e29e8((uint32_t)sizeof(ef_sun_flare));
    EF_U32(flare + offsetof(ef_sun_flare, next)) = g_sun_flares;
    g_sun_flares = flare;
    if (texture >= g_sun_texture_count)
        guest_callptr2_x(g_gui_error_hook, 0x00190388u,      /* "Unknown flare texture id %hd" */
                         (uint32_t)(int32_t)texture);
    EF_U8(flare + offsetof(ef_sun_flare, radius)) = (uint8_t)radius;
    EF_U8(flare + offsetof(ef_sun_flare, value2)) = (uint8_t)value2;
    EF_U16(flare + offsetof(ef_sun_flare, texture)) = (uint16_t)texture;
    EF_U16(flare + offsetof(ef_sun_flare, value4)) = (uint16_t)value4;
    EF_U16(flare + offsetof(ef_sun_flare, textured)) = 1u;
    EF_U16(flare + offsetof(ef_sun_flare, value5)) = (uint16_t)value5;
}
