/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

typedef struct {
    uint32_t world[3];
    ef_addr32 rotation, source;
    uint16_t channel, unused;
    uint32_t draw_count, update_count, update_index;
} ef_page_history_frame;
EF_STATIC_ASSERT(page_history_rotation, offsetof(ef_page_history_frame, rotation) == 12);
EF_STATIC_ASSERT(page_history_source, offsetof(ef_page_history_frame, source) == 16);
EF_STATIC_ASSERT(page_history_channel, offsetof(ef_page_history_frame, channel) == 20);
EF_STATIC_ASSERT(page_history_frame_size, sizeof(ef_page_history_frame) == 36);

static ef_addr32 page_history_state(ef_addr32 frame)
{
    uint32_t count = EF_U8(EF_U32(0x0041717cu) + 1u);
    ef_addr32 base = EF_U32(0x00417174u) + 11u + count * 29u;
    return base + (uint32_t)(int32_t)EF_S16(frame + 20u) * 2u;
}

/* 0xa0f84,1181 bytes. AX source, DX point, BX channel; preserves
 * ECX/ESI/EDI/EBP. Sole direct caller9f0fa advances its interpreter PC
 * in ESI before the call and consumes no return value. */
void update_page_history_a0f84(uint32_t source_index, uint32_t point_index,
                              uint32_t channel)
{
    ef_page_history_frame storage;
    ef_addr32 frame = (ef_addr32)(uintptr_t)&storage;
    ef_addr32 resource, state, source, rotation, point, entry;
    uint32_t index, identifier;
    uint16_t x, y, z, yaw, step, limit, life;
    int16_t count, pitch, speed, entry_yaw;
    int32_t setup_z, setup_x;
    uint8_t updates, repeat;

    EF_U16(frame + 20u) = (uint16_t)channel;
    resource = EF_U32(0x0041717cu);
    /* Entry uses the initially captured resource, unlike later reloads. */
    count = EF_U8(resource + 1u);
    state = EF_U32(0x00417174u) + 11u + (uint32_t)count * 29u
        + (uint32_t)(int32_t)(int16_t)channel * 2u;
    source = EF_U32(0x00417174u) + 11u
        + (uint32_t)(int32_t)(int16_t)source_index * 29u;
    rotation = resource + (uint32_t)(EF_S32(source + 16u) >> 16) * 2u;
    EF_U32(frame + 12u) = rotation;
    point = resource + 10u + (uint32_t)(int32_t)(int16_t)point_index * 6u;
    EF_U16(0x00417226u) = EF_U16(point + 2u);
    setup_z = EF_S32(point + 2u) >> 16;
    EF_U32(frame + 16u) = source;
    setup_x = EF_S16(point);
    guest_callptr_eaxedx_x(EF_U32(0x00132af4u),
        (uint32_t)setup_x, (uint32_t)setup_z);

    step = EF_U16(0x00417194u);
    x = EF_U16(0x00417224u);
    y = EF_U16(0x00417226u);
    z = EF_U16(0x00417228u);
    x = (uint16_t)(x + step);
    step = EF_U16(0x00417196u);
    EF_U16(0x00417224u) = x;
    y = (uint16_t)(y + step);
    step = EF_U16(0x00417198u);
    EF_U16(0x00417226u) = y;
    z = (uint16_t)(z + step);
    updates = EF_U8(0x00137a00u);
    EF_U16(0x00417228u) = z;
    if (updates != 0u) {
        if (EF_U16(state) != 0u) {
            step = EF_U16(state + 6u);
            yaw = (uint16_t)(EF_U16(state + 8u) + step);
            limit = EF_U16(state + 2u);
            EF_U16(state + 8u) = yaw;
            if (yaw == limit || yaw == EF_U16(state + 4u))
                EF_U16(state + 6u) = (uint16_t)(0u - EF_U16(state + 6u));
            count = EF_S16(state + 20u);
            if (count < EF_S16(state + 14u)) {
                if (count != 0) {
                    entry = state + 22u + (uint32_t)(EF_S32(state + 18u) >> 16) * 10u;
                    count = EF_S16(state + 20u);
                    index = 0u;
                    if (count > 0) {
                        do {
                            EF_U16(entry) = EF_U16(entry - 10u);
                            EF_U16(entry + 2u) = EF_U16(entry - 8u);
                            EF_U16(entry + 4u) = EF_U16(entry - 6u);
                            EF_U16(entry + 6u) = EF_U16(entry - 4u);
                            EF_U16(entry + 8u) = EF_U16(entry - 2u);
                            ++index;
                            count = EF_S16(state + 20u);
                            entry -= 10u;
                        } while ((int16_t)index < count);
                    }
                }
                entry = state + 22u;
                life = EF_U16(state + 16u);
                yaw = EF_U16(0x00417222u);
                EF_U16(entry) = life;
                yaw = (uint16_t)(yaw + EF_U16(0x00417190u));
                yaw = (uint16_t)(yaw + EF_U16(state + 8u));
                EF_U16(entry + 4u) = EF_U16(0x00417224u);
                EF_U16(entry + 6u) = EF_U16(0x00417226u);
                EF_U16(entry + 8u) = EF_U16(0x00417228u);
                source = EF_U32(frame + 16u);
                EF_U16(entry + 2u) = yaw;
                if (EF_U8(source + 28u) != 0u) {
                    rotation = EF_U32(frame + 12u);
                    yaw = EF_U16(entry + 2u);
                    yaw = (uint16_t)(yaw + EF_U16(rotation + 2u));
                    EF_U16(entry + 2u) = yaw;
                }
                EF_U16(state + 20u) = (uint16_t)(EF_U16(state + 20u) + 1u);
            }
        }
        count = EF_S16(state + 20u);
        if (count != 0) {
            EF_U32(frame + 28u) = (uint16_t)count;
            entry = state + 22u;
            EF_U32(frame + 32u) = 0u;
            if (count > 0) {
                do {
                    life = (uint16_t)(EF_U16(entry) - 1u);
                    EF_U16(entry) = life;
                    if ((int16_t)life <= 0)
                        EF_U16(state + 20u) = (uint16_t)(EF_U16(state + 20u) - 1u);
                    speed = (int16_t)(EF_S32(state + 16u) >> 16);
                    entry_yaw = (int16_t)(EF_S32(entry) >> 16);
                    pitch = (int16_t)(EF_S32(state + 8u) >> 16);
                    add_direction_offset_cee88(entry + 4u, (uint32_t)(int32_t)pitch,
                        (uint32_t)(int32_t)entry_yaw, speed);
                    entry += 10u;
                    index = EF_U32(frame + 32u) + 1u;
                    count = EF_S16(frame + 28u);
                    EF_U32(frame + 32u) = index;
                } while ((int16_t)index < count);
            }
        }
    }

    x = (uint16_t)(EF_S32(0x00417222u) >> 16);
    y = (uint16_t)(EF_S32(0x00417224u) >> 16);
    z = (uint16_t)(EF_S32(0x00417226u) >> 16);
    if ((uint8_t)rotate_view_test_160122(EF_U16(EF_U32(0x0041717cu) + 2u),
            (uint32_t)(int32_t)(int16_t)z, (uint32_t)(int32_t)(int16_t)y,
            (uint32_t)(int32_t)(int16_t)x, frame) != 0u) {
        x = EF_U16(0x0041718eu);
        yaw = EF_U16(0x00417222u);
        EF_U16(0x00417188u) = x;
        yaw = (uint16_t)(yaw + EF_U16(0x00417190u));
        yaw = (uint16_t)(yaw + EF_U16(state + 8u));
        EF_U16(0x0041718cu) = EF_U16(0x00417192u);
        source = EF_U32(frame + 16u);
        EF_U32(0x001600a8u) = (uint32_t)(int32_t)EF_S16(source);
        source = EF_U32(frame + 16u);
        EF_U32(0x001600acu) = source + 20u;
        EF_U32(0x001600b8u) = 1u;
        EF_U32(0x001600b0u) = frame;
        source = EF_U32(frame + 16u);
        EF_U16(0x0041718au) = yaw;
        repeat = EF_U8(source + 28u);
        EF_U32(0x001600b4u) = 0x00417188u;
        if (repeat != 0u) {
            rotation = EF_U32(frame + 12u);
            x = (uint16_t)(EF_U16(0x00417188u) + EF_U16(rotation));
            rotation = EF_U32(frame + 12u);
            y = (uint16_t)(EF_U16(0x0041718au) + EF_U16(rotation + 2u));
            rotation = EF_U32(frame + 12u);
            source = EF_U32(frame + 16u);
            z = (uint16_t)(EF_U16(0x0041718cu) + EF_U16(rotation + 4u));
            EF_U16(0x00417188u) = x;
            EF_U16(0x0041718au) = y;
            repeat = EF_U8(source + 28u);
            EF_U16(0x0041718cu) = z;
            EF_U32(0x001600b8u) = repeat;
        }
        source = EF_U32(frame + 16u);
        EF_U32(0x001600c0u) = guest_callptr_stack1_ret_x(
            EF_U32(0x00132b14u), (uint32_t)(int32_t)EF_S16(source));
        guest_fn_160255_x();
        state = page_history_state(frame);
        count = EF_S16(state + 20u);
        if (count != 0) {
            EF_U32(frame + 24u) = (uint16_t)count;
            EF_U16(0x00417188u) = EF_U16(state + 10u);
            index = 0u;
            EF_U16(0x0041718cu) = 0u;
            if (count > 0) {
                do {
                    entry = state + 22u + (uint32_t)(int32_t)(int16_t)index * 10u;
                    x = (uint16_t)(EF_S32(entry + 2u) >> 16);
                    y = (uint16_t)(EF_S32(entry + 4u) >> 16);
                    z = (uint16_t)(EF_S32(entry + 6u) >> 16);
                    if ((uint8_t)rotate_view_test_160122(200u,
                            (uint32_t)(int32_t)(int16_t)z,
                            (uint32_t)(int32_t)(int16_t)y,
                            (uint32_t)(int32_t)(int16_t)x, frame) != 0u) {
                        EF_U16(0x0041718au) = EF_U16(entry + 2u);
                        EF_U32(0x001600a8u) = (uint32_t)(EF_S32(state + 10u) >> 16);
                        EF_U32(0x001600b0u) = frame;
                        source = EF_U32(frame + 16u);
                        EF_U32(0x001600acu) = 0x00132b3cu;
                        identifier = (uint32_t)(int32_t)EF_S16(source);
                        EF_U32(0x001600b4u) = 0x00417188u;
                        EF_U32(0x001600b8u) = 1u;
                        EF_U32(0x001600c0u) = guest_callptr_stack1_ret_x(
                            EF_U32(0x00132b14u), identifier);
                        guest_fn_160255_x();
                    }
                    state = page_history_state(frame);
                    count = EF_S16(frame + 24u);
                    ++index;
                } while ((int16_t)index < count);
            }
        }
    }
    guest_callptr4_x(EF_U32(0x00132ad8u),
        (uint32_t)(EF_S32(frame) >> 16), (uint32_t)(EF_S32(frame + 4u) >> 16),
        (uint32_t)(EF_S32(frame + 8u) >> 16),
        EF_U8(EF_U32(0x00417174u) + 10u) & 15u);
}
