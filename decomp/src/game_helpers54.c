/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Scene angle setup, volume bounds test and page switching (230..260 bytes). */

/* 0x42e8c, 238 bytes. Switch the panel pages or finish the pending camera move. */
void update_panel_pages_42e8c(void)
{
    if (EF_U16(0x00137998u) != 0) {
        guest_fn_a4c84_w();
        guest_fn_cbc9b_w(0x00184534u);
        guest_fn_ac7a8_w(0x00184540u);
        return;
    }
    if (EF_U16(0x0013799au) != 0) {
        guest_fn_a4c84_w();
        guest_fn_cbc9b_w(0x00184534u);
        guest_fn_ac7a8_w(0x00184548u);
        return;
    }
    if (EF_U8(0x00126576u) == 0)
        return;
    if (EF_U8(0x00126577u) == 0) {
        ((void (*)(void))EF_U32(0x00126620u + (uint32_t)EF_U8(0x00126576u) * 4u))();
        if ((EF_S32(0x00126572u) >> 16) != -1)
            guest_fn_421c0_w();
    }
    if (EF_U16(0x001379a0u) == EF_U16(0x00126580u) && EF_U16(0x001379a2u) == EF_U16(0x00126582u)) {
        EF_U16(0x00126574u) = 0xffff;
        guest_fn_aca88_w();
        apply_a4b80_a4db0(0x00184550u);
        apply_a4b80_a4db0(0x001844a4u);
        EF_U8(0x00126576u) = 0;
        guest_fn_d290_w();
        guest_fn_ba5c_w();
        guest_fn_a6018_w(0x001843f0u);
        guest_fn_aca94_w();
        guest_fn_a4d90_w(0x00184558u);
    }
}

/* 0x832b8, 232 bytes. Latch the scaled box corner and test it against the current volume bounds. */
uint8_t box_outside_bounds_832b8(int32_t a, int32_t b, int32_t c)
{
    int32_t x = a >> 8;
    int32_t y = b >> 8;
    int32_t z = c >> 8;
    int32_t limit;

    EF_S32(0x00420910u) = EF_S32(0x001fcc14u) >> 8;
    EF_S32(0x00420914u) = x;
    EF_S32(0x00420904u) = EF_S32(0x001fcc18u) >> 8;
    EF_S32(0x00420908u) = y;
    EF_S32(0x0042091cu) = z;
    EF_S32(0x00420918u) = EF_S32(0x001fcc1cu) >> 8;
    EF_U16(0x00420936u) = (uint16_t)x;
    EF_U16(0x00420930u) = EF_U16(0x00420910u);
    EF_U16(0x00420938u) = (uint16_t)y;
    EF_U16(0x00420932u) = EF_U16(0x00420904u);
    EF_U16(0x0042093au) = (uint16_t)z;
    EF_U16(0x00420934u) = EF_U16(0x00420918u);
    if (guest_fn_7b7d8_w() == 0)
        return 0;
    if (EF_S32(0x001f8eacu) == 0)
        return 0;
    limit = EF_S32(0x001f8eb0u);
    if (!(x - 3 > limit) && !(x + 3 < limit))
        return 0;
    limit = EF_S32(0x001f8eacu);
    if (!(y - 3 > limit) && !(y + 3 < limit))
        return 0;
    limit = EF_S32(0x001f8ea8u);
    if (!(z - 3 > limit) && !(z + 3 < limit))
        return 0;
    return 1;
}

