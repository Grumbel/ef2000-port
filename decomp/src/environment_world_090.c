/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_environment_090.h"
#include "ef_mem.h"

void env_load_missile_palette_da7fc(void)
{
    static const ef_addr32 palette[][2] = {
        { 0x424b60u, 0x433f30u }, { 0x422460u, 0x437f34u },
        { 0x4257e0u, 0x43bf38u }, { 0x4254e0u, 0x43ff3cu },
        { 0x424e90u, 0x443f40u }
    };
    char name[80];
    uint16_t index;

    guest_fn_df26f_x(env_take_line_090(), 0x190000u, (uintptr_t)name);
    index = EF_U16(0x425ba0u);
    if (index < sizeof(palette) / sizeof(palette[0]))
        load_companion_files_dbf3c(palette[index][0], (ef_addr32)(uintptr_t)name,
                                   palette[index][1]);
    ++EF_U16(0x425ba0u);
}

void env_world_name_da964(void)
{
    int32_t planet, flags;
    guest_fn_df26f_x(env_take_line_090(), 0x1900dcu,
                     (uintptr_t)0x425b94u, (uintptr_t)0x425b90u,
                     (uintptr_t)&planet, (uintptr_t)&flags);
    EF_U16(0x425bc6u) = (uint16_t)planet;
    EF_U16(0x425bccu) = (uint16_t)flags;
}

static void parse_map_filename(ef_addr32 destination, ef_addr32 missing_message)
{
    char name[80];
    ef_addr32 filename = (ef_addr32)(uintptr_t)name;
    guest_fn_df26f_x(env_take_line_090(), 0x190000u, (uintptr_t)name);
    if (strlen_ds(filename) == 0u)
        guest_callptr1_x(EF_U32(0x132a74u), missing_message);
    copy_string_9d749(destination, filename);
    strcat_9ca57(destination, 0x190118u);
}

void env_xmap_name_da9d4(void)
{
    parse_map_filename(0x425bf0u, 0x1900e8u);
}

void env_mmap_name_daa5c(void)
{
    char name[80];
    int16_t index;
    ef_addr32 filename = (ef_addr32)(uintptr_t)name;
    ef_addr32 destination;

    guest_fn_df26f_x(env_take_line_090(), 0x190120u,
                     (uintptr_t)&index, (uintptr_t)name);
    if (strlen_ds(filename) == 0u)
        guest_callptr1_x(EF_U32(0x132a74u), 0x190128u);
    if (index < 0 || index >= 5)
        guest_callptr2_x(EF_U32(0x132a74u), 0x190168u, (uint32_t)(int32_t)index);
    destination = 0x425bf0u + (uint32_t)(int32_t)index * 25u;
    copy_string_9d749(destination, filename);
    strcat_9ca57(destination, 0x190118u);
}

void env_ref_point_dab40(void)
{
    parse_map_filename(0x425c6du, 0x1901acu);
}

void env_map_name_dabc8(void)
{
    char name[80];
    ef_addr32 filename = (ef_addr32)(uintptr_t)name;
    guest_fn_df26f_x(env_take_line_090(), 0x190000u, (uintptr_t)name);
    uppercase_string(filename);
    copy_string_9d749(0x425b28u, filename);
}

void env_load_player_virtual_db0b4(void)
{
    char name[80];
    guest_fn_df26f_x(env_take_line_090(), 0x190000u, (uintptr_t)name);
    init_record_pool_80d8c((ef_addr32)(uintptr_t)name);
    /* The original loads DH with 0xff and the initializer preserves EDX. */
    EF_U8(0x15bb5cu) = 0xffu;
}

void env_load_player_sort_db114(void)
{
    char name[80];
    guest_fn_df26f_x(env_take_line_090(), 0x190000u, (uintptr_t)name);
    nop_392fc();
}

void env_load_player_db16c(void)
{
    char name[80];
    guest_fn_df26f_x(env_take_line_090(), 0x190000u, (uintptr_t)name);
    store_word_39300((ef_addr32)(uintptr_t)name);
}

void env_set_flak_count_db1c4(void)
{
    char name[80];
    guest_fn_df26f_x(env_take_line_090(), 0x190000u, (uintptr_t)name);
    nop_3930c();
}

void env_load_flak_db21c(void)
{
    guest_fn_df26f_x(env_take_line_090(), 0x1902d8u, (uintptr_t)0x425afcu);
    if (EF_S32(0x425ae4u) > 20)
        guest_callptr1_x(EF_U32(0x132a74u), 0x1902dcu);
    clear_dwords_19bb38();
}

void env_cloud_fade_db420(void)
{
    ef_addr32 entry;
    ef_addr32 line;
    uint32_t index;

    if (EF_S32(0x15bb60u) == 20)
        guest_callptr1_x(EF_U32(0x132a74u), 0x190334u);
    line = env_take_line_090();
    entry = 0x421590u + EF_U32(0x15bb60u) * 20u;
    guest_fn_df26f_x(line, 0x19034cu, (uintptr_t)(entry + 16u), (uintptr_t)entry,
                     (uintptr_t)(entry + 4u), (uintptr_t)(entry + 8u),
                     (uintptr_t)(entry + 12u));
    index = EF_U32(0x15bb60u);
    EF_U32(0x15bb60u) = index + 1u;
    --EF_U32(0x4215a0u + index * 20u);
}
