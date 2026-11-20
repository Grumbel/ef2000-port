/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/* Environment script (READCOLS) keyword handlers for the hybrid build.
 * Each takes the next script line with env_take_line_090, scans it with
 * the original sscanf 0xdf26f and stores the values in fixed tables.
 * Errors go through the error hook at 0x132a74. The names are those of
 * the keyword table at 0x15bb80, which pairs each name with its handler;
 * the tracker had them one entry off. */

#include "ef_abi.h"
#include "ef_environment_090.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define MAX_LIGHTNING_AREAS 20
#define MAX_CRATER_TYPES EF_CRATER_TYPES
#define CRATER_RECORD_SIZE 110u
#define MAX_CLOUD_LEVELS 2
#define MAX_HEAVENLY_BODIES 40

/* 0xdad4c, 244 bytes, "lightning_point". The four coordinates are
 * stored scaled by 2048. A full table is reported, but the line is
 * still stored one entry past the end, as in the original. */
void env_lightning_point_dad4c(void)
{
    ef_addr32 line = env_take_line_090();
    int32_t bounds[4];
    int32_t value;
    ef_lightning_area *area;
    unsigned i;

    if (g_lightning_area_count == MAX_LIGHTNING_AREAS)
        guest_callptr1_x(g_gui_error_hook, 0x00190218u);  /* "Too many lightning areas" */
    guest_fn_df26f_x(line, 0x00190250u,                     /* "%ld,%ld,%ld,%ld,%i" */
                     (uintptr_t)&bounds[0], (uintptr_t)&bounds[1],
                     (uintptr_t)&bounds[2], (uintptr_t)&bounds[3],
                     (uintptr_t)&value);
    area = &g_lightning_areas[g_lightning_area_count];
    for (i = 0u; i < 4u; ++i)
        area->bounds[i] = (int32_t)((uint32_t)bounds[i] << 11);
    area->value = (int16_t)value;
    ++g_lightning_area_count;
}

/* 0xdae40, 179 bytes, "set_nb_craters". Scan the three counts straight
 * into their globals and allocate both 110-byte record arrays; too many
 * crater types is reported without stopping the allocation. */
void env_set_nb_craters_dae40(void)
{
    guest_fn_df26f_x(env_take_line_090(), 0x0018ff74u,      /* "%d %d %d" */
                     (uintptr_t)&g_crater_count, (uintptr_t)&g_crater_type_count,
                     (uintptr_t)&g_crater_slot_count);
    if (g_crater_type_count > MAX_CRATER_TYPES)
        guest_callptr1_x(g_gui_error_hook, 0x00190264u);  /* "Nb Crater types > limit" */
    g_crater_records = guest_fn_e29e8((uint32_t)g_crater_count * CRATER_RECORD_SIZE);
    g_crater_slots = guest_fn_e29e8((uint32_t)g_crater_slot_count * CRATER_RECORD_SIZE);
}

/* 0xdb730, 214 bytes, "cloud_fade". At most two cloud levels; a third
 * line is reported with the limit and otherwise ignored. */
void env_cloud_fade_db730(void)
{
    int32_t values[4];
    int32_t byte5;
    int32_t byte6;
    ef_cloud_level *level;
    unsigned i;

    guest_fn_df26f_x(env_take_line_090(), 0x001903a8u,      /* "%d %d %d %d %d %d" */
                     (uintptr_t)&values[0], (uintptr_t)&values[1],
                     (uintptr_t)&values[2], (uintptr_t)&values[3],
                     (uintptr_t)&byte5, (uintptr_t)&byte6);
    if (g_cloud_level_count >= MAX_CLOUD_LEVELS) {
        guest_callptr2_x(g_gui_error_hook, 0x001903bcu,     /* "Only %d cloud levels" */
                         MAX_CLOUD_LEVELS);
        return;
    }
    level = &g_cloud_levels[g_cloud_level_count];
    for (i = 0u; i < 4u; ++i)
        level->values[i] = values[i];
    level->bytes[0] = (uint8_t)byte5;
    level->bytes[1] = (uint8_t)byte6;
    ++g_cloud_level_count;
}

/* 0xdb808, 158 bytes, "load_flak". Read one "name slot" line per flak
 * type and register it with 0x2a7ac. The count is reread after each
 * line; an unset count (-1) is reported and reads no lines. */
