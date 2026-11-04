/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Page-row, id-list and statistics helpers, 0x9fe1c..0xa4820. */

/* 0x9fe1c, 139 bytes. Unlink an id from the singly linked list of 78-byte records. */
void unlink_record_9fe1c(int16_t id)
{
    int16_t previous = -1;
    int16_t current = (int16_t)EF_U16(0x00132b26u);

    while (current != -1 && current != id) {
        previous = current;
        current = (int16_t)EF_U16(0x00416e44u + (uint32_t)((int32_t)current * 0x4e));
    }
    if (current != id)
        return;
    if (previous == -1)
        EF_U16(0x00132b26u) = EF_U16(0x00416e44u + (uint32_t)((int32_t)current * 0x4e));
    else
        EF_U16(0x00416e44u + (uint32_t)((int32_t)previous * 0x4e)) =
            EF_U16(0x00416e44u + (uint32_t)((int32_t)current * 0x4e));
}

/* 0xa0ebc, 142 bytes. Fill a page row cell from a 9-word descriptor and mark it present. */
void fill_page_cell_a0ebc(ef_addr32 source)
{
    ef_addr32 row = EF_U32(0x00417174u) + 0xbu + (uint32_t)EF_U8(EF_U32(0x0041717cu) + 1u) * 29u;
    ef_addr32 cell = row + (uint32_t)((int32_t)(int16_t)EF_U16(source) * 2);

    EF_U16(cell) = 1;
    EF_U16(cell + 2u) = EF_U16(source + 2u);
    EF_U16(cell + 4u) = EF_U16(source + 4u);
    EF_U16(cell + 6u) = EF_U16(source + 6u);
    EF_U16(cell + 8u) = EF_U16(source + 2u);
    EF_U16(cell + 0xau) = EF_U16(source + 8u);
    store_table_entry_9dde0(cell + 0xcu, EF_U16(source + 0xau));
    EF_U16(cell + 0xeu) = EF_U16(source + 0xcu);
    EF_U16(cell + 0x10u) = EF_U16(source + 0xeu);
    EF_U16(cell + 0x14u) = 0;
    EF_U16(cell + 0x12u) = EF_U16(source + 0x10u);
}

/* 0xa2698, 141 bytes. Apply a page cell to a slot unless the slot is locked, and refresh it. */
void apply_page_cell_a2698(int16_t slot, int16_t column)
{
    ef_addr32 base = EF_U32(0x00417174u) + 0xbu;
    ef_addr32 record = base + (uint32_t)((int32_t)slot * 29);
    ef_addr32 cell;

    if ((EF_U8(record + 0x1cu) & 0x80u) != 0)
        return;
    cell = base + (uint32_t)EF_U8(EF_U32(0x0041717cu) + 1u) * 29u + (uint32_t)((int32_t)column * 2);
    guest_fn_a2728((int32_t)slot,
                   (int32_t)(int16_t)EF_U16(cell + 8u),
                   (int32_t)(int16_t)EF_U16(cell + 0xau),
                   (int32_t)(int16_t)EF_U16(cell + 0xcu));
    if ((EF_U8(record + 0x1cu) & 0x40u) != 0)
        return;
    if (EF_U8(0x00137a00u) == 0)
        return;
    guest_fn_a207c(cell, record);
}

/* 0xa36b0, 156 bytes. Accumulate clamped timing offsets of the listed entries. */
void accumulate_offsets_a36b0(void)
{
    uint16_t now = EF_U16(0x001357d6u);
    ef_addr32 ids = EF_U32(0x00136b74u);
    ef_addr32 header = EF_U32(0x00136b78u);
    uint32_t i;

    for (i = 0; i < EF_U8(header); i++) {
        uint32_t index = EF_U8(ids + i);
        ef_addr32 entry = EF_U32(0x00135878u + (index << 5));
        int16_t delta;

        if (EF_U8(entry + 6u) != 0)
            continue;
        delta = (int16_t)(now - EF_U16(entry + 0xbu));
        if (delta > 100)
            delta = 100;
        if (delta < -100)
            delta = -100;
        EF_U32(0x001357ecu) = EF_U32(0x001357ecu) + (uint32_t)(int32_t)delta;
        EF_U16(0x001357dau) = (uint16_t)(EF_U16(0x001357dau) + 1u);
    }
    EF_U32(0x00136b78u) = header;
    EF_U32(0x00136b74u) = ids;
    EF_U16(0x001357d6u) = now;
}

/* 0xa4718, 129 bytes. Remove an object id from the id table and clear its flag 4. */
void remove_id_a4718(uint16_t id)
{
    uint16_t count = EF_U16(0x0041eed8u);
    uint16_t i;

    if (id == 0 || id > count)
        return;
    {
        ef_addr32 object = EF_U32(0x0041edfcu + (uint32_t)id * 4u);

        EF_U8(object + 0x3bu) = (uint8_t)(EF_U8(object + 0x3bu) & 0xfbu);
    }
    for (i = 0; i < count; i++) {
        if (id != EF_U16(0x0041eda8u + (uint32_t)i * 2u))
            continue;
        while ((int32_t)i < (int32_t)count - 1) {
            EF_U16(0x0041eda8u + (uint32_t)i * 2u) = EF_U16(0x0041edaau + (uint32_t)i * 2u);
            i++;
        }
        EF_U16(0x0041eda6u + (uint32_t)count * 2u) = 0;
        break;
    }
}

/* 0xa479c, 131 bytes. Add a flagged object id to the first free slot of the id table. */
void append_id_a479c(uint16_t id)
{
    uint16_t count = EF_U16(0x0041eed8u);
    ef_addr32 object;
    uint16_t i;

    if (id == 0 || id > count)
        return;
    object = EF_U32(0x0041edfcu + (uint32_t)id * 4u);
    if ((EF_U8(object + 0x3bu) & 8u) == 0)
        return;
    EF_U8(object + 0x3bu) = (uint8_t)(EF_U8(object + 0x3bu) | 4u);
    for (i = 0; i < count; i++) {
        if (id == EF_U16(0x0041eda8u + (uint32_t)i * 2u))
            return;
    }
    for (i = 0; i < count; i++) {
        if (EF_U16(0x0041eda8u + (uint32_t)i * 2u) == 0) {
            EF_U16(0x0041eda8u + (uint32_t)i * 2u) = id;
            return;
        }
    }
}

/* 0xa4820, 137 bytes. Push a flagged object id to the front of the id table. */
void push_front_id_a4820(uint16_t id)
{
    uint16_t count = EF_U16(0x0041eed8u);
    ef_addr32 object;
    uint16_t i;

    if (id == 0 || id > count)
        return;
    object = EF_U32(0x0041edfcu + (uint32_t)id * 4u);
    if ((EF_U8(object + 0x3bu) & 8u) == 0)
        return;
    EF_U8(object + 0x3bu) = (uint8_t)(EF_U8(object + 0x3bu) | 4u);
    for (i = 0; i < count; i++) {
        if (id == EF_U16(0x0041eda8u + (uint32_t)i * 2u))
            return;
    }
    for (i = (uint16_t)(count - 1u); i > 0; i--)
        EF_U16(0x0041eda8u + (uint32_t)i * 2u) = EF_U16(0x0041eda6u + (uint32_t)i * 2u);
    EF_U16(0x0041eda8u) = id;
}
