/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_WIDGET_LINKS_H
#define EF_STRUCTS_WIDGET_LINKS_H

#include "ef_types.h"

/* Partial widget layout; only the sibling link is established here. */
typedef struct EF_PACKED ef_widget_link {
    uint8_t unknown_00[0x21];
    ef_addr32 next;
} ef_widget_link;
EF_STATIC_ASSERT(ef_widget_next_off, offsetof(ef_widget_link, next) == 0x21);

#endif
