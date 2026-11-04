/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Fourth x87 batch: matrix products, scaled integer conversions and scene reset. */

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

/* 0x88ddc, 115 bytes. Store three scaled doubles as angle words in an output record. */
void store_scaled_angles_88ddc(ef_addr32 out, double a0, double a1, double a2)
{
    int32_t x;
    int32_t y;

    a0 = a0 * EF_F64(0x0018a4b0u) * EF_F64(0x0018a4b8u);
    a1 = a1 * EF_F64(0x0018a4b0u) * EF_F64(0x0018a4b8u);
    a2 = a2 * EF_F64(0x0018a4c0u);
    x = (int32_t)(-a0);
    y = (int32_t)a1;
    EF_U16(out + 0xcu) = (uint16_t)x;
    EF_S32(out + 0x26u) = (int32_t)a2;
    EF_U16(out + 0xeu) = (uint16_t)y;
}

/* 0x88e50, 71 bytes. Multiply a vector of three doubles by a 3x3 double matrix (column form). */
void matrix_vector_88e50(ef_addr32 out, ef_addr32 vec, ef_addr32 m)
{
    EF_F64(out) = EF_F64(m + 0x18u) * EF_F64(vec + 8u) + EF_F64(m) * EF_F64(vec) + EF_F64(m + 0x30u) * EF_F64(vec + 0x10u);
    EF_F64(out + 8u) = EF_F64(m + 0x20u) * EF_F64(vec + 8u) + EF_F64(m + 8u) * EF_F64(vec) + EF_F64(m + 0x38u) * EF_F64(vec + 0x10u);
    EF_F64(out + 0x10u) = EF_F64(m + 0x28u) * EF_F64(vec + 8u) + EF_F64(m + 0x10u) * EF_F64(vec) + EF_F64(m + 0x40u) * EF_F64(vec + 0x10u);
}

/* 0x88f6c, 71 bytes. Multiply a vector of three doubles by a 3x3 double matrix (row form). */
void matrix_vector_88f6c(ef_addr32 out, ef_addr32 vec, ef_addr32 m)
{
    EF_F64(out) = EF_F64(m + 8u) * EF_F64(vec + 8u) + EF_F64(m) * EF_F64(vec) + EF_F64(m + 0x10u) * EF_F64(vec + 0x10u);
    EF_F64(out + 8u) = EF_F64(m + 0x20u) * EF_F64(vec + 8u) + EF_F64(m + 0x18u) * EF_F64(vec) + EF_F64(m + 0x28u) * EF_F64(vec + 0x10u);
    EF_F64(out + 0x10u) = EF_F64(m + 0x38u) * EF_F64(vec + 8u) + EF_F64(m + 0x30u) * EF_F64(vec) + EF_F64(m + 0x40u) * EF_F64(vec + 0x10u);
}

/* 0x8bf58, 91 bytes. Scaled distance from a unit to an integer vector, truncated; 0 if the unit is missing. */
int32_t unit_vector_distance_8bf58(int16_t unit, ef_addr32 vec)
{
    ef_addr32 u = guest_fn_7489c(unit);
    double v[3];
    double k1;
    double k2;

    if (u == 0)
        return 0;
    k1 = EF_F64(0x0018a885u);
    k2 = EF_F64(0x0018a88du);
    v[0] = (double)EF_S32(vec + 8u) * k1 * k2;
    v[1] = (double)EF_S32(vec) * k1 * k2;
    v[2] = (double)EF_S32(vec + 4u) * k1 * k2;
    return (int32_t)(guest_fn_b415c(u + 0x49u, (ef_addr32)(uintptr_t)v) * EF_F64(0x0018a895u));
}

/* 0x92f10, 135 bytes. Reset the scene record from the current object and the scene angle record. */
void reset_scene_record_92f10(void)
{
    ef_addr32 object;

    if (EF_U16(0x00211ae2u) == 0)
        guest_fn_18a68();
    object = guest_lookup_object_8ffe8(EF_S32(0x00211ae0u) >> 16);
    EF_S32(0x001a5bf0u) = (int32_t)*(float *)ef_guest_memory_ptr(0x001a5bb8u);
    init_bounds_d0b68(0x00214404u);
    if (object != 0) {
        EF_U32(0x0021446cu) = EF_U32(object);
        EF_U32(0x00214470u) = EF_U32(object + 4u);
        EF_U32(0x00214474u) = EF_U32(object + 8u);
        if (scene_lookup_named_word(EF_S32(object + 0x1eu) >> 16) != 0)
            EF_U16(0x0021448au) = (uint16_t)scene_lookup_named_word(EF_S32(object + 0x1eu) >> 16);
    }
    init_bounds_d0b68(0x00214378u);
    guest_fn_9357c();
}

/* 0x97534, 163 bytes. True when an object faces the camera closely enough and is active. */
uint8_t object_faces_camera_97534(ef_addr32 object)
{
    uint8_t a[0x40];
    ef_addr32 base = (ef_addr32)(uintptr_t)a;
    int32_t dot;

    init_bounds_d0b68(base);
    guest_fn_d0ebc(base, EF_U16(object + 0x24u));
    guest_fn_d0dbc(base, EF_U16(object + 0x28u), 0xffff0000u);
    guest_fn_d0fb8(base, EF_U16(object + 0x2cu), 0);
    guest_fn_ea864(base + 0x24u, 0, 0xffff0000u, 0);
    guest_fn_ea864(base + 0x30u, 0, 0xffff0000u, 0);
    guest_fn_ea9ec(base + 0x30u, base);
    dot = guest_fn_ea9b8(base + 0x30u, base + 0x24u);
    return (double)dot * EF_F64(0x0018ac5au) > EF_F64(0x0018ac62u) && EF_S32(object + 0x1cu) > 0;
}
