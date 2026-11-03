/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_LINK_RESOLUTION_H
#define EF_STRUCTS_LINK_RESOLUTION_H

#include "ef_types.h"

/* These are two independent record shapes inferred from 0x3856c and
 * 0x38658; the semantic identity of their enclosing tables is unresolved. */
typedef struct EF_PACKED ef_link_reference {
    uint8_t unknown_00[0x16];
    int16_t index;                 /* +0x16 */
    ef_addr32 current;            /* +0x18 */
} ef_link_reference;
EF_STATIC_ASSERT(ef_link_reference_index_offset, offsetof(ef_link_reference, index) == 0x16);
EF_STATIC_ASSERT(ef_link_reference_current_offset, offsetof(ef_link_reference, current) == 0x18);
EF_STATIC_ASSERT(ef_link_reference_size, sizeof(ef_link_reference) == 0x1c);

typedef struct EF_PACKED ef_link_candidate {
    uint8_t unknown_00[0x15];
    ef_addr32 alternate;          /* +0x15, unaligned */
} ef_link_candidate;
EF_STATIC_ASSERT(ef_link_candidate_alternate_offset, offsetof(ef_link_candidate, alternate) == 0x15);

#endif