void env_load_flak_db808(void)
{
    char name[80];
    int32_t slot;
    int32_t i;

    if (g_flak_type_count == -1)
        guest_callptr1_x(g_gui_error_hook, 0x00190400u);  /* "Nb Flak types not specified" */
    for (i = 0; i < g_flak_type_count; ++i) {
        guest_fn_df26f_x(env_take_line_090(), 0x00190004u,  /* "%s %d" */
                         (uintptr_t)name, (uintptr_t)&slot);
        init_entries_2a7ac((ef_addr32)(uintptr_t)name, (uint32_t)slot);
    }
}

/* 0xdb8bc, 160 bytes, "load_horizon". Store a heavenly body: the
 * negated angle, the current fix_horizon flag and the shape number that
 * 0x9ddfc finds for the name. Indices of 40 and above are ignored; the
 * signed check lets negative indices through, as in the original. */
void env_load_horizon_db8bc(void)
{
    int32_t index;
    int16_t angle;
    char name[80];
    ef_heavenly_body *body;

    guest_fn_df26f_x(env_take_line_090(), 0x0019043cu,      /* "%d %hd %s" */
                     (uintptr_t)&index, (uintptr_t)&angle, (uintptr_t)name);
    if (index >= MAX_HEAVENLY_BODIES)
        return;
    body = &g_heavenly_bodies[index];
    body->angle = (int16_t)-angle;
    body->horizon_fixed = g_horizon_fixed;
    body->shape = (int16_t)guest_lookup_value((ef_addr32)(uintptr_t)name);
}

/* Shared body of "load_cock_pal" (0xda748) and "load_missile_pal"
 * (0xda8c4), 158 bytes each. Build "\colours\<name>.<extension>" with
 * 0xe7cec, report a file the size hook cannot find, then load it into
 * DESTINATION whatever the check returned. */
static void load_palette_file(ef_addr32 extension, ef_addr32 missing_message,
                              ef_addr32 destination)
{
    char name[80];
    char path[80];

    guest_fn_df26f_x(env_take_line_090(), 0x00190000u, (uintptr_t)name);  /* "%s" */
    build_path_with_name_e7cec(0x00190064u, (ef_addr32)(uintptr_t)name,      /* "\colours\" */
                               extension, (ef_addr32)(uintptr_t)path);
    if (guest_callptr_eax_ret_x(g_resource_size_hook, (uint32_t)(uintptr_t)path) == 0u)
        guest_callptr1_x(g_gui_error_hook, missing_message);
    guest_callptr_eaxedx_x(g_resource_load_hook, (uint32_t)(uintptr_t)path, destination);
}

/* 0xda748, "load_cock_pal": the cockpit palette, "<name>.dat". */
void env_load_cock_pal_da748(void)
{
    load_palette_file(0x00190060u, 0x00190070u, 0x00421750u);
}

/* 0xda8c4, "load_missile_pal": the missile palette, "<name>.col". */
void env_load_missile_pal_da8c4(void)
{
    load_palette_file(0x001900a4u, 0x001900a8u, 0x00425190u);
}

/* 0xdb28c, 157 bytes, "heavenly_body". Scan a shape name and four
 * "%hd" values: values 2 and 3 straight into their globals (value 3
 * is cleared first, so a short line leaves it 0), value 4 squared and
 * value 5 as read. The shape number is looked up for "<name>.3". */
void env_heavenly_body_db28c(void)
{
    char name[20];
    int16_t size;
    int16_t value5;

    g_heavenly_value3 = 0;
    guest_fn_df26f_x(env_take_line_090(), 0x00190310u,      /* "%s %hd %hd %hd %hd" */
                     (uintptr_t)name, (uintptr_t)&g_heavenly_value2,
                     (uintptr_t)&g_heavenly_value3, (uintptr_t)&size,
                     (uintptr_t)&value5);
    strcat_9ca57((ef_addr32)(uintptr_t)name, 0x00190324u);    /* ".3" */
    g_heavenly_shape = (int16_t)load_shape_de864((ef_addr32)(uintptr_t)name);
    g_heavenly_value5 = value5;
    g_heavenly_size_squared = (int16_t)(size * size);
}

/* Report an empty name or one longer than LIMIT; the name is used
 * either way. */
static void check_name_length(ef_addr32 name, uint32_t limit, ef_addr32 message)
{
    if (strlen_ds(name) == 0u || strlen_ds(name) > limit)
        guest_callptr1_x(g_gui_error_hook, message);
}

/* 0xdbff8, 153 bytes, "coltab". Store "<name>.dat" (name of at most
 * eight characters) and load the colour table from it. */
