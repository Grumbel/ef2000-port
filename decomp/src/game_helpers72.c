/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Cursor forwarding and scene camera latching. */

/* 0x845c0, 91 bytes. Forward the cursor vector, redoing it once when the hit test accepts the position. */
uint8_t forward_cursor_vector_845c0(void)
{
    uint16_t bx;
    uint16_t dx;

    guest_fn_842b4_x();
    update_ui_composite();
    bx = EF_U16(0x001fcc3eu);
    dx = EF_U16(0x001fcc40u);
    guest_fn_84394_x(0x001fcbd4u, dx, bx);
    if (EF_U16(0x001fcbd0u) != 0 || EF_U16(0x001fcbd2u) != 0) {
        if (guest_fn_834e8_w((int16_t)bx, (int16_t)dx) != 0) {
            guest_fn_842b4_x();
            guest_fn_84394_x(0x001fcbd4u, dx, bx);
        }
    }
    return 0;
}

/* 0x944f4, 135 bytes. Latch the camera position and place the scene with the negated view angles. */
void latch_scene_camera_944f4(void)
{
    EF_U32(0x0021444cu) = EF_U32(0x001a5becu);
    EF_U32(0x00214448u) = EF_U32(0x001a5bf0u);
    EF_U32(0x00214450u) = EF_U32(0x001a5be8u);
    request_scene_pair((int16_t)(-(int32_t)EF_U16(0x00127d3cu)), 0);
    guest_fn_c8d30_x(EF_U32(0x002117b0u) << 8, EF_U32(0x002117b4u) << 8, EF_U32(0x002117b8u) << 8,
                     (uint32_t)(-(EF_S32(0x00127d3au) >> 16)), 0x3c);
    set_scene_angles((int16_t)(-(int32_t)EF_U16(0x00127d3cu)), (int16_t)(-(int32_t)EF_U16(0x00127d3eu)), 0);
}
