/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_OBJECT_RELATION_H
#define EF_STRUCTS_OBJECT_RELATION_H

#include "ef_types.h"

/* View of the two observed 16-bit fields, not an inferred complete object. */
typedef struct EF_PACKED ef_object_relation_fields {
    uint8_t unknown_000[0x146];
    uint16_t active_slot;     /* +0x146 */
    uint8_t unknown_148[0x10];
    uint16_t pending_slot;    /* +0x158 */
} ef_object_relation_fields;

EF_STATIC_ASSERT(ef_relation_active_off,
                 offsetof(ef_object_relation_fields, active_slot) == 0x146);
EF_STATIC_ASSERT(ef_relation_pending_off,
                 offsetof(ef_object_relation_fields, pending_slot) == 0x158);

#endif
