/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_environment_090.h"
#include "ef_mem.h"

#include <stdint.h>

/* The original 0xdf26f parsing call and later engine callees still cross
 * explicit host boundaries. `number[]` denotes the parsed output slots in
 * the original stack/global layout, not a guessed Watcom register ABI. */

/* 0xda1b4 size 112: fade mode and signed depth forwarded to the engine. */
void env_darkside_fade_da1b4(void)
{
    ef_environment_fields_090 f;
    env_scan_line_090(0xda1b4u, 0x18ff6cu, &f);
    EF_U16(0x17d3c4u) = (uint16_t)f.number[0];
    EF_U16(0x12afd8u) = EF_U16(0x17d3c4u);
    (void)env_engine_call_090(0xc486cu, (uint32_t)(int32_t)(int16_t)f.number[1], 0u, 0u);
}

/* 0xda224 size 138: table index and validity check. */
void env_coltab_da224(void)
{
    ef_environment_fields_090 f;
    uint32_t valid;
    env_scan_line_090(0xda224u, 0x18ff74u, &f);
    EF_U16(0x17d3c4u) = (uint16_t)f.number[0];
    EF_U16(0x12afd8u) = EF_U16(0x17d3c4u);
    valid = env_engine_call_090(0xe7ea4u, f.number[1], f.number[2], 0u);
    if (valid == 0u)
        env_report_090(0x18ff80u);
}

/* 0xda2b8 size 168: append a cloud altitude/type entry. The diagnostic
 * at count >=1 does not terminate the original routine. */
void env_altitude_da2b8(void)
{
    ef_environment_fields_090 f;
    uint32_t index = EF_U32(0x425b18u);
    if ((int32_t)index >= 1)
        env_report_090(0x18ffc4u);
    env_scan_line_090(0xda2b8u, 0x18ffe8u, &f);
    EF_U32(0x425b14u + index * 4u) = 0u - f.number[0];
    /* Original 0x9d749 copies the parsed name to a 20-byte slot. */
    (void)env_engine_call_090(0x9d749u, 0x425b00u + index * 20u,
                              f.number[1], 0u);
    EF_U32(0x425b18u) = index + 1u;
}

/* 0xda360 size 167: upside-down altitude, optional second negative word. */
void env_altitude2_da360(void)
{
    ef_environment_fields_090 f;
    env_scan_line_090(0xda360u, 0x18fff0u, &f);
    EF_U16(0x425bc8u) = 1u;
    EF_U16(0x425bd0u) = (uint16_t)(0u - f.number[0]);
    EF_U16(0x425baau) = (uint16_t)f.number[1];
    if (EF_U16(0x425baau) != 0u)
        EF_U16(0x425baau) = (uint16_t)(0u - EF_U16(0x425baau));
    (void)env_engine_call_090(0x9ca57u, 0x425490u, f.number[2], 0u);
}

/* 0xda408 size 167: companion path for ground level/texture. */
void env_ground_da408(void)
{
    ef_environment_fields_090 f;
    env_scan_line_090(0xda408u, 0x18fff0u, &f);
    EF_U16(0x425ba6u) = 1u;
    EF_U16(0x425baeu) = (uint16_t)(0u - f.number[0]);
    EF_U16(0x425bb6u) = (uint16_t)f.number[1];
    if (EF_U16(0x425bb6u) != 0u)
        EF_U16(0x425bb6u) = (uint16_t)(0u - EF_U16(0x425bb6u));
    (void)env_engine_call_090(0x9ca57u, 0x422348u, f.number[2], 0u);
}

/* 0xda4b0 size 102: one negative ceiling and a presence flag. */
void env_ceiling_da4b0(void)
{
    ef_environment_fields_090 f;
    env_scan_line_090(0xda4b0u, 0x18fffcu, &f);
    EF_U16(0x425bbeu) = 1u;
    EF_U16(0x425ba8u) = (uint16_t)(0u - f.number[0]);
}

/* 0xda518 size 102: one negative planet index and a presence flag. */
void env_planet_number_da518(void)
{
    ef_environment_fields_090 f;
    env_scan_line_090(0xda518u, 0x18fffcu, &f);
    EF_U16(0x425bc0u) = 1u;
    EF_U16(0x425bacu) = (uint16_t)(0u - f.number[0]);
}

/* 0xda580 size 74: one 16-bit game-type value (format "%hd") directly
 * to a global, not a string despite the original function's name. */
void env_game_type_da580(void)
{
    ef_environment_fields_090 f;
    env_scan_line_090(0xda580u, 0x18fffcu, &f);
    EF_U16(0x425be4u) = (uint16_t)f.number[0];
}
