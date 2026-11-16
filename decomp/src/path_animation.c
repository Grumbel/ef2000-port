/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xf36a4, 109 bytes. No inputs or consumed output; six other GPRs
 * preserved, plain RET. Sole caller f4079 retains its hover value in EDI.
 * Draw the selected path before updating the animation stage. */
void step_animation_f36a4(void)
{
    int32_t path;
    uint16_t delta, stage;

    path = EF_S32(0x00161e18u) >> 16;
    draw_path_segments_f3610((uint32_t)path);
    delta = EF_U16(0x00161e18u);
    stage = EF_U16(0x00161e16u);
    stage = (uint16_t)(stage + delta);
    EF_U16(0x00161e16u) = stage;
    if ((int16_t)stage < 0) {
        EF_U16(0x00161e18u) = 0;
        EF_U16(0x00161e16u) = 0;
        return;
    }
    /* Re-read the original overlapping dword; only exactly five stops. */
    if ((EF_S32(0x00161e14u) >> 16) == 5) {
        refresh_f35f0();
        EF_U16(0x00161e18u) = 0;
        EF_U16(0x00161e16u) = 4;
    }
}
