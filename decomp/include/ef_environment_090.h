/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_ENVIRONMENT_090_H
#define EF_ENVIRONMENT_090_H

#include "ef_types.h"

/* These are host drafts, not Watcom ABI definitions. A scanner adapter
 * represents the original sscanf-like 0xdf26f call, including its stack
 * outputs; an engine adapter represents still-unrecovered guest callees.
 * Its return stands for the specific observed register selected per call
 * (e.g. EAX after e7ea4, but EDX after 80d8c), not a general ABI. */
typedef struct ef_environment_fields_090 {
    uint32_t number[8];
    char text[80];
} ef_environment_fields_090;

void env_scan_fields_090(uint32_t original_entry, uint32_t format_address,
                         ef_addr32 input_line,
                         ef_environment_fields_090 *fields);
uint32_t env_engine_call_090(uint32_t target, uint32_t eax,
                            uint32_t edx, uint32_t ebx);
void env_scan_line_090(uint32_t entry, uint32_t format,
                       ef_environment_fields_090 *fields);
void env_report_090(uint32_t message_address);

void env_darkside_fade_da1b4(void);
void env_coltab_da224(void);
void env_altitude_da2b8(void);
void env_altitude2_da360(void);
void env_ground_da408(void);
void env_ceiling_da4b0(void);
void env_planet_number_da518(void);
void env_game_type_da580(void);
void env_load_missile_palette_da7fc(void);
void env_world_name_da964(void);
void env_xmap_name_da9d4(void);
void env_mmap_name_daa5c(void);
void env_ref_point_dab40(void);
void env_map_name_dabc8(void);
void env_load_player_virtual_db0b4(void);
void env_load_player_sort_db114(void);
void env_load_player_db16c(void);
void env_set_flak_count_db1c4(void);
void env_load_flak_db21c(void);
void env_cloud_fade_db420(void);

#endif
