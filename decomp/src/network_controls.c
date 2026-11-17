/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x33630, 328 body bytes in two ranges (33630..336be, 33dbe..33e78).
 * No arguments. All decisions precede the refresh and apply callbacks. */
void refresh_network_controls_33630(void)
{
    uint8_t primary;
    uint8_t secondary;
    uint8_t unavailable;
    uint8_t inactive;
    uint8_t hosting;
    uint8_t waiting;
    uint8_t joined;
    uint8_t allow_primary;

    primary = EF_U8(0x001357c8u) != 0 ||
        (EF_U8(0x00135798u) == 0 && EF_U8(0x0013578du) == 0);
    allow_primary = EF_U8(0x0013578cu) != 0 &&
        (EF_U8(0x001357c8u) == 0 || EF_S16(0x001357b0u) >= 2) &&
        (EF_U8(0x001357c8u) != 0 || EF_U8(0x00135798u) != 0);
    unavailable = !allow_primary;
    secondary = EF_U8(0x0013578du) != 0;
    inactive = 0;
    hosting = 0;
    waiting = 0;
    joined = 0;
    if (EF_U8(0x0013578cu) == 0 && EF_U8(0x0013578du) == 0) {
        inactive = 1;
    } else if (EF_U8(0x001357c8u) != 0) {
        hosting = 1;
    } else if (EF_S16(0x001357b0u) > 1) {
        joined = 1;
    } else {
        waiting = 1;
    }
    refresh_all_widgets_a4c84();
    if (primary)
        apply_a4cb4_a4d90(0x00182d34u);
    if (inactive || secondary)
        apply_a4cb4_a4d90(0x00182d3cu);
    if (unavailable)
        apply_a4cb4_a4d90(0x00182c4cu);
    if (allow_primary)
        apply_a4cb4_a4d90(0x00182d48u);
    if (hosting)
        apply_a4cb4_a4d90(0x00182d50u);
    if (waiting)
        apply_a4cb4_a4d90(0x00182d60u);
    if (joined)
        show_status_34a04();
}
