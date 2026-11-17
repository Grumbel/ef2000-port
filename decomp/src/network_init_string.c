/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x33760, 74 bytes. EAX=widget. Format into the original 16-byte local.
 * ORIGINAL BUG: the XOR result indexes the table without a bounds check;
 * neither the selected string nor the formatted output is length checked. */
void net_new_init_string_33760(ef_addr32 widget)
{
    char text[16];
    uint8_t choice = (uint8_t)(EF_U8(0x0015f688u) ^ 1u);
    ef_addr32 selected = EF_U32(0x00125958u + 4u * choice);
    EF_U8(0x0015f688u) = choice;
    format_ui_status_value((ef_addr32)(uintptr_t)text, 0x00182bc0u, selected);
    show_widget_a7588(widget, (ef_addr32)(uintptr_t)text);
    write_settings_file_a2bdc();
}

/* 0x33ad8, 129 bytes. EAX=root. Sample the initialization choice only
 * after showing the device field. Keep the original 72-byte local buffer.
 * ORIGINAL BUG: the byte index and formatted length are unchecked. */
void net_open_loops_form_33ad8(ef_addr32 root)
{
    char text[72];
    ef_addr32 widget;
    ef_addr32 selected;
    widget = find_widget_checked_a78bc(root, 0x00182c70u);
    show_widget_a7588(widget, 0x0015f4fdu);
    selected = EF_U32(0x00125958u + 4u * EF_U8(0x0015f688u));
    format_ui_status_value((ef_addr32)(uintptr_t)text, 0x00182bc0u, selected);
    widget = find_widget_checked_a78bc(root, 0x00182c78u);
    show_widget_a7588(widget, (ef_addr32)(uintptr_t)text);
    widget = find_widget_checked_a78bc(root, 0x00182c80u);
    show_widget_a7588(widget, 0x0015f53du);
    widget = find_widget_checked_a78bc(root, 0x00182c8cu);
    show_widget_a7588(widget, 0x0015f57du);
}
