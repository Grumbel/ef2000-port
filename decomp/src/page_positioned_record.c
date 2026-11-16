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

    if ((uint8_t)rotate_view_test_160156(
            EF_U16(EF_U32(0x0041717cu) + 2u), EF_U32(world + 8u),
            EF_U32(world + 4u), EF_U32(world), world) != 0u) {
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

/* 0xa08c8, 356 bytes. AX=record index; all six other GPRs preserved.
 * Sole direct caller 9e8e0/9e8e3 supplies the next script word. */
void start_rotated_page_record_a08c8(uint32_t record_index)
{
    ef_positioned_record_frame storage;
    ef_addr32 frame = (ef_addr32)(uintptr_t)&storage;
    ef_addr32 world = frame + 12u;
    ef_addr32 record, rotation;
    uint16_t x, y, z;
    uint8_t flag;

    if ((EF_S32(0x00417194u) >> 16) <= -0x400)
        return;
    if (EF_S16(0x00417196u) > 0)
        return;
    record = EF_U32(0x00417174u) + 0x0bu
        + (uint32_t)(int32_t)(int16_t)record_index * 29u;
    rotation = EF_U32(0x0041717cu)
        + (uint32_t)(EF_S32(record + 0x10u) >> 16) * 2u;
    guest_callptr_eaxedx_x(EF_U32(0x00132af4u), 0u, 0u);
    guest_callptr_eaxedx_x(EF_U32(0x00132af0u), frame, 0x00417164u);
    if ((uint8_t)rotate_view_test_160156(
            EF_U16(EF_U32(0x0041717cu) + 2u), EF_U32(frame + 8u),
            EF_U32(frame + 4u), EF_U32(frame), world) == 0u)
        return;

    x = EF_U16(0x0041718eu);
    y = EF_U16(0x00417222u);
    EF_U16(0x00417188u) = x;
    EF_U16(0x0041718au) = (uint16_t)(EF_U16(0x00417190u) + y);
    EF_U16(0x0041718cu) = EF_U16(0x00417192u);
    EF_U32(0x001600a8u) = (uint32_t)(int32_t)EF_S16(record);
    EF_U32(0x001600acu) = record + 0x14u;
    EF_U32(0x001600b0u) = world;
    EF_U32(0x001600b8u) = 1u;
    flag = EF_U8(record + 0x1cu);
    EF_U32(0x001600b4u) = 0x00417188u;
    if (flag != 0u) {
        /* Read all offsets before publishing any modified angle. */
        x = (uint16_t)(EF_U16(0x00417188u) + EF_U16(rotation));
        y = (uint16_t)(EF_U16(0x0041718au) + EF_U16(rotation + 2u));
        z = (uint16_t)(EF_U16(0x0041718cu) + EF_U16(rotation + 4u));
        EF_U16(0x00417188u) = x;
        EF_U16(0x0041718au) = y;
        flag = EF_U8(record + 0x1cu);
        EF_U16(0x0041718cu) = z;
        EF_U32(0x001600b8u) = flag;
    }
    EF_U32(0x001600c0u) = guest_callptr_stack1_ret_x(
        EF_U32(0x00132b14u), (uint32_t)(int32_t)EF_S16(record));
    guest_fn_160329_x();
}

/* 0xa0704, 452 bytes. AX=record index, DX=point index; EDX scratch.
 * Sole direct caller 9e8d6 loads the signed pair from the script. */
void start_offset_page_record_a0704(uint32_t record_index, uint32_t point_index)
{
    uint32_t storage[3];
    ef_addr32 world = (ef_addr32)(uintptr_t)storage;
    ef_addr32 record = EF_U32(0x00417174u) + 0x0bu
        + (uint32_t)(int32_t)(int16_t)record_index * 29u;
    ef_addr32 rotation = EF_U32(0x0041717cu)
        + (uint32_t)(EF_S32(record + 0x10u) >> 16) * 2u;
    ef_addr32 point = EF_U32(0x0041717cu) + 0x0au
        + (uint32_t)(int32_t)(int16_t)point_index * 6u;
    uint16_t x, y, z;
    uint32_t vx, vy, vz;
    uint8_t flag;

    EF_U16(0x00417226u) = EF_U16(point + 2u);
    guest_callptr_eaxedx_x(EF_U32(0x00132af4u),
        (uint32_t)(int32_t)EF_S16(point),
        (uint32_t)(EF_S32(point + 2u) >> 16));
    x = (uint16_t)(EF_U16(0x00417224u) + EF_U16(0x00417194u));
    y = (uint16_t)(EF_U16(0x00417226u) + EF_U16(0x00417196u));
    z = (uint16_t)(EF_U16(0x00417228u) + EF_U16(0x00417198u));
    EF_U16(0x00417224u) = x;
    vx = (uint32_t)(EF_S32(0x00417222u) >> 16);
    EF_U16(0x00417226u) = y;
    vy = (uint32_t)(EF_S32(0x00417224u) >> 16);
    EF_U16(0x00417228u) = z;
    vz = (uint32_t)(EF_S32(0x00417226u) >> 16);
    if ((uint8_t)rotate_view_test_160122(
            EF_U16(EF_U32(0x0041717cu) + 2u), vz, vy, vx, world) != 0u) {
        EF_U16(0x00417188u) = EF_U16(0x0041718eu);
        EF_U16(0x0041718au) = EF_U16(0x00417190u);
        EF_U16(0x0041718cu) = EF_U16(0x00417192u);
        EF_U32(0x001600a8u) = (uint32_t)(int32_t)EF_S16(record);
        EF_U32(0x001600acu) = record + 0x14u;
        EF_U32(0x001600b4u) = 0x00417188u;
        EF_U32(0x001600b0u) = world;
        flag = EF_U8(record + 0x1cu);
        EF_U32(0x001600b8u) = 1u;
        if (flag != 0u) {
            x = (uint16_t)(EF_U16(0x00417188u) + EF_U16(rotation));
            y = (uint16_t)(EF_U16(0x0041718au) + EF_U16(rotation + 2u));
            z = (uint16_t)(EF_U16(0x0041718cu) + EF_U16(rotation + 4u));
            EF_U16(0x00417188u) = x;
            EF_U16(0x0041718au) = y;
            flag = EF_U8(record + 0x1cu);
            EF_U16(0x0041718cu) = z;
            EF_U32(0x001600b8u) = flag;
        }
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

/* 0xa2728, 399 bytes. AX=record, DX/BX/CX=word coordinates.
 * Sole caller a26fc retains ESI/EDI; only ESI/EDI/EBP are preserved.
 * World output remains uninitialized if the original view test skips it. */
void start_coordinate_page_record_a2728(uint32_t record_index, uint32_t point_x,
    uint32_t point_y, uint32_t point_z)
{
    uint32_t storage[3];
    ef_addr32 world = (ef_addr32)(uintptr_t)storage;
    ef_addr32 record = EF_U32(0x00417174u) + 0x0bu
        + (uint32_t)(int32_t)(int16_t)record_index * 29u;
    ef_addr32 rotation = EF_U32(0x0041717cu)
        + (uint32_t)(EF_S32(record + 0x10u) >> 16) * 2u;
    uint16_t x, y, z;
    uint32_t vx, vy, vz;
    uint8_t flag;

    EF_U16(0x00417226u) = (uint16_t)point_y;
    guest_callptr_eaxedx_x(EF_U32(0x00132af4u),
        (uint32_t)(int32_t)(int16_t)point_x,
        (uint32_t)(int32_t)(int16_t)point_z);
    x = (uint16_t)(EF_U16(0x00417224u) + EF_U16(0x00417194u));
    y = (uint16_t)(EF_U16(0x00417226u) + EF_U16(0x00417196u));
    z = (uint16_t)(EF_U16(0x00417228u) + EF_U16(0x00417198u));
    EF_U16(0x00417224u) = x;
    vx = (uint32_t)(EF_S32(0x00417222u) >> 16) << 16;
    EF_U16(0x00417226u) = y;
    vy = (uint32_t)(EF_S32(0x00417224u) >> 16) << 16;
    EF_U16(0x00417228u) = z;
    vz = (uint32_t)(EF_S32(0x00417226u) >> 16) << 16;
    if ((uint8_t)rotate_view_test_160156(
            EF_U16(EF_U32(0x0041717cu) + 2u), vz, vy, vx, world) != 0u) {
        x = (uint16_t)(EF_U16(0x0041718eu) + EF_U16(rotation));
        y = EF_U16(rotation + 2u);
        EF_U16(0x00417188u) = x;
        y = (uint16_t)(EF_U16(0x00417190u) + y + EF_U16(0x00417222u));
        z = EF_U16(rotation + 4u);
        EF_U16(0x0041718au) = y;
        EF_U16(0x0041718cu) = (uint16_t)(EF_U16(0x00417192u) + z);
        EF_U32(0x001600a8u) = (uint32_t)(int32_t)EF_S16(record);
        EF_U32(0x001600acu) = record + 0x14u;
        EF_U32(0x001600b0u) = world;
        flag = EF_U8(record + 0x1cu);
        EF_U32(0x001600b4u) = 0x00417188u;
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

typedef struct {
    uint8_t header[12], position[12], reserved[12], rotation[12], tail[30];
} ef_page_chain_record;

EF_STATIC_ASSERT(page_chain_position, offsetof(ef_page_chain_record, position) == 12);
EF_STATIC_ASSERT(page_chain_rotation, offsetof(ef_page_chain_record, rotation) == 36);
EF_STATIC_ASSERT(page_chain_stride, sizeof(ef_page_chain_record) == 78);

/* 0xa0228, 651 bytes. AX=record, DX=point; EDX scratch.
 * Sole caller9e881 supplies script words; retain EBX/ECX/ESI/EDI/EBP. */
void start_chain_page_record_a0228(uint32_t record_index, uint32_t point_index)
{
    uint32_t storage[3];
    ef_addr32 world = (ef_addr32)(uintptr_t)storage;
    ef_addr32 record, rotation, point, entry;
    int32_t index;
    uint16_t x, y, z, bias;
    uint32_t vx, vy, vz, component;
    uint8_t flag;

    index = (int32_t)(int16_t)find_chain_entry_a0018((uint16_t)record_index,
        (uint16_t)(EF_S32(0x00417228u) >> 16));
    if (index == -1)
        return;
    record = EF_U32(0x00417174u) + 0x0bu
        + (uint32_t)(int32_t)(int16_t)record_index * 29u;
    rotation = EF_U32(0x0041717cu)
        + (uint32_t)(EF_S32(record + 0x10u) >> 16) * 2u;
    point = EF_U32(0x0041717cu) + 0x0au
        + (uint32_t)(int32_t)(int16_t)point_index * 6u;
    EF_U16(0x00417226u) = EF_U16(point + 2u);
    guest_callptr_eaxedx_x(EF_U32(0x00132af4u),
        (uint32_t)(int32_t)EF_S16(point),
        (uint32_t)(EF_S32(point + 2u) >> 16));
    x = (uint16_t)(EF_U16(0x00417224u) + EF_U16(0x00417194u));
    y = (uint16_t)(EF_U16(0x00417226u) + EF_U16(0x00417196u));
    z = (uint16_t)(EF_U16(0x00417228u) + EF_U16(0x00417198u));
    entry = 0x00416e40u + (uint32_t)index * 78u;
    EF_U16(0x00417228u) = z;
    EF_U16(0x00417224u) = x;
    component = (uint32_t)(EF_S32(entry + 12u) >> 8);
    x = (uint16_t)((uint32_t)(EF_S32(0x00417222u) >> 16) + component);
    EF_U16(0x00417226u) = y;
    EF_U16(0x00417224u) = x;
    component = (uint32_t)(EF_S32(entry + 16u) >> 8);
    EF_U16(0x00417226u) = (uint16_t)((uint32_t)(EF_S32(0x00417224u) >> 16) + component);
    component = (uint32_t)(EF_S32(entry + 20u) >> 8);
    EF_U16(0x00417228u) = (uint16_t)((uint32_t)(EF_S32(0x00417226u) >> 16) + component);
    vx = (uint32_t)(EF_S32(0x00417222u) >> 16) << 16;
    vy = (uint32_t)(EF_S32(0x00417224u) >> 16) << 16;
    vz = (uint32_t)(EF_S32(0x00417226u) >> 16) << 16;
    if ((uint8_t)rotate_view_test_160156(
            EF_U16(EF_U32(0x0041717cu) + 2u), vz, vy, vx, world) != 0u) {
        x = EF_U16(0x0041718eu);
        bias = EF_U16(0x00417222u);
        EF_U16(0x00417188u) = x;
        y = (uint16_t)(EF_U16(0x00417190u) + bias);
        x = EF_U16(0x00417188u);
        EF_U16(0x0041718au) = y;
        EF_U16(0x0041718cu) = EF_U16(0x00417192u);
        x = (uint16_t)(x + EF_U16(entry + 36u));
        y = EF_U16(0x0041718au);
        bias = EF_U16(entry + 40u);
        EF_U32(0x001600b4u) = 0x00417188u;
        y = (uint16_t)(y + bias);
        bias = EF_U16(entry + 44u);
        z = EF_U16(0x0041718cu);
        EF_U16(0x00417188u) = x;
        z = (uint16_t)(z + bias);
        EF_U32(0x001600a8u) = (uint32_t)(int32_t)EF_S16(record);
        EF_U16(0x0041718au) = y;
        EF_U32(0x001600acu) = record + 0x14u;
        EF_U32(0x001600b8u) = 1u;
        EF_U32(0x001600b0u) = world;
        flag = EF_U8(record + 0x1cu);
        EF_U16(0x0041718cu) = z;
        if (flag != 0u) {
            x = (uint16_t)(EF_U16(0x00417188u) + EF_U16(rotation));
            y = (uint16_t)(y + EF_U16(rotation + 2u));
            bias = EF_U16(rotation + 4u);
            EF_U16(0x00417188u) = x;
            z = (uint16_t)(z + bias);
            EF_U16(0x0041718au) = y;
            flag = EF_U8(record + 0x1cu);
            EF_U16(0x0041718cu) = z;
            EF_U32(0x001600b8u) = flag;
        }
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

/* 0x9f940,446 bytes. AX=record, DX=point; EDX scratch.
 * Sole direct caller9e847 supplies script words and retains ESI. */
void dispatch_offset_page_record_9f940(uint32_t record_index, uint32_t point_index)
{
    uint32_t storage[3];
    ef_addr32 world = (ef_addr32)(uintptr_t)storage;
    ef_addr32 record = EF_U32(0x00417174u) + 0x0bu
        + (uint32_t)(int32_t)(int16_t)record_index * 29u;
    ef_addr32 rotation = EF_U32(0x0041717cu)
        + (uint32_t)(EF_S32(record + 0x10u) >> 16) * 2u;
    ef_addr32 point = EF_U32(0x0041717cu) + 0x0au
        + (uint32_t)(int32_t)(int16_t)point_index * 6u;
    uint16_t x, y, z;
    uint32_t vx, vy, vz;
    uint8_t flag;

    EF_U16(0x00417226u) = EF_U16(point + 2u);
    guest_callptr_eaxedx_x(EF_U32(0x00132af4u),
        (uint32_t)(int32_t)EF_S16(point),
        (uint32_t)(EF_S32(point + 2u) >> 16));
    x = (uint16_t)(EF_U16(0x00417224u) + EF_U16(0x00417194u));
    y = (uint16_t)(EF_U16(0x00417226u) + EF_U16(0x00417196u));
    z = (uint16_t)(EF_U16(0x00417228u) + EF_U16(0x00417198u));
    EF_U16(0x00417224u) = x;
    vx = (uint32_t)(EF_S32(0x00417222u) >> 16);
    EF_U16(0x00417226u) = y;
    vy = (uint32_t)(EF_S32(0x00417224u) >> 16);
    EF_U16(0x00417228u) = z;
    vz = (uint32_t)(EF_S32(0x00417226u) >> 16);
    if ((uint8_t)rotate_view_test_160122(
            EF_U16(EF_U32(0x0041717cu) + 2u), vz, vy, vx, world) != 0u) {
        x = EF_U16(0x0041718eu);
        y = EF_U16(0x00417222u);
        EF_U16(0x00417188u) = x;
        EF_U16(0x0041718au) = (uint16_t)(EF_U16(0x00417190u) + y);
        EF_U16(0x0041718cu) = EF_U16(0x00417192u);
        EF_U32(0x001600a8u) = (uint32_t)(int32_t)EF_S16(record);
        EF_U32(0x001600b8u) = 1u;
        EF_U32(0x001600acu) = record + 0x14u;
        EF_U32(0x001600b0u) = world;
        flag = EF_U8(record + 0x1cu);
        EF_U32(0x001600b4u) = 0x00417188u;
        if (flag != 0u) {
            x = (uint16_t)(EF_U16(0x00417188u) + EF_U16(rotation));
            y = (uint16_t)(EF_U16(0x0041718au) + EF_U16(rotation + 2u));
            z = EF_U16(rotation + 4u);
            z = (uint16_t)(EF_U16(0x0041718cu) + z);
            EF_U16(0x00417188u) = x;
            EF_U16(0x0041718au) = y;
            flag = EF_U8(record + 0x1cu);
            EF_U16(0x0041718cu) = z;
            EF_U32(0x001600b8u) = flag;
        }
        guest_fn_16018a_x();
    }
    guest_callptr4_x(EF_U32(0x00132ad8u),
        (uint32_t)(EF_S32(world) >> 16),
        (uint32_t)(EF_S32(world + 4u) >> 16),
        (uint32_t)(EF_S32(world + 8u) >> 16),
        EF_U8(EF_U32(0x00417174u) + 0x0au) & 0x0fu);
}
