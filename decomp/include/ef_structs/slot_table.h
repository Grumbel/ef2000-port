/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#ifndef EF_STRUCTS_SLOT_TABLE_H
#define EF_STRUCTS_SLOT_TABLE_H
#include "ef_types.h"

/* Minimum shapes read by bd5f8. */
typedef struct EF_PACKED ef_slot_table_record {
    uint8_t unknown_00;
    uint8_t kind;
    uint8_t unknown_02[3];
    uint8_t count;
    ef_addr32 slots;
} ef_slot_table_record;
EF_STATIC_ASSERT(ef_slot_table_kind_offset, offsetof(ef_slot_table_record, kind) == 1);
EF_STATIC_ASSERT(ef_slot_table_count_offset, offsetof(ef_slot_table_record, count) == 5);
EF_STATIC_ASSERT(ef_slot_table_slots_offset, offsetof(ef_slot_table_record, slots) == 6);

typedef struct EF_PACKED ef_slot_table_entry {
    uint16_t occupied;
    uint8_t unknown_02[8];
} ef_slot_table_entry;
EF_STATIC_ASSERT(ef_slot_table_occupied_offset, offsetof(ef_slot_table_entry, occupied) == 0);
EF_STATIC_ASSERT(ef_slot_table_entry_size, sizeof(ef_slot_table_entry) == 10);
#endif
