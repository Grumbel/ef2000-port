/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_ARCHIVE_CONTEXT_H
#define EF_STRUCTS_ARCHIVE_CONTEXT_H

#include "ef_types.h"

/* An open archive (lookup 0xeb520, seek 0xeb698, load 0xeba64). The
 * eight-byte header of the entry last sought is cached at +0x148. */
typedef struct EF_PACKED ef_archive_context {
    uint32_t handle;
    uint8_t unknown_04[0x144];
    uint16_t header_tag;           /* +0x148, 'RJ' 0x524a or 'RA' 0x4152 */
    uint16_t unknown_14a;
    uint32_t header_unpacked_size; /* +0x14c */
    uint32_t header_position;      /* +0x150, position the header was read at */
} ef_archive_context;
EF_STATIC_ASSERT(ef_archive_context_tag_off, offsetof(ef_archive_context, header_tag) == 0x148);
EF_STATIC_ASSERT(ef_archive_context_position_off, offsetof(ef_archive_context, header_position) == 0x150);

/* A directory entry returned by 0xeb520. */
typedef struct EF_PACKED ef_archive_entry {
    uint32_t unknown_00;
    uint32_t position;
    uint32_t size;                 /* stored size */
} ef_archive_entry;

#define EF_ARCHIVE_TAG_RJ 0x524au
#define EF_ARCHIVE_TAG_RA 0x4152u

#endif
