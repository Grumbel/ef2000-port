/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Matrix transforms, palette blend and unit scans, 0x64000..0x77534. */

/* Rotate a vector by a 3x3 matrix of 16.15 coefficients stored column-wise. */
static void rotate_vector_15(ef_addr32 vector, ef_addr32 matrix)
{
    uint32_t x = EF_U32(vector);
    uint32_t y = EF_U32(vector + 4u);
    uint32_t z = EF_U32(vector + 8u);
    uint32_t t0 = EF_U32(matrix + 0x0cu) * y + x * EF_U32(matrix) + z * EF_U32(matrix + 0x18u);
    uint32_t t1 = x * EF_U32(matrix + 4u) + y * EF_U32(matrix + 0x10u) + z * EF_U32(matrix + 0x1cu);
    uint32_t t2 = x * EF_U32(matrix + 8u) + y * EF_U32(matrix + 0x14u) + z * EF_U32(matrix + 0x20u);

    EF_S32(vector) = (int32_t)t0 >> 15;
    EF_S32(vector + 4u) = (int32_t)t1 >> 15;
    EF_S32(vector + 8u) = (int32_t)t2 >> 15;
}

/* 0x6456c, 171 bytes. Rotate a vector by the matrix at 0x1a7c30. */
void rotate_vector_6456c(ef_addr32 vector)
{
    rotate_vector_15(vector, 0x001a7c30u);
}

/* 0x64618, 171 bytes. Rotate a vector by the matrix at 0x1a5bc0. */
void rotate_vector_64618(ef_addr32 vector)
{
    rotate_vector_15(vector, 0x001a5bc0u);
}

/* 0x663a4, 186 bytes. Find the next unit after an index that matches a team mode; return its id or -1. */
uint32_t find_next_unit_663a4(uint32_t start, uint8_t mode)
{
    uint16_t remaining = EF_U16(0x002119f4u);
    int16_t index = (int16_t)start;
    ef_addr32 unit = 0;
    uint16_t step = 0;

    EF_U16(0x001a822au) = 0;
    while (remaining != 0) {
        step = 0;
        index++;
        if (index >= (int16_t)EF_U16(0x002119f4u))
            index = 0;
        unit = guest_fn_7489c((int32_t)index);
        if (unit != 0) {
            if ((EF_U8(unit + 0x24u) & 8u) != 0) {
                unit = 0;
            } else {
                ef_addr32 owner = EF_U32(0x0019bb58u) + (uint32_t)((int32_t)(int16_t)EF_U16(unit + 0x28u) * 0xca);

                if (EF_U8(owner + 0x20u) == 0) {
                    unit = 0;
                } else if (mode == 1) {
                    if (EF_U16(0x00197250u) == EF_U16(owner + 0x15u))
                        ; /* kept */
                    else
                        unit = 0;
                } else if (mode == 2) {
                    if (EF_U16(0x00197250u) == EF_U16(owner + 0x15u))
                        unit = 0;
                }
            }
        }
        EF_U16(0x001a822au) = (uint16_t)(EF_U16(0x001a822au) + 1u);
        remaining--;
        if (unit != 0)
            break;
    }
    (void)step;
    if (unit != 0)
        return EF_U16(unit + 0x16u);
    return 0xffffffffu;
}

/* 0x69680, 189 bytes. Transform (x, y) by the 3x3 matrix at 0x15ffda and add the translation pair. */
void transform_point_69680(ef_addr32 out_xyz, ef_addr32 out_base, int16_t x, int16_t y)
{
    int32_t px = x;
    int32_t py = y;
    uint32_t a = EF_U32(0x0015ffe6u) * (uint32_t)py + EF_U32(0x0015ffdau) * (uint32_t)px + (EF_U32(0x0015fff2u) << 10);
    uint32_t b = EF_U32(0x0015ffeau) * (uint32_t)py + EF_U32(0x0015ffdeu) * (uint32_t)px + (EF_U32(0x0015fff6u) << 10);
    uint32_t c = EF_U32(0x0015ffe2u) * (uint32_t)px + (uint32_t)py * EF_U32(0x0015ffeeu) + (EF_U32(0x0015fffau) << 10);

    EF_U16(out_base) = EF_U16(0x00159094u);
    EF_U16(out_base + 2u) = EF_U16(0x00159098u);
    EF_U16(out_base + 4u) = EF_U16(0x0015909cu);
    EF_U16(out_xyz) = (uint16_t)(((int32_t)EF_U32(0x00159092u) >> 16) + ((int32_t)a >> 11));
    EF_U16(out_xyz + 2u) = (uint16_t)(((int32_t)EF_U32(0x00159096u) >> 16) + ((int32_t)b >> 11));
    EF_U16(out_xyz + 4u) = (uint16_t)(((int32_t)EF_U32(0x0015909au) >> 16) + ((int32_t)c >> 11));
}

/* 0x758c0, 186 bytes. Blend a 256-entry RGB palette towards a target colour by factor/50. */
void blend_palette_758c0(uint16_t factor, ef_addr32 source, ef_addr32 dest, uint8_t red,
                         uint8_t stack_green, uint8_t stack_blue)
{
    int32_t scale = (int16_t)factor;
    int16_t i;

    for (i = 0; i < 0x100; i++) {
        ef_addr32 in = source + (uint32_t)((int32_t)i * 3);
        ef_addr32 out = dest + (uint32_t)((int32_t)i * 3);

        EF_U8(out) = (uint8_t)(((((int32_t)(int8_t)EF_U8(in) - (int32_t)red) * scale) / 0x32 + (int32_t)red) & 0x3f);
        EF_U8(out + 1u) = (uint8_t)(((((int32_t)(int8_t)EF_U8(in + 1u) - (int32_t)stack_green) * scale) / 0x32 + (int32_t)stack_green) & 0x3f);
        EF_U8(out + 2u) = (uint8_t)(((((int32_t)(int8_t)EF_U8(in + 2u) - (int32_t)stack_blue) * scale) / 0x32 + (int32_t)stack_blue) & 0x3f);
    }
}

/* 0x77534, 187 bytes. Trigger the linked-unit event for a unit and notify the matching display slots. */
void trigger_linked_event_77534(uint32_t id)
{
    int32_t index = (int16_t)id;
    ef_addr32 unit = guest_fn_7489c(index);
    ef_addr32 linked;

    if (unit == 0)
        return;
    linked = guest_fn_7489c((int32_t)EF_U32(unit + 0x186u) >> 16);
    if (linked == 0)
        return;
    EF_U8(unit + 0x18au) = 1;
    guest_fn_6a1d0(index, 0x0e);
    if (EF_U8(0x00125847u) != 0)
        guest_fn_9e508((int32_t)EF_U32(unit + 0x1eu) >> 16, 0x16);
    if ((uint16_t)id != EF_U16(0x001f71cau))
        return;
    guest_fn_801e4(1, 1, 1);
    guest_fn_801e4(0x2f, 0, 1);
    if (EF_U8(unit + 0x18bu) == 0)
        guest_fn_9e508((int32_t)EF_U32(linked + 0x1eu) >> 16, 0x0d);
    else if (EF_U8(unit + 0x18bu) == 1)
        guest_fn_9e508((int32_t)EF_U32(linked + 0x1eu) >> 16, 0x0f);
}
