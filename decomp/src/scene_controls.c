/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

/* 0x8f620, 0x23 bytes, five caller functions. All three stores are words. */
void configure_scene_feature(int16_t index)
{
    g_render_feature_index = index;
    g_render_feature_limit = 50;
    g_render_feature_enabled = 1;
}

/* 0x8bf00, 0x1c bytes, two caller functions. The original sign-extends the
 * selector's high word for the filtered lookup; DL retains the incoming AL. */
void set_secondary_object_byte(uint8_t value)
{
    int16_t selected = (int16_t)(g_grid_secondary_selector >> 16);
    ef_addr32 object = lookup_filtered_indexed_object(selected);
    if (object != 0) {
        *(uint8_t *)ef_guest_memory_ptr(object + 0x1d7u) = value;
    }
}

/* 0x8bf1c, 0x39 bytes, five caller functions. With the gate enabled, the
 * signed EAX>>8 threshold switches comparison direction at g_scene_gate_limit.
 * In the original, only AL is a return value. */
uint8_t scene_gate_allows(int32_t value)
{
    if (g_scene_gate_enabled == 0) { return 1; }
    int32_t target = g_scene_gate_target_packed >> 16;
    int32_t shifted = value >> 8;
    if (target < g_scene_gate_limit) {
        return (uint8_t)(shifted >= target);
    }
    return (uint8_t)(shifted <= target);
}

/* 0x91d9c, 0x55 bytes, four caller functions. DX values passed to the
 * callbacks have their upper halves explicitly cleared. Callback order is
 * second, first, third, while mirrored fields are first, second, third. */
void configure_scene_angle_triplet(int16_t first, int16_t second, int16_t third)
{
    const ef_addr32 record = 0x214404;
    initialize_scene_angle_record(record);
    update_scene_angle_first(record, (uint16_t)second);
    update_scene_angle_second(record, (uint16_t)first);
    update_scene_angle_third(record, (uint16_t)third);
    g_scene_angle_saved_first = (uint16_t)first;
    g_scene_angle_saved_second = (uint16_t)second;
    g_scene_angle_saved_third = (uint16_t)third;
}
