/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_UNIT_TYPES_H
#define EF_STRUCTS_UNIT_TYPES_H

#include "ef_types.h"

/* 0x128564: 0xe5-byte unit-type records, indexed by the type byte at
 * unit +0xe9 or by the enemy-view type table at 0x12afda. Only fields
 * read by 0x7d138 are recovered. */
typedef struct EF_PACKED ef_unit_type_record {
    ef_addr32 name;                  /* +0x00, text looked up through 0x9ddfc */
    uint8_t unknown_04[0x39];
    uint8_t has_page_text;           /* +0x3d, enemy view runs page text 7 */
    uint8_t unknown_3e[0xa7];
} ef_unit_type_record;
EF_STATIC_ASSERT(ef_unit_type_has_page_text_off, offsetof(ef_unit_type_record, has_page_text) == 0x3d);
EF_STATIC_ASSERT(ef_unit_type_size, sizeof(ef_unit_type_record) == 0xe5);

#endif
