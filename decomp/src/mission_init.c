/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define g_gui_message_hook   EF_U32(0x00132a74u)  /* cdecl message sink */

#define STR_WAY_EXTENSION    0x001854fcu  /* ".WAY" */
#define STR_INIT_SECTION     0x00185504u  /* "INIT:" */
#define STR_UNKNOWN_COMMAND  0x0018550cu  /* "MISSION ERROR: Unknown command '%s'." */
#define STR_NOT_ALLOWED      0x00185534u  /* "MISSION ERROR: Command '%s' not allowed in this context" */
#define STR_NO_WAYPOINTS     0x0018556cu  /* "... No waypoint file specified in mission file." */
#define STR_NO_LEVEL         0x001855a8u  /* "... No level file specified in mission file." */
#define STR_NO_WORLD_TYPE    0x001855e0u  /* "... No world type specified in mission file." */
#define STR_NO_START         0x00185618u  /* "... No starting position specified in mission file." */
#define STR_LEVEL_DIRECTORY  0x00185658u  /* "\\lev\\" */

#define ADDR_MISSION_WORD    0x001a5968u  /* current script word */
#define ADDR_LEVEL_PATH      0x00425490u  /* level directory path buffer */
#define ADDR_LEVEL_NAME      0x001a58c8u  /* level file name from the mission */

#define WAY_EXTENSION_SIZE   5
#define INIT_COMMANDS        1           /* command mask for the INIT: section */

enum { COMMAND_OK = 0, COMMAND_NOT_ALLOWED = 1, COMMAND_UNKNOWN = 2 };

/* g_mission_sections bits, set by the INIT: commands */
#define SECTION_WAYPOINTS    0x01
#define SECTION_LEVEL        0x02
#define SECTION_WORLD_TYPE   0x04
#define SECTION_START        0x08

static void report(ef_addr32 message)
{
    guest_callptr1_x(g_gui_message_hook, message);
}

/* Reset the mission state the INIT: section fills in. */
static void reset_mission_state(void)
{
    int i;

    EF_U16(0x001a5a86u) = 0;
    g_mission_sections = 0;
    EF_U16(0x001a5a8cu) = 0xffff;
    EF_U16(0x001a5a8au) = 0xffff;
    EF_U16(0x00159130u) = 0;
    EF_U32(0x00127d14u) = 0;
    EF_U32(0x001a5be4u) = 0;
    EF_U32(0x00127d08u) = 0;
    EF_U32(0x00127d0cu) = 0;
    EF_U32(0x00127d10u) = 0;
    EF_U32(0x00127d18u) = 0;
    EF_U32(0x00127d1cu) = 0;
    EF_U32(0x00127d20u) = 0;
    EF_U32(0x001a58f8u) = EF_U32(0x00127ce8u);
    EF_U32(0x00127d24u) = 0;
    EF_U32(0x001a5900u) = 0;
    EF_U16(0x00127d3eu) = 0;
    EF_U32(0x001a58a8u) = 0xffffffffu;
    EF_U32(0x001a58fcu) = EF_U32(0x00127cf4u);
    EF_U8(0x001a5aa0u) = 0;
    EF_U16(0x00127d40u) = 0;
    g_cloud_level_count = 0;
    EF_U8(0x001a5a90u) = 0xff;
    EF_U8(0x001a5a9fu) = 0;
    g_burst_zone_count = 0;
    for (i = 0; i < WAY_EXTENSION_SIZE; i++)
        g_waypoint_extension[i] = EF_U8(STR_WAY_EXTENSION + i);
    EF_U8(0x001a5a9du) = 0;
    EF_U8(0x001a5a9eu) = 0;
    EF_U8(0x001a5aa1u) = 0;
    EF_U8(0x00127d00u) = 0;
    EF_U8(0x001a5a9cu) = 99;
    EF_U8(0x001a5a9bu) = 99;
    EF_U16(0x001a5a84u) = 0;
    EF_U32(0x001a35acu) = 0xffffffffu;
    EF_U8(0x001a5a99u) = 0;
    EF_U16(0x00127d3cu) = 0;
    EF_U32(0x00127d28u) = 0;
    EF_U32(0x00127d2cu) = 0;
    EF_U16(0x001a5a82u) = 0;
    EF_U16(0x0019bf74u) = 0;
}

/* 0x52164, 611 bytes. Load the mission file's INIT: section: reset the
 * mission state (waypoint extension ".WAY"), load the text file, skip to
 * "INIT:" and dispatch each command, reporting unknown or disallowed ones.
 * Free the file block, report each missing waypoint file, level file,
 * world type or starting position, keep the starting position and load
 * the level from "<default>\lev\<name>". Always returns 1. */
int16_t load_mission_init_52164(void)
{
    uint8_t status;

    reset_mission_state();
    load_text_file_510d0();
    skip_to_word_50fcc(STR_INIT_SECTION);
    while ((uint16_t)wait_command_50e84() != 0) {
        status = dispatch_command_50f54(INIT_COMMANDS);
        if (status == COMMAND_UNKNOWN)
            guest_callptr2_x(g_gui_message_hook, STR_UNKNOWN_COMMAND, ADDR_MISSION_WORD);
        else if (status == COMMAND_NOT_ALLOWED)
            guest_callptr2_x(g_gui_message_hook, STR_NOT_ALLOWED, ADDR_MISSION_WORD);
    }
    arena_free_e2af4(g_mission_file_block);

    if ((g_mission_sections & SECTION_WAYPOINTS) == 0)
        report(STR_NO_WAYPOINTS);
    if ((g_mission_sections & SECTION_LEVEL) == 0)
        report(STR_NO_LEVEL);
    if ((g_mission_sections & SECTION_WORLD_TYPE) == 0)
        report(STR_NO_WORLD_TYPE);
    if ((g_mission_sections & SECTION_START) == 0)
        report(STR_NO_START);

    g_mission_start_x = g_player_x;
    g_mission_start_y = g_player_y;
    build_default_path_9c980(ADDR_LEVEL_PATH);
    strcat_9ca57(ADDR_LEVEL_PATH, STR_LEVEL_DIRECTORY);
    guest_fn_d9e50(ADDR_LEVEL_NAME);
    return 1;
}