void env_coltab_dbff8(void)
{
    char name[80];
    ef_addr32 text = (ef_addr32)(uintptr_t)name;

    guest_fn_df26f_x(env_take_line_090(), 0x00190000u, (uintptr_t)name);  /* "%s" */
    check_name_length(text, 8u, 0x001904d4u);                 /* "Invalid coltab name" */
    copy_string_9d749((ef_addr32)(uintptr_t)g_coltab_name, text);
    strcat_9ca57((ef_addr32)(uintptr_t)g_coltab_name, 0x00190504u);  /* ".dat" */
    load_colour_table_107310();
}

/* 0xdc094, 143 bytes, "init3d_file". Store the name (at most twelve
 * characters) and load the pattern file it names. */
void env_init3d_file_dc094(void)
{
    char name[80];
    ef_addr32 text = (ef_addr32)(uintptr_t)name;

    guest_fn_df26f_x(env_take_line_090(), 0x00190000u, (uintptr_t)name);  /* "%s" */
    check_name_length(text, 12u, 0x0019050cu);                /* "Invalid init3d name" */
    copy_string_9d749((ef_addr32)(uintptr_t)g_init3d_name, text);
    load_pattern_file_e4010((ef_addr32)(uintptr_t)g_init3d_name);
}

/* Load "\colours\<name>.col" into DESTINATION; the load result is not
 * checked. */
static void load_colour_file(ef_addr32 name, ef_addr32 destination)
{
    char path[80];

    build_path_with_name_e7cec(0x00190064u, name, 0x001900a4u,  /* "\colours\", "col" */
                               (ef_addr32)(uintptr_t)path);
    guest_callptr_eaxedx_x(g_resource_load_hook, (uint32_t)(uintptr_t)path, destination);
}

/* 0xdc124, 175 bytes, "east_west_palettes". Not scanned: the first
 * palette name runs up to a space (only the space is cleared), the
 * second is the rest of the line, taken like any other line. */
void env_east_west_palettes_dc124(void)
{
    ef_addr32 first = g_env_script_cursor;
    ef_addr32 end = first;

    while (EF_U8(end) != 0u && EF_U8(end) != (uint8_t)' ')
        ++end;
    EF_U8(end) = 0u;
    g_env_script_cursor = end + 1u;
    load_colour_file(first, 0x00420f90u);
    load_colour_file(env_take_line_090(), 0x00421290u);
    g_east_west_palettes_loaded = 1u;
}

#define GAME_TYPE_COUNT 14
#define PALETTE_COUNT 12

/* 0xda5cc, 119 bytes, "game_type". Point the sentinel entry after the
 * fourteen game types at the name just read, so the scan always ends;
 * an unknown name reaches the sentinel's handler, which reports it. The
 * sentinel keeps pointing at this stack buffer afterwards, as in the
 * original. */
void env_game_type_da5cc(void)
{
    char name[80];
    const ef_env_keyword *keyword = g_game_type_keywords;

    guest_fn_df26f_x(env_take_line_090(), 0x00190000u, (uintptr_t)name);  /* "%s" */
    g_game_type_keywords[GAME_TYPE_COUNT].name = (ef_addr32)(uintptr_t)name;
    while (guest_strcmp(keyword->name, (ef_addr32)(uintptr_t)name) != 0)
        ++keyword;
    guest_callptr0_x(keyword->handler);
}

/* 0xda644, 258 bytes, "load_pal". Load the named palette and its shade
 * table into the next of twelve banks and keep the line's value. The
 * first palette also becomes the current palette. Past twelve the
 * error hook gets the number and the limit; the count still grows. */
void env_load_pal_da644(void)
{
    char name[80];
    int32_t value;
    int16_t index;

    guest_fn_df26f_x(env_take_line_090(), 0x00190004u,      /* "%s %d" */
                     (uintptr_t)name, (uintptr_t)&value);
    index = g_palette_count;
    if (index == 0) {
        load_companion_files_dbf3c((ef_addr32)(uintptr_t)g_palette_banks[0],
                                   (ef_addr32)(uintptr_t)name,
                                   (ef_addr32)(uintptr_t)g_shade_tables[0]);
        memcpy_dc832((ef_addr32)(uintptr_t)g_current_palette,
                     (ef_addr32)(uintptr_t)g_palette_banks[0], sizeof(g_current_palette));
        g_first_palette_value = value;
        g_palette_values[0] = value;
    } else if (index < PALETTE_COUNT) {
        load_companion_files_dbf3c((ef_addr32)(uintptr_t)g_palette_banks[index],
                                   (ef_addr32)(uintptr_t)name,
                                   (ef_addr32)(uintptr_t)g_shade_tables[index]);
        g_palette_values[index] = value;
    } else {
        guest_callptr3_x(g_gui_error_hook, 0x0019000cu,     /* "Palette number %d ... Only %d" */
                         (uint32_t)(int32_t)index, PALETTE_COUNT);
    }
    ++g_palette_count;
}

