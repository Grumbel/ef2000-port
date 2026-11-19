/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/* Mission-planning (gpw) window callbacks from the GUI table at 0x12b4c8.
 * They use the plain Watcom register ABI; dialog and widget lookups abort
 * inside the callee when a name is missing. */

#include "ef_abi.h"
#include "ef_globals.h"

/* 0x69f1c, 141 bytes, callback slot 0x12b680. Save the 20 three-byte
 * pair marks, clear the GUI word 0x41eeda and mark the plane window
 * redrawn. In connection mode 3 refresh the slot list first. When the
 * socket is ready, refresh again in mode 1 and request "txmiss";
 * otherwise set the pending latch to 100. Mode 3 finally shows the
 * "loading" widget of "loadingwin2" (0xa46cc keeps EDX, so 0xa78bc
 * receives the widget name loaded before the dialog lookup). EBX and
 * EDX are saved. EAX returns the connection-mode byte, or in mode 3
 * whatever the widget show leaves there; the GUI dispatcher observes it. */
uint32_t gpwRedrawPlaneWindow(void)
{
    memcpy_dc832((ef_addr32)(uintptr_t)g_saved_object_pair_marks,
                 (ef_addr32)(uintptr_t)g_object_pair_marks,
                 sizeof g_saved_object_pair_marks);
    clear_word_41eeda();
    g_plane_window_redrawn = 1u;
    if (g_frontend_connection_mode == 3u)
        refresh_slot_list_69d10();
    if (g_socket_pending_ready != 0u) {
        if (g_frontend_connection_mode == 1u)
            refresh_slot_list_69d10();
        set_pending_name_ac7a8(0x00186354u);            /* "txmiss" */
    } else {
        g_frontend_pending_latch = 100u;
    }
    if (g_frontend_connection_mode != 3u)
        return g_frontend_connection_mode;
    return show_widget_a7588_r(find_widget_checked_a78bc(
                                   find_named_or_abort_a46cc(0x0018635cu), /* "loadingwin2" */
                                   0x00186368u),                      /* "loading" */
                               0x00131102u);
}
