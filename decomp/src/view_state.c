/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"

/* 0x6b608, 24 bytes. When view-state sync is enabled, run the selected
 * object update 0x72944 and copy the pending view state into the object
 * named by the signed high word of g_grid_secondary_selector. The original
 * returns through the RET at 0x6b604 and falls through into 0x6b620.
 * No inputs; EAX scratch, every other register preserved by both callees.
 * Sole caller 0x8d59c@0x8d979. */
void sync_selected_view_state_6b608(void)
{
    if (g_view_state_sync_enabled == 0u)
        return;
    guest_fn_72944();
    copy_view_state_6b620((uint32_t)(int32_t)(g_grid_secondary_selector >> 16));
}