/* 0x919d4, 245 bytes. Reset the scene angle state and install the default handlers. */
void reset_scene_angle_state_919d4(void)
{
    uint64_t pair;

    EF_U16(0x0012dbc6u) = 0;
    EF_U32(0x002143e8u) = 0;
    EF_U16(0x00214488u) = (uint16_t)guest_fn_9ddfc_n(0x0018ab3cu);
    EF_U16(0x002144a2u) = 0;
    EF_U16(0x0021449cu) = 0;
    EF_U16(0x0021449eu) = 0;
    EF_U16(0x0012dba0u) = 0x14;
    EF_U16(0x0012db96u) = 0;
    EF_U16(0x0012db98u) = 0;
    EF_U16(0x0012db9au) = 0;
    EF_U16(0x0012dba2u) = 0;
    EF_U16(0x0012dba4u) = 0;
    EF_U16(0x0012dba6u) = 0;
    EF_U16(0x002144aeu) = 0;
    EF_U16(0x002144acu) = 0;
    EF_U8(0x0012db9eu) = 0x12;
    EF_U8(0x0012dbc4u) = 0x12;
    init_bounds_d0b68(0x0012dbecu);
    EF_U16(0x00214490u) = 0xffff;
    EF_U16(0x00214492u) = 0;
    pair = guest_fn_d9b30_w();
    EF_U32(0x00214358u) = (uint32_t)pair;
    EF_U32(0x00214360u) = (uint32_t)(pair >> 32);
    EF_U32(0x00214364u) = (uint32_t)(pair >> 32);
    EF_U32(0x00214368u) = (uint32_t)(pair >> 32);
    EF_U32(0x00158f74u) = 0x0009199cu;
    EF_U32(0x0021435cu) = 0;
    EF_U8(0x00214548u) = 0;
    EF_U32(0x00158f78u) = 0x000919b8u;
}

/* 0x93e58, 234 bytes. Compute the scene angles from the stored vector (variant with heading latch). */
void init_scene_angles_93e58(void)
{
    int32_t x;
    int32_t y;

    EF_U16(0x0012dba0u) = 0;
    EF_U16(0x0012dba6u) = 0;
    guest_fn_d9ba0_w(EF_U32(0x00214358u), 3, 0x00214360u, 0x0021435cu, 0x00214364u, 0x00214368u);
    init_bounds_d0b68(0x0012dbecu);
    guest_fn_d0ebc(0x0012dbecu, (uint16_t)(-(int32_t)EF_U16(0x00214360u)));
    guest_fn_d0dbc(0x0012dbecu, (uint16_t)(-(int32_t)EF_U16(0x0021435cu)), 0x00214360u);
    guest_fn_d0fb8(0x0012dbecu, (uint16_t)(-(int32_t)EF_U16(0x00214364u)), 0x0021435cu);
    x = (int32_t)EF_U32(0x0021435cu);
    y = (int32_t)EF_U32(0x00214360u);
    EF_U8(0x001a6d1fu) = ((x >= 0x258 && x <= 0xfde8) || (y >= 0x258 && y <= 0xfde8)) ? 1 : 0;
    EF_U16(0x0012dc80u) = 0x30;
    EF_U16(0x0012db9au) = EF_U16(0x00214360u);
    guest_fn_93a8c();
    guest_fn_95018();
}

/* 0x91f30, 249 bytes. Apply the pending yaw and roll input to the scene orientation. */
void apply_orientation_input_91f30(void)
{
    uint8_t flags;

    guest_fn_d099c(0x0021439cu, 0x0012dbecu);
    flags = EF_U8(0x002144aau);
    if (flags & 1)
        guest_fn_d0ebc(0x0021439cu, EF_U16(0x0012dc7cu));
    if (flags & 2)
        guest_fn_d0ebc(0x0021439cu, (uint16_t)(-(int32_t)EF_U16(0x0012dc7cu)));
    guest_fn_d099c(0x0012dbecu, 0x0021439cu);
    if (flags & 4)
        guest_fn_d0dbc(0x0012dbecu, EF_U16(0x0012dc7cu), 0xffff0000u);
    if (flags & 8)
        guest_fn_d0dbc(0x0012dbecu, (uint16_t)(-(int32_t)EF_U16(0x0012dc7cu)), 0xffff0000u);
    if ((int16_t)guest_fn_d13ac_w(0x0012dbecu) < 0 && EF_U16(0x0012db94u) != 0) {
        if (flags & 4)
            guest_fn_d0dbc(0x0012dbecu, (uint16_t)(-(int32_t)EF_U16(0x0012dc7cu)), 0xffff0000u);
        if (flags & 8)
            guest_fn_d0dbc(0x0012dbecu, EF_U16(0x0012dc7cu), 0xffff0000u);
        EF_U16(0x0012db94u) = 0;
    }
}
