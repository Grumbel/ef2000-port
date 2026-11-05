/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_ENVIRONMENT_092_H
#define EF_ENVIRONMENT_092_H

#include "ef_environment_091.h"

/* Further host-only READCOLS models. Guest comparison, transformation,
 * lookup and palette loader ABIs are intentionally unresolved adapters. */
void env_palette_dispatch_da5cc(void);
void env_cockpit_palette_da644(void);
void env_lightning_point_dac2c(void);
void env_smoke_rings_daef4(void);

#endif
