/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_WIDGET_LINKS_H
#define EF_STRUCTS_WIDGET_LINKS_H

#include "ef_types.h"

/* Partial widget layout; only explicitly asserted fields are established here. */
typedef struct EF_PACKED ef_widget_link {
    uint8_t unknown_00[0x21];
    ef_addr32 next;
    uint8_t unknown_25[2];
    ef_addr32 text;
    uint8_t unknown_2b[0x3c];
    uint16_t content_height;
} ef_widget_link;
EF_STATIC_ASSERT(ef_widget_next_off, offsetof(ef_widget_link, next) == 0x21);

EF_STATIC_ASSERT(ef_widget_text_off, offsetof(ef_widget_link, text) == 0x27);

EF_STATIC_ASSERT(ef_widget_content_height_off, offsetof(ef_widget_link, content_height) == 0x67);

#endif
