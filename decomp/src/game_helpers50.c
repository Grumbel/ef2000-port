/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Effect records, object moves and angle limits, 0x8ff48..0x94954.
 * In the flat 32-bit guest, host stack buffers are valid guest pointers. */

/* 0x8ff48, 159 bytes. Create an effect record, link it at the head of the list and number it. */
uint16_t create_effect_record_8ff48(void)
{
    ef_addr32 record = ((uint32_t (*)(uint32_t, uint32_t))EF_U32(0x0012dd9cu))(0x60u, 0);
    ef_addr32 head;
    int32_t index;

    EF_U32(record + 0x4cu) = 0;
    head = EF_U32(0x00211ad4u);
    EF_U16(record + 0x1au) = 1;
    EF_U32(record + 0x50u) = head;
    if (head != 0)
        EF_U32(head + 0x4cu) = record;
    EF_U32(0x00211ad4u) = record;
    if (((int32_t)EF_U32(0x00211adcu) >> 16) > 0x3770)
        ui_report_missing_record(0x0018aa9cu);
    index = (int32_t)EF_U32(0x00211adcu) >> 16;
    EF_U32(EF_U32(0x00211ad0u) + (uint32_t)(index * 4) - 4u) = record;
    EF_U16(record + 0x18u) = 0;
    EF_U16(record + 0x16u) = (uint16_t)(EF_U16(0x00211adeu) + 0x2000u);
    EF_U16(0x00211ae0u) = (uint16_t)(EF_U16(0x00211ae0u) + 1u);
    EF_U16(0x00211adeu) = (uint16_t)(EF_U16(0x00211adeu) + 1u);
    return EF_U16(record + 0x16u);
}

/* 0x90124, 200 bytes. Apply a result to an object slot, optionally computing the response value. */
void apply_slot_result_90124(int16_t slot, int16_t kind, uint16_t extra)
{
    ef_addr32 record = guest_lookup_object_8ffe8((int32_t)slot);
    int16_t local;
    int16_t response;

    if (record == 0 || (EF_U8(record + 0x30u) & 2u) != 0)
        return;
    if (EF_U8(0x00125847u) != 0) {
        guest_fn_cd150((int32_t)EF_U32(record + 0x1eu) >> 16);
        if (guest_fn_cd2e8_ret((int32_t)kind) == 0) {
            local = (int16_t)guest_fn_cd258();
            response = (int16_t)classify_object_percentage((uint8_t)local);
        } else {
            local = 0x64;
            response = 1;
        }
    } else {
        local = 0x64;
        response = 1;
    }
    if ((EF_U8(record + 0x30u) & 1u) != 0)
        return;
    if (response == 1)
        clear_scene_node_children(slot);
    guest_fn_10a9c((int32_t)slot, (int32_t)response);
    if (EF_U8(0x00127da9u) != 0)
        guest_fn_55f0c((int32_t)slot, (int32_t)local, (int32_t)response, (int32_t)extra);
}

/* 0x90378, 189 bytes. Drop an object near the camera onto the terrain and register its move. */
void drop_object_near_camera_90378(ef_addr32 object)
{
    int32_t x = EF_S32(object) >> 8;
    int32_t z;
    int32_t metric;

    if (abs_a28f2(x - EF_S32(0x0021444cu)) >= 0x4000)
        return;
    if (abs_a28f2((EF_S32(object + 4u) >> 8) - EF_S32(0x00214448u)) >= 0x4000)
        return;
    z = EF_S32(object + 8u) >> 8;
    if (abs_a28f2(z - EF_S32(0x00214450u)) >= 0x4000)
        return;
    metric = evaluate_scene_metric(0, (ef_addr32)x, (ef_addr32)z) << 8;
    EF_S32(object + 4u) = metric;
    /* ORIGINAL BUG: the height is stored and then immediately overwritten with its negation. */
    EF_S32(object + 4u) = -metric;
    guest_fn_a0ea8((int32_t)EF_U32(object + 0x1eu) >> 16, (uint32_t)((int32_t)EF_U32(object + 0xau) >> 16),
                   (int32_t)EF_U32(object + 0xcu) >> 16, (uint32_t)((int32_t)EF_U32(object + 0xeu) >> 16));
    if ((scene_gate_allows(EF_S32(object + 4u)) & 0xffu) != 0)
        guest_fn_9e0c4((int32_t)EF_U32(object + 0x1eu) >> 16, EF_U32(object) << 8, EF_U32(object + 4u) << 8,
                       EF_U32(object + 8u) << 8);
}

/* 0x93f44, 185 bytes. Reset the angle record from the current limits and flag out-of-range limits. */
void reset_angle_limits_93f44(void)
{
    uint32_t limits[3];
    uint8_t flag;

    EF_U16(0x0012dba0u) = 0;
    EF_U16(0x0012dba6u) = 0;
    guest_fn_d9cb4((ef_addr32)(uintptr_t)&limits[2], (ef_addr32)(uintptr_t)&limits[1], (ef_addr32)(uintptr_t)&limits[0]);
    initialize_scene_angle_record(0x0012dbecu);
    update_scene_angle_first(0x0012dbecu, (uint16_t)limits[2]);
    update_scene_angle_second(0x0012dbecu, (uint16_t)limits[1]);
    update_scene_angle_third(0x0012dbecu, (uint16_t)limits[0]);
    if (((int32_t)limits[2] >= 0x258 && (int32_t)limits[2] <= 0xfde8)
        || ((int32_t)limits[1] >= 0x258 && (int32_t)limits[1] <= 0xfde8))
        flag = 1;
    else
        flag = 0;
    EF_U8(0x001a6d1fu) = flag;
    EF_U16(0x0012dc80u) = 0x46;
    guest_fn_93a8c();
    request_scene_refresh();
}

/* 0x94954, 198 bytes. Update the navigation target of a unit from its linked object. */
void update_navigation_target_94954(int16_t unit_id)
{
    ef_addr32 unit = guest_fn_7489c((int32_t)unit_id);
    ef_addr32 target;

    EF_U16(0x0021448au) = 0x14;
    if (unit == 0)
        return;
    if (EF_U8(unit + 0x16cu) == 6) {
        target = guest_lookup_object_8ffe8((int32_t)EF_U32(unit + 0x168u) >> 16);
        if (target == 0)
            return;
        if (guest_lookup_object_8ffe8((int32_t)EF_U32(target + 0x14u) >> 16) == target)
            prepare_navigation_component(unit, target);
    } else {
        target = guest_fn_7489c((int32_t)EF_U32(unit + 0x144u) >> 16);
        if (target == 0 || (EF_U8(target + 0x24u) & 8u) != 0)
            return;
        if (scene_lookup_named_word((int32_t)EF_U32(target + 0x1eu) >> 16) != 0)
            EF_U16(0x0021448au) = (uint16_t)scene_lookup_named_word((int32_t)EF_U32(target + 0x1eu) >> 16);
        if (guest_fn_7489c((int32_t)EF_U32(target + 0x14u) >> 16) == target)
            prepare_navigation_component(unit, target);
    }
    guest_record_field_30(unit, target);
}
