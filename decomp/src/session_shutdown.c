/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x34d94, 131 bytes. No consumed register arguments; callers 34f79 and
 * 352c7 discard EAX. Clear each ownership flag before closing its handle.
 * Read the second flag only after the first close, which can change it. */
void shutdown_menu_session_34d94(void)
{
    uint32_t handle;

    EF_U8(0x0019f5aeu) = 0;
    if (EF_U8(0x0013578cu) != 0) {
        if (EF_U8(0x001357c8u) != 0)
            run_progress_delay_34d50();
        shutdown_device_a3da0();
    }
    clear_session_34ea4();
    set_pending_name_ac7a8(0x00182f24u);
    EF_U8(0x0019f59cu) = 1;
    if (EF_U8(0x0019f59eu) != 0) {
        handle = EF_U32(0x0019f35cu);
        EF_U8(0x0019f59eu) = 0;
        guest_fn_a7990(handle);
    }
    if (EF_U8(0x0019f5a3u) != 0) {
        handle = EF_U32(0x0019f360u);
        EF_U8(0x0019f5a3u) = 0;
        guest_fn_a7990(handle);
    }
    EF_U8(0x0019f59bu) = 0xff;
}
