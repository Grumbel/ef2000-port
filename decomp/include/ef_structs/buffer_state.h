/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_BUFFER_STATE_H
#define EF_STRUCTS_BUFFER_STATE_H

#include "ef_types.h"

/* 0x1f8e00, 12-byte records selected by 0x7aac0. */
typedef struct EF_PACKED ef_plane_source {
    ef_addr32 high_plane;            /* +0x00, copied to 0x1f8eb4 */
    ef_addr32 low_plane;             /* +0x04, copied to 0x1f8eb8 */
    ef_addr32 auxiliary;             /* +0x08, copied to 0x1f8ea4 */
} ef_plane_source;
EF_STATIC_ASSERT(ef_plane_source_size, sizeof(ef_plane_source) == 12);
EF_STATIC_ASSERT(ef_plane_source_high, offsetof(ef_plane_source, high_plane) == 0);
EF_STATIC_ASSERT(ef_plane_source_low, offsetof(ef_plane_source, low_plane) == 4);
EF_STATIC_ASSERT(ef_plane_source_aux, offsetof(ef_plane_source, auxiliary) == 8);

#endif
