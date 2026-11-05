/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_ENVIRONMENT_091_H
#define EF_ENVIRONMENT_091_H

#include "ef_environment_090.h"

/* Host-only models of the READCOLS record and sky handlers. Their scanf,
 * allocation, string and engine calls retain unresolved guest adapters. */
void env_load_darkside_palette_da748(void);
void env_set_darkside_values_da8c4(void);
void env_set_crater_count_dad4c(void);
void env_allocate_craters_dae40(void);
void env_sun_flare_db28c(void);
void env_sun_texture_db654(void);
void env_cannon_hit_db730(void);
void env_fix_horizon_db808(void);
void env_heavenly_body_db8bc(void);
void env_init3d_file_dbff8(void);
void env_east_west_palettes_dc094(void);
void env_setsun_dc124(void);

#endif
