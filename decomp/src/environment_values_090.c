/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_environment_090.h"
#include "ef_mem.h"

/* Give the original scanner its output addresses in format order. Scanning
 * directly into game RAM retains prior values when a conversion fails. */
void env_darkside_fade_da1b4(void)
{
    int32_t depth;
    guest_fn_df26f_x(env_take_line_090(), 0x18ff6cu,
                     (uintptr_t)&depth, (uintptr_t)0x17d3c4u);
    EF_U16(0x12afd8u) = EF_U16(0x17d3c4u);
    guest_set_scale_pair_x((uint32_t)(int32_t)(int16_t)depth);
}

void env_coltab_da224(void)
{
    int32_t lower, upper;
    guest_fn_df26f_x(env_take_line_090(), 0x18ff74u,
                     (uintptr_t)&upper, (uintptr_t)&lower, (uintptr_t)0x17d3c4u);
    EF_U16(0x12afd8u) = EF_U16(0x17d3c4u);
    if (build_ramp_table_e7ea4(lower, upper) == 0u)
        guest_callptr1_x(EF_U32(0x132a74u), 0x18ff80u);
}

/* Reporting a full altitude table does not cancel the append. */
void env_altitude_da2b8(void)
{
    char name[80];
    uint32_t altitude;
    uint32_t index;

    if (EF_S32(0x425b18u) >= 1)
        guest_callptr1_x(EF_U32(0x132a74u), 0x18ffc4u);
    guest_fn_df26f_x(env_take_line_090(), 0x18ffe8u,
                     (uintptr_t)&altitude, (uintptr_t)name);
    index = EF_U32(0x425b18u);
    EF_U32(0x425b14u + index * 4u) = 0u - altitude;
    copy_string_9d749(0x425b00u + index * 20u, (ef_addr32)(uintptr_t)name);
    ++EF_U32(0x425b18u);
}

static void parse_surface(ef_addr32 flag, ef_addr32 height,
                          ef_addr32 extra_height, ef_addr32 texture)
{
    char suffix[80];
    ef_addr32 line = env_take_line_090();

    EF_U16(flag) = 1u;
    guest_fn_df26f_x(line, 0x18fff0u, (uintptr_t)height,
                     (uintptr_t)extra_height, (uintptr_t)suffix);
    EF_U16(height) = (uint16_t)(0u - EF_U16(height));
    if (EF_U16(extra_height) != 0u)
        EF_U16(extra_height) = (uint16_t)(0u - EF_U16(extra_height));
    strcat_9ca57(texture, (ef_addr32)(uintptr_t)suffix);
}

void env_altitude2_da360(void)
{
    parse_surface(0x425bc8u, 0x425bd0u, 0x425baau, 0x425490u);
}

void env_ground_da408(void)
{
    parse_surface(0x425ba6u, 0x425baeu, 0x425bb6u, 0x422348u);
}

static void parse_negative_word(ef_addr32 flag, ef_addr32 destination)
{
    ef_addr32 line = env_take_line_090();

    EF_U16(flag) = 1u;
    guest_fn_df26f_x(line, 0x18fffcu, (uintptr_t)destination);
    EF_U16(destination) = (uint16_t)(0u - EF_U16(destination));
}

void env_ceiling_da4b0(void)
{
    parse_negative_word(0x425bbeu, 0x425ba8u);
}

void env_planet_number_da518(void)
{
    parse_negative_word(0x425bc0u, 0x425bacu);
}

void env_game_type_da580(void)
{
    guest_fn_df26f_x(env_take_line_090(), 0x18fffcu, (uintptr_t)0x425be4u);
}
