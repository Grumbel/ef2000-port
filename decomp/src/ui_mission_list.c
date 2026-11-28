/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/* Load the UI mission list (the "\cfg\" text file named by
 * g_score_file_name) into a table of 76-byte rows. */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define LIST_CURSOR       0x001f8f70u /* parse position in the loaded file */
#define LIST_COUNT        0x0012b056u /* int16, number of rows */
#define NETWORK_SESSION   0x00125935u /* nonzero: skip rows below the minimum level */
#define MINIMUM_LEVEL     0x001357b0u /* int16 */

#define ROW_SIZE          0x4cu
#define ROW_TITLE         0x00u /* 0x28 bytes */
#define ROW_BRIEFING      0x28u /* 0x12 bytes */
#define ROW_CONFIG        0x3au /* 0x12 bytes */

#define STR_CFG_DIR       0x00187ce4u /* "\cfg\" */
#define STR_NO_FILE       0x00187cecu /* "ERROR: Unable to find UI missions file - %s" */
#define STR_NO_MEMORY     0x00187d18u /* "simul: no memory for mission list" */
#define STR_ADVANCED      0x00187d3cu /* "advanced.txt" */

/* Add-on mission N (1..4): config file name "addonN.cfg", title
 * "Add-on Mission #N" and briefing "addonN.brf". */
static const uint32_t addon_strings[4][3] = {
    { 0x00187d4cu, 0x00187d58u, 0x00187d6cu },
    { 0x00187d78u, 0x00187d84u, 0x00187d98u },
    { 0x00187da4u, 0x00187db0u, 0x00187dc4u },
    { 0x00187dd0u, 0x00187ddcu, 0x00187df0u },
};

static void copy_guest_bytes(ef_addr32 dst, ef_addr32 src, uint32_t count)
{
    uint32_t i;

    for (i = 0; i < count; i++)
        EF_U8(dst + i) = EF_U8(src + i);
}

/* Copy characters from the cursor into DST until one of STOP_A/STOP_B and
 * return the stop character; the cursor ends past it. */
static int16_t copy_field(ef_addr32 *cursor, ef_addr32 dst, uint8_t *stop, uint8_t stop_a, uint8_t stop_b)
{
    int16_t j = 0;
    uint8_t c;

    while ((c = EF_U8(*cursor)) != stop_a && c != stop_b) {
        EF_U8(dst + (uint32_t)(int32_t)j) = c;
        j++;
        (*cursor)++;
    }
    (*cursor)++;
    *stop = c;
    return j;
}

/* 0x7bee8, 1085 bytes. The file starts (after tabs and spaces) with the row
 * count on its own line; *ROWS is reallocated for that many rows plus four.
 * Each row line is "title$briefing$config" optionally followed by "$level";
 * a row without a level loses the last character of its config name (the
 * carriage return). With a network session, rows whose level is below the
 * minimum are dropped. For the "advanced.txt" list outside a network
 * session, up to four add-on missions are appended when their config file
 * exists. Quirks kept from the original: spaces in the count line leave
 * holes in the number buffer, the level is parsed without a terminator
 * from the buffer the count used, and after a level the next title starts
 * at the newline. */
