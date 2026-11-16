/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x37260,83 bytes including the shared 3718e..371ad tail. No inputs,
 * six registers preserved, EAX scratch, plain RET. Callback name retained
 * from the original registry; it does not describe every branch. */
void gui_open_credits_37260(void)
{
    uint32_t mode = EF_U8(0x00125846u);

    if (mode != 3u) {
        if (mode == 1u) {
            if (EF_U8(0x00125935u) != 0u)
                set_pending_name_ac7a8(0x00183290u);
            else
                EF_U8(0x0041f2e8u) = 100u;
        }
    } else if (EF_U8(0x00126576u) == 0u) {
        if ((EF_S32(0x0019769eu) >> 16) != -1)
            guest_fn_b1e0();
    }
}

/* 0x37254,12 bytes. Falls through into 37260 after setting mode one.
 * Preserve that shared logic rather than specializing the callback. */
void gui_new_socket_37254(void)
{
    EF_U8(0x00125846u) = 1u;
    gui_open_credits_37260();
}
