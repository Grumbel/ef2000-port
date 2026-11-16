/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* Prefix view only; later vertex fields are not described here. */
typedef struct {
    uint8_t position_and_projection[0x20];
    uint32_t flags;
    uint32_t texture_u;
    uint32_t texture_v;
} ef_textured_vertex;

EF_STATIC_ASSERT(textured_vertex_flags, offsetof(ef_textured_vertex, flags) == 0x20);
EF_STATIC_ASSERT(textured_vertex_u, offsetof(ef_textured_vertex, texture_u) == 0x24);
EF_STATIC_ASSERT(textured_vertex_v, offsetof(ef_textured_vertex, texture_v) == 0x28);

/* 0xd2a7c, 518 bytes. ESI is the word-index cursor and result; ECX is count.
 * Preserve EAX/EBX/EDX/EDI/EBP. The atlas mode biases both texture axes and
 * selects the alternate span callback before processing the render object. */
ef_addr32 build_atlas_textured_object_d2a7c(ef_addr32 list, uint32_t count)
{
    uint32_t atlas = EF_U8(0x0015b960u);
    uint32_t shift;
    uint32_t bias = 0u;
    uint32_t i;
    ef_addr32 slot = 0x004209d0u;

    EF_U32(0x0042099cu) = 1u;
    EF_U32(0x00420974u) = 0u;
    EF_U32(0x004209bcu) = slot;
    EF_U32(slot) = count;
    shift = EF_U32(0x00158f0cu) != 0u ? 9u : 16u;
    EF_U32(0x00420990u) = 0x001016e0u;
    EF_U32(0x00420994u) = 0x000d5a90u;
    if (atlas != 0u) {
        bias = atlas << 15;
        EF_U32(0x00420994u) = 0x000d53ffu;
        EF_U32(0x0015b95cu) = 0x00433f34u + EF_U32(0x00160000u) * 0x4004u;
    }
    for (i = 0u; (int32_t)i < (int32_t)count; ++i) {
        ef_addr32 vertex = EF_U32(0x0017bf70u) + EF_U16(list);
        uint32_t u = (uint32_t)EF_U16(0x0015997cu + i * 4u) << shift;
        uint32_t v;

        EF_U32(vertex + 0x24u) = u - bias;
        v = (uint32_t)EF_U16(0x0015997eu + i * 4u) << shift;
        if (atlas == 0u) {
            uint8_t flags = (uint8_t)(EF_U8(vertex + 0x20u) | 2u);

            /* Original non-atlas path writes bit 1 before storing V. */
            EF_U8(vertex + 0x20u) = flags;
            EF_U32(vertex + 0x28u) = v;
            EF_U8(vertex + 0x20u) = (uint8_t)(flags & 0xfbu);
        } else {
            EF_U32(vertex + 0x28u) = v - bias;
            EF_U8(vertex + 0x20u) = (uint8_t)((EF_U8(vertex + 0x20u) | 2u) & 0xfbu);
        }
        EF_U32(0x0042099cu) = EF_U32(0x0042099cu) & EF_U32(vertex + 0x20u);
        list += 2u;
        EF_U32(slot + 4u) = vertex;
        slot += 4u;
    }
    process_render_object_d23d8(0x00420970u);
    return list;
}
