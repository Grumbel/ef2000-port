/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Ring tables, saved-state wrappers and record readers, 0x56208..0x97ca8. */

/* 0x56208, 94 bytes. Clear ten 55-byte record counters and the 80x8 ready-word grid. */
void clear_counters_56208(void)
{
    int16_t i;
    int16_t row;

    for (i = 0; i < 10; i++) {
        EF_U16((uint32_t)((int32_t)i * 0x37) + 0x001a63cau) = 0;
        EF_U16((uint32_t)((int32_t)i * 0x37) + 0x001a63c8u) = 0;
    }
    for (row = 0; row < 0x50; row++) {
        int16_t column;

        for (column = 0; column < 8; column++)
            EF_U16((uint32_t)((int32_t)row << 4) + (uint32_t)column * 2u + 0x001a5df0u) = 0;
    }
}

/* 0x5b288, 101 bytes. Run 0x5b2f0 with temporary values for four globals, then restore them. */
void run_with_temp_state_5b288(uint8_t a, uint16_t b, uint16_t c, uint8_t d)
{
    uint8_t saved_a = EF_U8(0x00128148u);
    uint16_t saved_b = EF_U16(0x001a6cfcu);
    uint8_t saved_d = EF_U8(0x0012b369u);
    uint16_t saved_c;

    EF_U8(0x00128148u) = a;
    EF_U16(0x001a6cfcu) = b;
    EF_U8(0x0012b369u) = d;
    saved_c = EF_U16(0x001a6cf0u);
    EF_U16(0x001a6cf0u) = c;
    guest_fn_5b2f0();
    EF_U8(0x0012b369u) = saved_d;
    EF_U16(0x001a6cf0u) = saved_c;
    EF_U16(0x001a6cfcu) = saved_b;
    EF_U8(0x00128148u) = saved_a;
}

/* 0x5b9a8, 103 bytes. Build a 10-entry table of scaled sine/cosine pairs (radius 12). */
void build_ring_table_5b9a8(void)
{
    int16_t i;

    for (i = 0; i < 10; i++) {
        uint32_t angle = (uint32_t)(((int32_t)i * 0xffff) / 10) & 0xffffu;
        uint32_t index = angle >> 3;
        int32_t sine = (int16_t)EF_U16(0x00141e08u + index * 2u);
        int32_t cosine = (int16_t)EF_U16(0x00142e08u + index * 2u);

        EF_U16(0x001a69b8u + (uint32_t)i * 4u) = (uint16_t)((sine * 0xc) >> 15);
        EF_U16(0x001a69bau + (uint32_t)i * 4u) = (uint16_t)((cosine * 0xc) >> 15);
    }
    EF_U8(0x001281f4u) = 1;
}

/* 0x64498, 140 bytes. Build a 36-entry table of scaled sine/cosine pairs (radius 50, truncating). */
void build_ring_table_64498(void)
{
    int16_t i;

    EF_U8(0x001282b0u) = 1;
    for (i = 0; i < 0x24; i++) {
        uint32_t angle = (uint32_t)(((int32_t)i * 0xffff) / 0x24) & 0xffffu;
        uint32_t index = angle >> 3;
        int32_t sine = (int16_t)EF_U16(0x00141e08u + index * 2u);
        int32_t cosine = (int16_t)EF_U16(0x00142e08u + index * 2u);

        EF_U16(0x001a7850u + (uint32_t)i * 4u) = (uint16_t)((sine * 0x32) / 0x8000);
        EF_U16(0x001a7852u + (uint32_t)i * 4u) = (uint16_t)((-(cosine * 0x32)) / 0x8000);
    }
    EF_U16(0x001a81beu) = 0;
}

/* 0x748f4, 107 bytes. Send unit parameters when a remote unit is selected, otherwise defer to 0x74970. */
void send_or_defer_748f4(int16_t unit_id, uint32_t b, uint32_t c, uint8_t kind)
{
    ef_addr32 unit = guest_fn_7489c((int32_t)unit_id);

    if (unit == 0)
        return;
    if (EF_U8(0x00127da9u) != 0 &&
        EF_U16(unit + 0x18u) != (uint16_t)EF_U8(0x001a666bu) &&
        ((int32_t)EF_U32(unit + 0x16u) >> 16) != 0xff) {
        send_unit_params_55e94(unit_id, kind);
        return;
    }
    guest_fn_74970((int32_t)unit_id, b, c, kind);
}

