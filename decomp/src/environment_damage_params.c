/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

#if defined(__WATCOMC__) && !defined(__GNUC__) && !defined(__clang__)
/* The guest sscanf wrapper takes three stack arguments (caller pops) and
 * saves/restores EBX, ECX and EDX itself, so only EAX is clobbered. */
#pragma aux environment_scan_value_087 parm caller [] modify [eax]
/* The shared body therefore also preserves every register but EAX. */
#pragma aux scan_parameter_line parm [eax] value [eax] modify [eax]
#endif

/* Common 74-byte body of 15 distinct, named X-32 entry points. All read
 * 0x425af8, split at LF or NUL and call the same guest sscanf at 0xdf26f,
 * with the 0x18fffc format and their own global destination. The callee's
 * EAX is preserved after its call and returned to the original caller.
 *
 * ORIGINAL BUG: After null-terminating at the LF/NUL, the original also
 * zeroes the preceding byte (after advancing the cursor). That handles
 * CRLF, but truncates ordinary LF-only lines and NUL-terminated final
 * lines by one character; do not normalize that behavior. The empty-line
 * case even writes to start-1, as in the original instruction stream. */
static uint32_t scan_parameter_line(ef_addr32 destination)
{
    ef_addr32 start = EF_U32(0x425af8u);
    ef_addr32 end = start;
    while (EF_U8(end) != 0u && EF_U8(end) != 10u)
        ++end;
    EF_U8(end) = 0u;
    ++end;
    EF_U8(end - 2u) = 0u;
    EF_U32(0x425af8u) = end;
    return environment_scan_value_087(start, 0x18fffcu, destination);
}

/* 0xdb95c, 74 bytes: craft-water impact parameter. */
uint32_t env_craft_hit_water_db95c(void)
{
    return scan_parameter_line(0x425bdcu);
}

/* 0xdb9a8, 74 bytes: craft-ground impact parameter. */
uint32_t env_craft_hit_ground_db9a8(void)
{
    return scan_parameter_line(0x425be0u);
}

/* 0xdb9f4, 74 bytes: cannon-concrete impact parameter. */
uint32_t env_cannon_hit_concrete_db9f4(void)
{
    return scan_parameter_line(0x425bdau);
}

/* 0xdba40, 74 bytes: missile impact parameter. */
uint32_t env_missile_hit_dba40(void)
{
    return scan_parameter_line(0x425bdeu);
}

/* 0xdba8c, 74 bytes: cannon-water impact parameter. */
uint32_t env_cannon_hit_water_dba8c(void)
{
    return scan_parameter_line(0x425beau);
}

/* 0xdbad8, 74 bytes: cannon-ground impact parameter. */
uint32_t env_cannon_hit_ground_dbad8(void)
{
    return scan_parameter_line(0x425beeu);
}

/* 0xdbb24, 74 bytes: missile-concrete impact parameter. */
uint32_t env_missile_hit_concrete_dbb24(void)
{
    return scan_parameter_line(0x425bd4u);
}

/* 0xdbb70, 74 bytes: bomb impact parameter. */
uint32_t env_bomb_hit_dbb70(void)
{
    return scan_parameter_line(0x425be6u);
}

/* 0xdbbbc, 74 bytes: missile-water impact parameter. */
uint32_t env_missile_hit_water_dbbbc(void)
{
    return scan_parameter_line(0x425becu);
}

/* 0xdbc08, 74 bytes: missile-ground impact parameter. */
uint32_t env_missile_hit_ground_dbc08(void)
{
    return scan_parameter_line(0x425bd6u);
}

/* 0xdbc54, 74 bytes: bomb-concrete impact parameter. */
uint32_t env_bomb_hit_concrete_dbc54(void)
{
    return scan_parameter_line(0x425bd8u);
}

/* 0xdbca0, 74 bytes: craft-building impact parameter. */
uint32_t env_craft_hit_building_dbca0(void)
{
    return scan_parameter_line(0x425bd2u);
}

/* 0xdbcec, 74 bytes: bomb-water impact parameter. */
uint32_t env_bomb_hit_water_dbcec(void)
{
    return scan_parameter_line(0x425be2u);
}

/* 0xdbd38, 74 bytes: bomb-ground impact parameter. */
uint32_t env_bomb_hit_ground_dbd38(void)
{
    return scan_parameter_line(0x425be8u);
}

/* 0xdbd84, 74 bytes: depth fading parameter. */
uint32_t env_depth_fade_dbd84(void)
{
    return scan_parameter_line(0x425bceu);
}
