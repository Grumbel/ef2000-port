/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Tiny guarded forwarders. */

/* 0xdd8c0, 14 bytes. Run 0xdd8dc when the saved width at 0x15ee94 is -1 (nothing saved yet); otherwise do nothing. */
void redraw_if_unsaved_dd8c0(void)
{
    if (EF_S16(0x0015ee94u) == -1)
        save_and_draw_block_dd8dc();
}

/* 0x101f78, 14 bytes. Forward a stack argument to 0x1111e5. */
void forward_to_1111e5_101f78(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t value)
{
    (void)u0; (void)u1; (void)u2; (void)u3;
    guest_fn_1111e5_x(value);
}
