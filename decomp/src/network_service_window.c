/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"
#include "ef_structs/widget_links.h"

/* 0x3396c, 361 bytes. EAX=root. Initialize provider/session controls.
 * ORIGINAL BUG: the leading-character scan sign-extends AX each iteration,
 * so a run of 32768 classified bytes wraps behind the input pointer. */
void initialize_network_service_window_3396c(ef_addr32 root)
{
    ef_addr32 first = find_widget_checked_a78bc(root, 0x00182c28u);
    ef_addr32 second = find_widget_checked_a78bc(root, 0x00182c34u);
    ef_addr32 third = find_widget_checked_a78bc(root, 0x00182c40u);
    ef_addr32 dialog = find_named_or_abort_a46cc(0x00182c4cu);
    ef_addr32 name = find_widget_checked_a78bc(dialog, 0x00182c58u);
    ef_addr32 source;
    uint32_t index;
    uint8_t character;
    char text[28];
    char placeholder[8];

    guest_fn_a4342_plain((ef_addr32)(uintptr_t)placeholder, 0x00182bdcu);
    if (EF_U8(0x00125936u) != 0) {
        show_widget_a7588(third, 0x00125937u);
    } else {
        show_widget_a7588(third, (ef_addr32)(uintptr_t)placeholder);
        show_widget_a7588(first, (ef_addr32)(uintptr_t)placeholder);
        show_widget_a7588(second, (ef_addr32)(uintptr_t)placeholder);
    }
    if (EF_U8(0x001357c8u) != 0) {
        index = 0;
        for (;;) {
            source = EF_U32(name + offsetof(ef_widget_link, text)) +
                (uint32_t)(int32_t)(int16_t)(uint16_t)index;
            character = (uint8_t)(EF_U8(source) + 1u);
            if ((EF_U8(0x00195af0u + character) & 2u) == 0)
                break;
            ++index;
        }
        copy_string_9d749((ef_addr32)(uintptr_t)text, source);
        uppercase_string((ef_addr32)(uintptr_t)text);
        strcat_9ca57((ef_addr32)(uintptr_t)text, 0x00182c64u);
        show_widget_a7588(first, (ef_addr32)(uintptr_t)text);
        copy_string_9d749(0x0019f566u, (ef_addr32)(uintptr_t)text);
        move_widget_a74bc(first, 0x116, 0x10);
        move_widget_a74bc(second, 0, 0);
    } else {
        move_widget_a74bc(second, 0x116, 0x10);
        move_widget_a74bc(first, 0, 0);
    }
    if (EF_U8(0x001357c8u) == 0 && EF_U8(0x0019f5aeu) == 0 &&
        EF_U8(0x00125902u) == 0) {
        show_widget_a7588(second, (ef_addr32)(uintptr_t)placeholder);
        show_widget_a7588(first, (ef_addr32)(uintptr_t)placeholder);
        EF_U16(0x0019f566u) = EF_U16(0x00182c6cu);
    }
}
