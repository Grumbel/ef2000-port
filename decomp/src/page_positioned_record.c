/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

typedef struct {
    uint32_t transformed[3], world[3];
} ef_positioned_record_frame;

EF_STATIC_ASSERT(positioned_record_world, offsetof(ef_positioned_record_frame, world) == 12);
EF_STATIC_ASSERT(positioned_record_frame_size, sizeof(ef_positioned_record_frame) == 24);

/* 0xa04b4, 344 bytes. AX=record index, DX=point index; EDX scratch.
 * Sole direct caller 9f516..9f521 reads the signed word pair from script.
 * EAX/EDX results are not consumed; preserve the other five GPRs. */
void start_positioned_page_record_a04b4(uint32_t record_index, uint32_t point_index)
{
    ef_positioned_record_frame storage;
    ef_addr32 frame = (ef_addr32)(uintptr_t)&storage;
    ef_addr32 world = frame + 12u;
    ef_addr32 record = EF_U32(0x00417174u) + 0x0bu
        + (uint32_t)(int32_t)(int16_t)record_index * 29u;
    ef_addr32 point = EF_U32(0x0041717cu) + 0x0au
        + (uint32_t)(int32_t)(int16_t)point_index * 6u;
    uint8_t flag;

    build_rotation_e994c(1u,
        (uint32_t)(EF_S32(0x0041718cu) >> 16),
        (uint32_t)(EF_S32(0x0041718eu) >> 16),
        (uint32_t)(EF_S32(0x00417190u) >> 16));
    transform_vector_e99b8(frame, EF_S16(point),
        EF_S32(point) >> 16, EF_S32(point + 2u) >> 16);
    EF_U32(world) = EF_U32(0x00417164u) + EF_U32(frame);
    EF_U32(world + 4u) = EF_U32(0x00417168u) + EF_U32(frame + 4u);
    EF_U32(world + 8u) = EF_U32(0x0041716cu) + EF_U32(frame + 8u);

    if ((uint8_t)rotate_view_test_160156(world, EF_U32(world + 8u),
            EF_U32(world + 4u), EF_U32(world),
            EF_U16(EF_U32(0x0041717cu) + 2u)) != 0u) {
        EF_U32(0x001600a8u) = (uint32_t)(int32_t)EF_S16(record);
        EF_U32(0x001600acu) = record + 0x14u;
        EF_U32(0x001600b0u) = world;
        flag = EF_U8(record + 0x1cu);
        EF_U32(0x001600b4u) = 0x0041718eu;
        /* Original tests once, then reads again on the nonzero path. */
        EF_U32(0x001600b8u) = flag != 0u ? EF_U8(record + 0x1cu) : 1u;
        EF_U32(0x001600c0u) = guest_callptr_stack1_ret_x(
            EF_U32(0x00132b14u), (uint32_t)(int32_t)EF_S16(record));
        guest_fn_160255_x();
    }
    guest_callptr4_x(EF_U32(0x00132ad8u),
        (uint32_t)(EF_S32(world) >> 16),
        (uint32_t)(EF_S32(world + 4u) >> 16),
        (uint32_t)(EF_S32(world + 8u) >> 16),
        EF_U8(EF_U32(0x00417174u) + 0x0au) & 0x0fu);
}
