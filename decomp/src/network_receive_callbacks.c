/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"

/* 0x35e68, 37 bytes. EAX=widget. The final two zero stack words are
 * the 0.0 scale consumed by 35de0 with RET 8. */
void net_open_rx_status_35e68(ef_addr32 widget)
{
    apply_to_widget_a75b4(widget, 0x00182fa8u, 0);
    clear_children_dirty(widget);
    guest_fn_35de0_x(0, 0);
}

/* 0x35e90, 37 bytes. EAX=widget. The final two zero stack words are
 * the 0.0 scale consumed by 35de0 with RET 8. */
void net_set_nonentry_flag_35e90(ef_addr32 widget)
{
    apply_to_widget_a75b4(widget, 0x00182fa8u, 0);
    clear_children_dirty(widget);
    guest_fn_35de0_x(0, 0);
}
