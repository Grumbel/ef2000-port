/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x33c64, 346 bytes. EAX=root; jump table maps driver modes 0..4.
 * Unsupported modes still update the name and host-status fields. */
void initialize_players_window_33c64(ef_addr32 root)
{
    ef_addr32 status = find_widget_checked_a78bc(root, 0x00182cb0u);
    ef_addr32 name = find_widget_checked_a78bc(root, 0x00182c58u);
    ef_addr32 driver = find_widget_checked_a78bc(root, 0x00182cbcu);
    ef_addr32 text;

    apply_a4b80_a4db0(0x00182cc8u);
    apply_a4b80_a4db0(0x00182cd4u);
    apply_a4b80_a4db0(0x00182ce4u);
    apply_a4b80_a4db0(0x00182cf4u);
    apply_a4b80_a4db0(0x00182d00u);
    if (EF_U8(0x001357bau) == 0) {
        show_widget_a7588(status, 0);
        show_widget_a7588(name, 0);
        show_widget_a7588(driver, 0);
        return;
    }
    switch (EF_U8(0x0013579au)) {
    case 0:
        apply_a4cb4_a4d90(0x00182cc8u);
        text = 0x00182d0cu;
        break;
    case 1:
        apply_a4cb4_a4d90(0x00182cd4u);
        text = 0x00182d10u;
        break;
    case 2:
        apply_a4cb4_a4d90(0x00182cd4u);
        apply_a4cb4_a4d90(0x00182ce4u);
        if (EF_U8(0x0013578du) != 0)
            apply_a4cb4_a4d90(0x00182d00u);
        text = 0x00182d18u;
        break;
    case 3:
        apply_a4cb4_a4d90(0x00182cf4u);
        text = 0x00182d20u;
        break;
    case 4:
        text = 0x00182d28u;
        break;
    default:
        goto update_name;
    }
    show_widget_a7588(driver, text);
update_name:
    show_widget_a7588(name, 0x0013579bu);
    show_widget_a7588(status, EF_U8(0x001357c8u) != 0 ? 0x00130268u : 0x0013026bu);
}