/* 0x8cae8, 120 bytes. Fill an object's 20 slot entries from the defaults table and reset its cursor. */
void reset_slot_entries_8cae8(ef_addr32 object)
{
    int16_t i;

    for (i = 0; i < 0x14; i++) {
        uint32_t offset = (uint32_t)((int32_t)i * 3);
        ef_addr32 slot = object + offset;

        EF_U8(slot + 0x19bu) = EF_U8(offset + 0x001f6ef4u);
        EF_U16(slot + 0x19cu) = EF_U16(offset + 0x001f6ef5u);
        if (((int32_t)EF_U32(slot + 0x198u) >> 24) == 9)
            EF_U16(slot + 0x19cu) = 0x13;
    }
    EF_U16(object + 0x1d5u) = 0x6e0;
    EF_U8(object + 0x1d4u) = 1;
    EF_U16(object + 0x1d8u) = 0x96;
    EF_U16(object + 0x1dau) = 0x96;
    guest_fn_8f07c(object);
}

/* 0x9075c, 148 bytes. Find the next free object id after a start id, wrapping at the table end. */
uint32_t next_free_object_9075c(uint32_t start)
{
    uint32_t cursor = start;
    uint32_t id;
    int32_t base;

    if ((int16_t)start < 0x2000)
        cursor = 0x2000;
    base = (int32_t)EF_U32(0x00211adeu) >> 16;
    if ((int16_t)cursor >= base + 0x2000)
        cursor = (cursor & 0xffff0000u) | (uint16_t)(EF_U16(0x00211ae0u) + 0x1fffu);
    id = cursor;
    for (;;) {
        ef_addr32 object;

        base = (int32_t)EF_U32(0x00211adeu) >> 16;
        id++;
        if ((int16_t)id >= base + 0x2000)
            id = 0x2000;
        object = guest_lookup_object_8ffe8((int32_t)(int16_t)id);
        if (object != 0 && (EF_U8(object + 0x30u) & 2u) == 0 && EF_U32(object + 0x1cu) == 0 &&
            (guest_fn_906b4((int32_t)(int16_t)id) & 0xffu) != 0)
            return id;
        if ((uint16_t)id == (uint16_t)cursor)
            break;
        if (EF_U16(0x00211ae0u) == 0)
            break;
    }
    return 0xffu;
}

/* 0x948c4, 132 bytes. Pair the current unit with a target unit, or reset the target state. */
void pair_with_target_948c4(int16_t target_id)
{
    ef_addr32 self = guest_fn_7489c((int32_t)EF_S32(0x001f71c8u) >> 16);
    ef_addr32 target = guest_fn_7489c((int32_t)target_id);
    uint16_t word;

    if (target == 0 || target_id == 0xff || self == 0 || EF_U16(self + 0x20u) == 0) {
        EF_U16(0x0021449cu) = 1;
        EF_U8(0x0012db9eu) = EF_U8(0x00214486u);
        return;
    }
    word = (uint16_t)guest_fn_9125c((int32_t)EF_U32(self + 0x1eu) >> 16);
    if (word != 0)
        EF_U16(0x0021448au) = (uint16_t)guest_fn_9125c((int32_t)EF_U32(self + 0x1eu) >> 16);
    guest_fn_9457c(self, target);
}

/* 0x97a94, 131 bytes. Apply a 78-byte camera record through 0xa0ea8 and 0x9e0c4. */
void apply_camera_record_97a94(int16_t index)
{
    int16_t view = (int16_t)(EF_U16(0x0012dc76u) == 0 ? EF_U16(0x0021448eu) : EF_U16(0x0021448cu));
    ef_addr32 record = (ef_addr32)((int32_t)index * 78);

    guest_fn_a0ea8((int32_t)view,
                   (int32_t)(int16_t)EF_U16(record + 0x00212294u),
                   (int32_t)(int16_t)EF_U16(record + 0x00212298u),
                   (int32_t)(int16_t)EF_U16(record + 0x0021229cu));
    guest_fn_9e0c4((int32_t)view,
                   EF_U32(record + 0x0021227cu) << 8,
                   EF_U32(record + 0x00212280u) << 8,
                   EF_U32(record + 0x00212284u) << 8);
}

/* 0x97ca8, 102 bytes. Copy the current 78-byte camera record to six output pointers. */
void read_camera_record_97ca8(ef_addr32 out_x, ef_addr32 out_y, ef_addr32 out_z, ef_addr32 out_a,
                              ef_addr32 stack_b, ef_addr32 stack_c)
{
    ef_addr32 record = (ef_addr32)(((int32_t)EF_U32(0x0012dc72u) >> 16) * 78);

    EF_U16(out_a) = EF_U16(record + 0x00212294u);
    EF_U16(stack_b) = EF_U16(record + 0x00212298u);
    EF_U16(stack_c) = EF_U16(record + 0x0021229cu);
    EF_U32(out_x) = EF_U32(record + 0x0021227cu);
    EF_U32(out_y) = EF_U32(record + 0x00212280u);
    EF_U32(out_z) = EF_U32(record + 0x00212284u);
}