/* 0xdac2c, 286 bytes, "ref_point". The two "%i" coordinates are
 * scanned as ints straight into the record, so Y overwrites the high
 * half of X and the name pointer later overwrites the high half of Y.
 * Only kinds 1 and 2 are kept. The uppercased name is copied to the
 * heap; the 257th point is reported after the count wraps to 0. */
void env_ref_point_dac2c(void)
{
    char name[80];
    ef_addr32 text = (ef_addr32)(uintptr_t)name;
    ef_addr32 line = env_take_line_090();
    int32_t kind = 0;
    ef_ref_point *point = &g_ref_points[g_ref_point_count];
    ef_addr32 copy;

    guest_fn_df26f_x(line, 0x001901e4u,                     /* "%i,%i,%i %[^)]" */
                     (uintptr_t)&point->x, (uintptr_t)&point->y,
                     (uintptr_t)&kind, (uintptr_t)name);
    point->kind = (int16_t)kind == 1 || (int16_t)kind == 2 ? (uint8_t)kind : 0u;
    uppercase_string(text);
    copy = guest_fn_e29e8(strlen_ds(text) + 1u);
    point->name = copy;
    copy_string_9d749(copy, text);
    if (++g_ref_point_count == 0u)
        guest_callptr1_x(g_gui_error_hook, 0x001901f4u);  /* "Too many refpoints" */
    fill_bytes_b3fb0(text, 0u, sizeof(name));
}

/* Store SHAPE for crater type TYPE in each of COUNT records and mark
 * them with state -1. */
static void set_crater_shapes(ef_addr32 records, int32_t count, ef_addr32 name, int32_t type)
{
    int32_t i;

    for (i = 0; i < count; ++i) {
        ef_addr32 record = records + (uint32_t)i * (uint32_t)sizeof(ef_crater_record);
        int16_t shape = (int16_t)guest_lookup_value(name);

        EF_U16(record + offsetof(ef_crater_record, shapes) + (uint32_t)type * 2u) = (uint16_t)shape;
        EF_U16(record + offsetof(ef_crater_record, state)) = 0xffffu;
    }
}

/* Link COUNT records into a ring and return the first. With COUNT 0
 * the original still writes the link of record -1. */
static ef_addr32 link_crater_ring(ef_addr32 records, int32_t count)
{
    int32_t i;

    for (i = 0; i < count - 1; ++i)
        EF_U32(records + (uint32_t)i * (uint32_t)sizeof(ef_crater_record) +
               offsetof(ef_crater_record, next)) =
            records + (uint32_t)(i + 1) * (uint32_t)sizeof(ef_crater_record);
    EF_U32(records + (uint32_t)(count - 1) * (uint32_t)sizeof(ef_crater_record) +
           offsetof(ef_crater_record, next)) = records;
    return records;
}

/* 0xdaef4, 448 bytes, "load_craters". One "name type" line per crater
 * type: every crater record and slot gets the shape of that name for
 * the type. Both arrays then become rings. An unset type count (-1) is
 * reported and reads no lines. */
void env_load_craters_daef4(void)
{
    char name[80];
    int32_t type;
    int32_t line;

    if (g_crater_type_count == -1)
        guest_callptr1_x(g_gui_error_hook, 0x0019029cu);  /* "Nb Craters not specified" */
    g_crater_ring = 0u;
    g_crater_slot_ring = 0u;
    for (line = 0; line < g_crater_type_count; ++line) {
        guest_fn_df26f_x(env_take_line_090(), 0x00190004u,  /* "%s %d" */
                         (uintptr_t)name, (uintptr_t)&type);
        set_crater_shapes(g_crater_records, g_crater_count, (ef_addr32)(uintptr_t)name, type);
        set_crater_shapes(g_crater_slots, g_crater_slot_count, (ef_addr32)(uintptr_t)name, type);
    }
    g_crater_ring = link_crater_ring(g_crater_records, g_crater_count);
    g_crater_slot_ring = link_crater_ring(g_crater_slots, g_crater_slot_count);
}
