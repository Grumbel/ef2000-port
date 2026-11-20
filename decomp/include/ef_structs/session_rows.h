/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_SESSION_ROWS_H
#define EF_STRUCTS_SESSION_ROWS_H

#include "ef_types.h"

/* 76-byte rows loaded by 0x7bee8 into the pools at 0x121e40 (arcade
 * levels) and 0x1f8f74 (training missions). */
typedef struct EF_PACKED ef_session_row {
    uint8_t unknown_00[0x28];
    char session_text[0x12];         /* +0x28, handed to 0x7c4fc */
    char config_name[0x12];          /* +0x3a, "xxx.cfg" */
} ef_session_row;
EF_STATIC_ASSERT(ef_session_row_config_off, offsetof(ef_session_row, config_name) == 0x3a);
EF_STATIC_ASSERT(ef_session_row_size, sizeof(ef_session_row) == 0x4c);

#endif
