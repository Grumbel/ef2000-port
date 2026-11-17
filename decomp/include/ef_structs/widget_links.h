/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_WIDGET_LINKS_H
#define EF_STRUCTS_WIDGET_LINKS_H

#include "ef_types.h"

/* Partial widget layout; only explicitly asserted fields are established here. */
typedef struct EF_PACKED ef_widget_link {
    uint8_t unknown_00[0x10];
    uint8_t id;
    uint8_t unknown_11[0x10];
    ef_addr32 next;
    uint8_t unknown_25[2];
    ef_addr32 text;
    uint8_t unknown_2b[0x14];
    uint8_t number_label;
    uint8_t unknown_40[0x11];
    ef_addr32 parent;
    uint8_t unknown_55[0x12];
    uint16_t content_height;
} ef_widget_link;
EF_STATIC_ASSERT(ef_widget_id_off, offsetof(ef_widget_link, id) == 0x10);
EF_STATIC_ASSERT(ef_widget_next_off, offsetof(ef_widget_link, next) == 0x21);

EF_STATIC_ASSERT(ef_widget_text_off, offsetof(ef_widget_link, text) == 0x27);
EF_STATIC_ASSERT(ef_widget_number_label_off, offsetof(ef_widget_link, number_label) == 0x3f);
EF_STATIC_ASSERT(ef_widget_parent_off, offsetof(ef_widget_link, parent) == 0x51);

EF_STATIC_ASSERT(ef_widget_content_height_off, offsetof(ef_widget_link, content_height) == 0x67);

#endif
