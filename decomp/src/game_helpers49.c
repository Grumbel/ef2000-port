/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Target tracking, effect fades and resource table loading, 0x8f3f4..0x8fe58.
 * In the flat 32-bit guest, host stack buffers are valid guest pointers. */

/* 0x8f3f4, 218 bytes. Make the other object the tracked one if it is at least as close as the current one. */
void track_closer_object_8f3f4(ef_addr32 object, ef_addr32 other)
{
    uint32_t dx;
    uint32_t dz;
    uint32_t new_distance;
    uint32_t old_distance;
    ef_addr32 current;

    if ((indexed_object_is_canonical(object) & 0xffu) == 0)
        return;
    if (other == EF_U32(object + 0x1cu))
        return;
    dx = abs_cf94a((EF_S32(other) >> 16) - (EF_S32(object) >> 16));
    dz = abs_cf94a((EF_S32(other + 8u) >> 16) - (EF_S32(object + 8u) >> 16));
    if ((int32_t)dx > (int32_t)dz)
        new_distance = abs_cf94a((EF_S32(other) >> 16) - (EF_S32(object) >> 16));
    else
        new_distance = abs_cf94a((EF_S32(other + 8u) >> 16) - (EF_S32(object + 8u) >> 16));
    current = EF_U32(object + 0x1cu);
    if (current != 0) {
        dx = abs_cf94a((EF_S32(current) >> 16) - (EF_S32(object) >> 16));
        dz = abs_cf94a((EF_S32(EF_U32(object + 0x1cu) + 8u) >> 16) - (EF_S32(object + 8u) >> 16));
        if ((int32_t)dx > (int32_t)dz)
            old_distance = abs_cf94a((EF_S32(EF_U32(object + 0x1cu)) >> 16) - (EF_S32(object) >> 16));
        else
            old_distance = abs_cf94a((EF_S32(EF_U32(object + 0x1cu) + 8u) >> 16) - (EF_S32(object + 8u) >> 16));
        if ((int32_t)old_distance < (int32_t)new_distance)
            return;
    }
    EF_U32(object + 0x1cu) = other;
}

/* 0x8f930, 181 bytes. Publish the current entry position of the player's unit through the 0x831a0 block. */
void publish_entry_position_8f930(void)
{
    ef_addr32 unit = guest_fn_7489c((int32_t)EF_U32(0x001f71c8u) >> 16);
    ef_addr32 block;
    ef_addr32 owner;
    uint32_t offset;

    if (unit == 0) {
        ui_report_missing_record(0x0018a9e0u);
        return;
    }
    block = EF_U32(0x000831a0u);
    EF_U8(block + 0x23u) = (uint8_t)(EF_U8(block + 0x23u) | 0x10u);
    owner = EF_U32(0x0019bb58u) + (uint32_t)(((int32_t)EF_U32(unit + 0x26u) >> 16) * 0xca);
    offset = (uint32_t)EF_U8(owner + 0x8eu) * 25u;
    EF_U32(block + 4u) = 0;
    EF_U32(block) = EF_U32(EF_U32(owner + 0x8au) + offset + 4u) << 8;
    owner = EF_U32(0x0019bb58u) + (uint32_t)(((int32_t)EF_U32(unit + 0x26u) >> 16) * 0xca);
    offset = (uint32_t)EF_U8(owner + 0x8eu) * 25u;
    EF_U32(block + 8u) = EF_U32(EF_U32(owner + 0x8au) + offset) << 8;
}

/* 0x8fc24, 180 bytes. Fade the ten 24-byte effect slots and relink their records. */
void fade_effect_slots_8fc24(void)
{
    int16_t i;

    for (i = 0; i < 10; i++) {
        ef_addr32 slot = 0x0020f460u + (uint32_t)((int32_t)i * 24);
        int16_t life = (int16_t)EF_U16(slot + 0x14u);

        if (life > 0) {
            EF_U16(slot + 0x14u) = (uint16_t)(life - (int16_t)EF_U16(0x00127d92u));
            guest_fn_a0ea8((int32_t)EF_U32(slot + 0x14u) >> 16, (uint32_t)(int32_t)(int16_t)(-(int16_t)EF_U16(slot + 0xcu)),
                           (int32_t)EF_U32(slot + 0xcu) >> 16, 0);
            guest_fn_9e0c4((int32_t)EF_U32(slot + 0x14u) >> 16, EF_U32(slot) << 8, EF_U32(slot + 4u) << 8,
                           EF_U32(slot + 8u) << 8);
            EF_S32(slot + 0x10u) = (EF_S32(slot + 0x10u) * 0x1f3) / 0x1f4;
        }
    }
}

/* 0x8fcec, 130 bytes. Name the units of a player's formation from the name table. */
void name_formation_units_8fcec(int16_t player)
{
    ef_addr32 owner;
    uint32_t i;
    ef_addr32 slot;
    uint32_t table_offset = 0;

    if (player == 0xff)
        return;
    owner = EF_U32(0x0019bb58u) + (uint32_t)((int32_t)player * 0xca);
    slot = owner;
    for (i = 0; (int32_t)i < (int32_t)EF_U8(owner + 0x4fu); i++) {
        ef_addr32 unit = guest_fn_7489c((int32_t)EF_U32(slot + 0x4eu) >> 16);

        if (unit == 0)
            ui_report_missing_record(0x0018aa48u);
        slot += 2u;
        copy_string_9d749(unit + 0x2bu, EF_U32(0x0012b1a8u + table_offset));
        table_offset += 8u;
        EF_U32(unit + 0x3fu) = guest_fn_81354(unit + 0x2bu);
    }
}

/* 0x8fe58, 135 bytes. Load the table resource and index its 50 records. */
void load_record_table_8fe58(void)
{
    uint8_t path[0x78];
    ef_addr32 buf = (ef_addr32)(uintptr_t)path;
    ef_addr32 block;
    ef_addr32 cursor;
    int16_t i = 0;

    clear_blocks_8fe30();
    guest_fn_8fd70(buf, 0x0012db7cu, 0x0018aa98u);
    {
        int16_t size = (int16_t)guest_callptr_eax_ret_x(
            EF_U32(0x00132ab4u), buf);
        if (size == 0)
            return;
        block = arena_take_e29e8((uint32_t)(int32_t)size);
    }
    if (block == 0)
        return;
    if ((uint8_t)guest_callptr_eaxedx_ret_x(
            EF_U32(0x00132ab8u), buf, block) == 0)
        return;
    EF_U16(0x00211ac8u) = EF_U16(block);
    EF_U16(0x00211acau) = EF_U16(block + 2u);
    cursor = block + 4u;
    while (i < 0x32) {
        uint16_t size = (uint16_t)guest_fn_8fdb8(cursor);

        if (size == 0)
            return;
        EF_U32(0x00211a00u + (uint32_t)((int32_t)i * 4)) = cursor;
        i++;
        cursor += (uint32_t)(int32_t)(int16_t)size;
    }
}
