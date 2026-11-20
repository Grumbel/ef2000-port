/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

#define OBJ_LINK_KEY        0x186u  /* dword, signed high word is the linked object */
#define OBJ_LINK_SLOT       0x187u  /* byte, row of g_object_link_record_base */
#define OBJ_POINT_INDEX     0x18bu  /* byte, point within the model row */
#define OBJ_MODEL           0xe9u   /* byte, row of the model origin table */
#define OBJ_PITCH           0x69u   /* double */
#define OBJ_ROLL            0x71u   /* double */
#define OBJ_LIFT            0x71u   /* double of the current object */

#define LINK_ROW_SIZE       25u
#define POINT_ROWS          0x0012a6cdu  /* 73-byte rows of 24-byte points */
#define POINT_ROW_SIZE      73u
#define POINT_SIZE          24u
#define MODEL_ORIGINS       0x001285f5u  /* 229-byte rows starting with a point */
#define MODEL_ROW_SIZE      229u
#define LIFT_SCALE          0x00186822u  /* double */

/* 0x70438, 606 bytes. Place the point the current object's link record
 * selects: look up the linked object (signed high word of +0x186), take
 * the point (+0x18b) of the model row its link slot names, rotate it by
 * the linked object's pitch and roll, subtract the current object's model
 * origin and, from the x part, its squared lift times a scale. When
 * 0x6e1c4 accepts the point, step the chase camera (0.2, 0.05); then
 * update the link to the selected unit (low word of
 * g_grid_secondary_selector, 0x772d0). The game runs this under control
 * word 0x127f (53-bit precision), where spilled intermediates are exact. */
void place_link_point_70438(void)
{
    ef_addr32 cur = g_current_object;
    ef_addr32 obj = lookup_filtered_indexed_object((int16_t)(EF_S32(cur + OBJ_LINK_KEY) >> 16));
    ef_addr32 p;
    ef_addr32 origin;
    double pitch, roll;
    double v[3];

    if (obj == 0)
        return;
    p = POINT_ROWS + (uint32_t)EF_U8(g_object_link_record_base + EF_U8(obj + OBJ_LINK_SLOT) * LINK_ROW_SIZE) * POINT_ROW_SIZE +
        (uint32_t)EF_U8(cur + OBJ_POINT_INDEX) * POINT_SIZE;
    origin = MODEL_ORIGINS + (uint32_t)EF_U8(cur + OBJ_MODEL) * MODEL_ROW_SIZE;
    pitch = EF_F64(obj + OBJ_PITCH);
    roll = EF_F64(obj + OBJ_ROLL);

    v[0] = EF_F64(p + 8u) * guest_fn_b45ac(pitch) * guest_fn_b45ac(roll) + EF_F64(p) * guest_fn_b45a2(pitch) +
           EF_F64(p + 16u) * guest_fn_b45ac(pitch) * guest_fn_b45a2(roll) - EF_F64(origin);
    v[0] = v[0] - guest_fn_b3fd0(EF_F64(cur + OBJ_LIFT)) * EF_F64(LIFT_SCALE);
    v[1] = -guest_fn_b45ac(roll) * EF_F64(p + 16u) + guest_fn_b45a2(roll) * EF_F64(p + 8u) - EF_F64(origin + 8u);
    v[2] = EF_F64(p + 8u) * guest_fn_b45a2(pitch) * guest_fn_b45ac(roll) - guest_fn_b45ac(pitch) * EF_F64(p) +
           EF_F64(p + 16u) * guest_fn_b45a2(pitch) * guest_fn_b45a2(roll) - EF_F64(origin + 16u);
    if (guest_fn_6e1c4((ef_addr32)(uintptr_t)v))
        guest_fn_6e3e0_w(0.2, 0.05);
    check_link_contact_772d0((int16_t)(EF_S32(g_current_object + OBJ_LINK_KEY) >> 16), (int16_t)g_grid_secondary_selector);
}