void load_ui_mission_list_7bee8(ef_addr32 rows)
{
    uint8_t path[0x48];
    uint8_t number[12];
    ef_addr32 file, cursor;
    uint32_t size;
    int16_t i, j;
    int k;

    build_default_path_9c980((ef_addr32)(uintptr_t)path);
    strcat_9ca57((ef_addr32)(uintptr_t)path, STR_CFG_DIR);
    strcat_9ca57((ef_addr32)(uintptr_t)path, (ef_addr32)(uintptr_t)g_score_file_name);
    size = guest_callptr_eax_ret_x(g_resource_size_hook, (ef_addr32)(uintptr_t)path);
    if (size == 0u)
        guest_callptr2_x(g_gui_error_hook, STR_NO_FILE, (ef_addr32)(uintptr_t)path);
    file = arena_alloc_e2a94(size);
    guest_callptr_eaxedx_x(g_resource_load_hook, (ef_addr32)(uintptr_t)path, file);

    cursor = file;
    while (EF_U8(cursor) == '\t' || EF_U8(cursor) == ' ')
        cursor++;
    i = 0;
    do {
        if (EF_U8(cursor) != ' ')
            number[i] = EF_U8(cursor);
        cursor++;
        i++;
    } while (EF_U8(cursor - 1u) != '\n');
    number[i] = 0;
    EF_U16(LIST_COUNT) = (uint16_t)parse_int((ef_addr32)(uintptr_t)number);
    EF_U32(rows) = reallocate_heap_e8651(EF_U32(rows),
                                         (uint32_t)(((int32_t)(int16_t)EF_U16(LIST_COUNT) + 4) * (int32_t)ROW_SIZE));
    if (EF_U32(rows) == 0u)
        guest_callptr1_x(g_gui_error_hook, STR_NO_MEMORY);

    for (i = 0; i < (int16_t)EF_U16(LIST_COUNT); i++) {
        ef_addr32 row;
        int16_t level = 0;
        uint8_t stop;

        row = EF_U32(rows) + (uint32_t)((int32_t)i * (int32_t)ROW_SIZE);
        j = copy_field(&cursor, row + ROW_TITLE, &stop, '$', '$');
        EF_U8(row + ROW_TITLE + (uint32_t)(int32_t)j) = 0;
        row = EF_U32(rows) + (uint32_t)((int32_t)i * (int32_t)ROW_SIZE);
        j = copy_field(&cursor, row + ROW_BRIEFING, &stop, '$', '$');
        EF_U8(row + ROW_BRIEFING + (uint32_t)(int32_t)j) = 0;
        row = EF_U32(rows) + (uint32_t)((int32_t)i * (int32_t)ROW_SIZE);
        j = copy_field(&cursor, row + ROW_CONFIG, &stop, '\n', '$');
        row = EF_U32(rows) + (uint32_t)((int32_t)i * (int32_t)ROW_SIZE);
        if (stop == '$') {
            EF_U8(row + ROW_CONFIG + (uint32_t)(int32_t)j) = 0;
            j = 0;
            while (EF_U8(cursor) != '\n') {
                number[j] = EF_U8(cursor);
                j++;
                cursor++;
            }
            EF_U32(LIST_CURSOR) = cursor;
            level = (int16_t)parse_int((ef_addr32)(uintptr_t)number);
        } else {
            EF_U32(LIST_CURSOR) = cursor;
            EF_U8(row + ROW_CONFIG - 1u + (uint32_t)(int32_t)j) = 0;
        }
        if (EF_U8(NETWORK_SESSION) != 0u && level < (int16_t)EF_U16(MINIMUM_LEVEL)) {
            EF_U16(LIST_COUNT) = (uint16_t)(EF_U16(LIST_COUNT) - 1u);
            i--;
        }
    }
    EF_U32(LIST_CURSOR) = cursor;

    if (EF_U8(NETWORK_SESSION) == 0u &&
        guest_strcmp((ef_addr32)(uintptr_t)g_score_file_name, STR_ADVANCED) == 0) {
        for (k = 0; k < 4; k++) {
            ef_addr32 row;

            if (guest_callptr_eax_ret_x(g_resource_size_hook, addon_strings[k][0]) == 0u)
                continue;
            row = EF_U32(rows) + (uint32_t)((int32_t)i * (int32_t)ROW_SIZE);
            copy_guest_bytes(row + ROW_TITLE, addon_strings[k][1], 18u);
            copy_guest_bytes(row + ROW_BRIEFING, addon_strings[k][2], 11u);
            copy_guest_bytes(row + ROW_CONFIG, addon_strings[k][0], 11u);
            EF_U16(LIST_COUNT) = (uint16_t)(EF_U16(LIST_COUNT) + 1u);
            i++;
        }
    }
    arena_free_e2af4(file);
}
