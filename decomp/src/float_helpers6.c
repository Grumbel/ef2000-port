/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Sixth x87 batch: vector algebra helpers and ratio counters. */

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

/* 0xa3824, 164 bytes. Dispatch on the balance ratio of a table entry. */
void dispatch_entry_ratio_a3824(uint8_t index)
{
    ef_addr32 entry = EF_U32(0x0013587cu + ((uint32_t)index << 5));
    double r = (double)EF_S32(entry + 3u) / (double)EF_U16(entry + 1u)
               - (double)EF_S32(entry + 9u) / (double)EF_U16(entry + 7u);
    uint8_t kind = EF_U8(entry);

    if (r > EF_F64(0x0018cd08u) || !(r >= EF_F64(0x0018cd10u))) {
        guest_fn_a3804(kind);
        return;
    }
    if (r > EF_F64(0x0018cd18u)) {
        guest_fn_a37c4(kind);
        return;
    }
    if (-r > EF_F64(0x0018cd18u))
        guest_fn_a37e4(kind);
}

/* 0xb0718, 156 bytes. Ratio of listed entries of kind 1 or 2 to all matching entries. */
double match_ratio_b0718(uint8_t type, uint32_t arg, ef_addr32 listp)
{
    double n1 = 0.0;
    double n2 = 0.0;
    ef_addr32 node;

    for (node = EF_U32(listp); node != 0; node = EF_U32(node + 4u)) {
        if (EF_U8(node + 0xbu) == 0 && (uint16_t)guest_fn_cc1ac(type, EF_U8(node + 0xeu), arg) != 0) {
            if (EF_U8(node + 0xdu) == 1 || EF_U8(node + 0xdu) == 2)
                n1 = 1.0 + n1;
            n2 = 1.0 + n2;
        }
    }
    if (n2 == 0.0)
        return 0.0;
    return n1 / n2;
}

/* 0xb07b4, 156 bytes. Same ratio using the second matching predicate. */
double match_ratio_b07b4(uint8_t type, uint32_t arg, ef_addr32 listp)
{
    double n1 = 0.0;
    double n2 = 0.0;
    ef_addr32 node;

    for (node = EF_U32(listp); node != 0; node = EF_U32(node + 4u)) {
        if (EF_U8(node + 0xbu) == 0 && (uint16_t)guest_fn_cc22c(type, EF_U8(node + 0xeu), arg) != 0) {
            if (EF_U8(node + 0xdu) == 1 || EF_U8(node + 0xdu) == 2)
                n1 = 1.0 + n1;
            n2 = 1.0 + n2;
        }
    }
    if (n2 == 0.0)
        return 0.0;
    return n1 / n2;
}

/* 0xb43dc, 21 bytes. Dot product of two 3D double vectors. */
double dot_product_b43dc(ef_addr32 a, ef_addr32 b)
{
    return EF_F64(a + 8u) * EF_F64(b + 8u) + EF_F64(a) * EF_F64(b) + EF_F64(a + 0x10u) * EF_F64(b + 0x10u);
}

/* 0xb4424, 68 bytes. Normalise a 3D vector into out; false when it is too short. */
uint8_t normalize_vector_b4424(ef_addr32 out, ef_addr32 in)
{
    double len = vector_length_b4364(in);
    double inv;

    if (!(len >= EF_F64(0x0018e8c5u)))
        return 0;
    inv = 1.0 / len;
    EF_F64(out) = EF_F64(in) * inv;
    EF_F64(out + 8u) = EF_F64(in + 8u) * inv;
    EF_F64(out + 0x10u) = inv * EF_F64(in + 0x10u);
    return 1;
}

/* 0xb4468, 50 bytes. Projection of a vector onto the unit direction of another. */
void project_vector_b4468(ef_addr32 out, ef_addr32 dir, ef_addr32 vec)
{
    double unit[3];
    double d;

    normalize_vector_b4424((ef_addr32)(uintptr_t)unit, dir);
    d = dot_product_b43dc((ef_addr32)(uintptr_t)unit, vec);
    EF_F64(out) = unit[0] * d;
    EF_F64(out + 8u) = unit[1] * d;
    EF_F64(out + 0x10u) = d * unit[2];
}

/* 0xb449c, 49 bytes. Component of a vector perpendicular to a direction. */
void reject_vector_b449c(ef_addr32 out, ef_addr32 dir, ef_addr32 vec)
{
    double proj[3];

    project_vector_b4468((ef_addr32)(uintptr_t)proj, dir, vec);
    EF_F64(out) = EF_F64(vec) - proj[0];
    EF_F64(out + 8u) = EF_F64(vec + 8u) - proj[1];
    EF_F64(out + 0x10u) = EF_F64(vec + 0x10u) - proj[2];
}

/* 0xb4620, 91 bytes. Two-argument arctangent (y, x); 0.0 when both are zero. */
double atan2_b4620(double y, double x)
{
    uint32_t yhi = ((uint32_t *)&y)[1] & 0x7fffffffu;
    uint32_t ylo = ((uint32_t *)&y)[0];
    uint32_t xhi = ((uint32_t *)&x)[1] & 0x7fffffffu;
    uint32_t xlo = ((uint32_t *)&x)[0];

    if (yhi == 0 && ylo == 0 && xhi == 0 && xlo == 0)
        return 0.0;
    return guest_fn_dc1e9(y, x);
}

/* 0xcbfa0, 79 bytes. Default range of an object: 400 for kinds 1 and 3, else 300. */
int32_t default_range_cbfa0(ef_addr32 object)
{
    uint8_t kind = EF_U8(object + 0xbu);

    if (kind == 1 || kind == 3)
        return 400;
    return 300;
}

/* 0xce0b0, 25 bytes. Scale a value relative to the horizontal viewport origin and size. */
double scale_view_x_ce0b0(float value, float factor)
{
    return ((double)value - (double)EF_S32(0x0015bdb8u)) * (double)factor / (double)EF_S32(0x0015bdd4u);
}

/* 0xce0cc, 25 bytes. Scale a value relative to the vertical viewport origin and size. */
double scale_view_y_ce0cc(float value, float factor)
{
    return ((double)value - (double)EF_S32(0x0015bdbcu)) * (double)factor / (double)EF_S32(0x0015bdd8u);
}
