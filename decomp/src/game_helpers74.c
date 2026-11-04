/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Scene motion and HUD frame lines. */

/* 0x941e4, 229 bytes. Fold the view roll into the scene angles and request a refresh at the limits. */
void update_scene_roll_941e4(void)
{
    uint16_t old_roll;
    int32_t roll;
    uint16_t neg;

    guest_fn_d099c(0x0021439cu, 0x0012dbecu);
    EF_U16(0x0012db98u) = (uint16_t)guest_fn_d13ac_x(0x0021439cu, 0xfffffffeu);
    old_roll = EF_U16(0x0012db9au);
    roll = (int32_t)guest_fn_d13dc(0x0021439cu, old_roll);
    EF_U16(0x0012db9au) = (uint16_t)roll;
    neg = (uint16_t)(-roll);
    EF_U16(0x0012dba6u) = 0xfffeu;
    EF_U16(0x0012db9au) = neg;
    if (old_roll == neg) {
        update_scene_angle_first(0x0021439cu, neg);
        EF_U16(0x0012db9au) = 0;
    } else if (neg != 0) {
        update_scene_angle_first(0x0021439cu, (uint16_t)(EF_S32(0x0012db98u) >> 18));
    }
    guest_fn_d099c(0x0012dbecu, 0x0021439cu);
    if ((int16_t)EF_U16(0x0012db9au) >= 0 && (EF_S32(0x0012db98u) >> 16) < 0xa0)
        request_scene_refresh();
    if ((int16_t)EF_U16(0x0012db9au) <= 0 && (EF_S32(0x0012db98u) >> 16) > -0xa0)
        request_scene_refresh();
    guest_fn_93a8c();
}

/* 0x9586c, 256 bytes. Chase-camera distance control driven by the speed input bits. */
void update_chase_distance_9586c(void)
{
    uint32_t step = (uint32_t)EF_U16(0x00127d92u) * 20u;
    int32_t v;
    ef_addr32 p;

    if (EF_U8(0x002144aau) & 0x10u)
        EF_U32(0x0012dc84u) = EF_U32(0x0012dc84u) + step;
    if (EF_U8(0x002144aau) & 0x20u)
        EF_U32(0x0012dc84u) = EF_U32(0x0012dc84u) - (uint32_t)EF_U16(0x00127d92u) * 20u;
    p = EF_U32(0x00214458u);
    if ((int32_t)(EF_U32(0x0012dc84u) << 8) > EF_S32(p + 4u) - 0x400)
        EF_U32(0x0012dc84u) = EF_U32(0x0012dc84u) - (uint32_t)EF_U16(0x00127d92u) * 20u;
    p = EF_U32(0x00214458u);
    v = EF_S32(0x0012dc84u);
    EF_U32(0x0021444cu) = (uint32_t)(EF_S32(p) >> 8);
    EF_U32(0x00214450u) = (uint32_t)(EF_S32(p + 8u) >> 8);
    EF_U32(0x00214448u) = (uint32_t)v;
    guest_fn_c4105_x();
    request_scene_pair((int32_t)0xffffc000, 0);
    p = EF_U32(0x00214458u);
    set_scene_input_triplet(EF_S32(p) << 8, EF_S32(0x0012dc84u) << 16, EF_S32(p + 8u) << 8);
    set_scene_angles((int16_t)0xc000, 0, 0);
}

/* 0x9bba4, 253 bytes. Draw the HUD frame: three vertical rails and four border lines. */
void draw_hud_frame_9bba4(void)
{
    guest_fn_c99ac_x(3, 0x1e, 0x1e, 0x3c, 0x32);
    guest_fn_c9a98_x(8, 0x1e, 0x1e, 0x3c, 0x32);
    guest_fn_c99ac_x(4, 0x1e, 0x3c, 0x3c, 0x50);
    guest_fn_c9a98_x(8, 0x1e, 0x3c, 0x3c, 0x50);
    guest_fn_c99ac_x(1, 0x1e, 0x5a, 0x3c, 0x6e);
    guest_fn_c9a98_x(8, 0x1e, 0x5a, 0x3c, 0x6e);
    guest_fn_c95ac_x(8, 0x19, 0x19, 0x19, 0x73);
    guest_fn_c95ac_x(8, 0x19, 0x19, 0xaa, 0x19);
    guest_fn_c95ac_x(8, 0xaa, 0x19, 0xaa, 0x73);
    guest_fn_c95ac_x(8, 0xaa, 0x73, 0x19, 0x73);
}
