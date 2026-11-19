/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_KOTS_H
#define EF_STRUCTS_KOTS_H

#include "ef_types.h"

/* 0x121e30: home base of each "kots" side, 8 bytes each. */
typedef struct EF_PACKED ef_kots_side_base {
    ef_addr32 name;                  /* +0x00, "Andoya" / "Orland" */
    int16_t position[2];             /* +0x04, map coordinates */
} ef_kots_side_base;
EF_STATIC_ASSERT(ef_kots_side_base_size, sizeof(ef_kots_side_base) == 8);

#endif
