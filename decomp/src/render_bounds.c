/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* Accessed prefix only, not the complete render-object allocation. */
typedef struct {
    uint8_t header[0x10];
    int32_t min_x, max_x, min_y, max_y;
    uint8_t dispatch[0x0c];
    uint16_t flags;
    uint8_t reserved[2];
    int32_t min_3x, min_3y, min_3z, max_3x, max_3y, max_3z;
} ef_render_bounds_prefix;

EF_STATIC_ASSERT(render_bounds_min_x, offsetof(ef_render_bounds_prefix, min_x) == 0x10);
EF_STATIC_ASSERT(render_bounds_max_x, offsetof(ef_render_bounds_prefix, max_x) == 0x14);
EF_STATIC_ASSERT(render_bounds_min_y, offsetof(ef_render_bounds_prefix, min_y) == 0x18);
EF_STATIC_ASSERT(render_bounds_max_y, offsetof(ef_render_bounds_prefix, max_y) == 0x1c);
EF_STATIC_ASSERT(render_bounds_flags, offsetof(ef_render_bounds_prefix, flags) == 0x2c);
EF_STATIC_ASSERT(render_bounds_min_3x, offsetof(ef_render_bounds_prefix, min_3x) == 0x30);
EF_STATIC_ASSERT(render_bounds_min_3y, offsetof(ef_render_bounds_prefix, min_3y) == 0x34);
EF_STATIC_ASSERT(render_bounds_min_3z, offsetof(ef_render_bounds_prefix, min_3z) == 0x38);
EF_STATIC_ASSERT(render_bounds_max_3x, offsetof(ef_render_bounds_prefix, max_3x) == 0x3c);
EF_STATIC_ASSERT(render_bounds_max_3y, offsetof(ef_render_bounds_prefix, max_3y) == 0x40);
EF_STATIC_ASSERT(render_bounds_max_3z, offsetof(ef_render_bounds_prefix, max_3z) == 0x44);

static uint8_t bounds_plane_test(ef_addr32 callback_slot, ef_addr32 plane,
                               uint32_t x, uint32_t y, uint32_t z)
{
    /* Original callback registers: EAX=plane, EDX=x, EBX=y, ECX=z; AL result. */
    return (uint8_t)guest_callptr4_ret_x(EF_U32(callback_slot), plane, x, y, z);
}

/* 0xd2130, 416 bytes. EAX=object; preserve every other GPR. Accumulate
 * classification flags without clearing old bits, as in the original. */
void classify_render_bounds_d2130(ef_addr32 object)
{
    if ((EF_U8(object + 0x2cu) & 1u) != 0u) {
        int32_t clip_min_x;

        compute_vertex_bounds_d1fc0(object);
        clip_min_x = EF_S32(0x004208bcu);
        if (clip_min_x > EF_S32(object + 0x14u) ||
            EF_S32(0x004208b8u) < EF_S32(object + 0x10u) ||
            EF_S32(0x004208c0u) > EF_S32(object + 0x1cu) ||
            EF_S32(0x004208b4u) < EF_S32(object + 0x18u)) {
            EF_U8(object + 0x2cu) |= 8u;
            return;
        }
        if (clip_min_x > EF_S32(object + 0x10u))
            EF_U8(object + 0x2cu) |= 0x10u;
        if (EF_S32(0x004208b8u) < EF_S32(object + 0x14u))
            EF_U8(object + 0x2cu) |= 0x20u;
        if (EF_S32(0x004208c0u) > EF_S32(object + 0x18u))
            EF_U8(object + 0x2cu) |= 0x40u;
        if (EF_S32(0x004208b4u) < EF_S32(object + 0x1cu))
            EF_U8(object + 0x2cu) |= 0x80u;
        return;
    }

    compute_point_bounds_d2040(object);
    /* Preserve plane order and short-circuit rejection. Each call reads
     * current object bounds; callbacks are not assumed to be pure. */
    if (bounds_plane_test(0x0042087cu, 0x00420868u, 0u, 0u, EF_U32(object + 0x44u)) ||
        bounds_plane_test(0x0042083cu, 0x00420828u, EF_U32(object + 0x3cu), 0u, EF_U32(object + 0x44u)) ||
        bounds_plane_test(0x0042089cu, 0x00420888u, EF_U32(object + 0x30u), 0u, EF_U32(object + 0x44u)) ||
        bounds_plane_test(0x0042085cu, 0x00420848u, 0u, EF_U32(object + 0x34u), EF_U32(object + 0x44u)) ||
        bounds_plane_test(0x0042081cu, 0x00420808u, 0u, EF_U32(object + 0x40u), EF_U32(object + 0x44u))) {
        EF_U8(object + 0x2cu) |= 8u;
        return;
    }
    if (bounds_plane_test(0x0042087cu, 0x00420868u, 0u, 0u, EF_U32(object + 0x38u)))
        EF_U8(object + 0x2du) |= 1u;
    if (bounds_plane_test(0x0042083cu, 0x00420828u, EF_U32(object + 0x30u), 0u, EF_U32(object + 0x38u)))
        EF_U8(object + 0x2cu) |= 0x10u;
    if (bounds_plane_test(0x0042089cu, 0x00420888u, EF_U32(object + 0x3cu), 0u, EF_U32(object + 0x38u)))
        EF_U8(object + 0x2cu) |= 0x20u;
    if (bounds_plane_test(0x0042081cu, 0x00420808u, 0u, EF_U32(object + 0x34u), EF_U32(object + 0x38u)))
        EF_U8(object + 0x2cu) |= 0x40u;
    if (bounds_plane_test(0x0042085cu, 0x00420848u, 0u, EF_U32(object + 0x40u), EF_U32(object + 0x38u)))
        EF_U8(object + 0x2cu) |= 0x80u;
}
