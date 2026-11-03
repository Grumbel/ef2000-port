/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_TACTICAL_RECORDS_H
#define EF_TACTICAL_RECORDS_H

#include "ef_types.h"

/* 0x1a4a0c: a 20-entry, seven-byte event ring. The final byte is cleared
 * on every append, not retained from the previous entry. */
typedef struct EF_PACKED ef_tactical_ring_entry {
    uint8_t enabled;
    int16_t x;
    int16_t y;
    uint8_t kind;
    uint8_t reserved;
} ef_tactical_ring_entry;
EF_STATIC_ASSERT(ef_ring_size, sizeof(ef_tactical_ring_entry) == 7);
EF_STATIC_ASSERT(ef_ring_x_off, offsetof(ef_tactical_ring_entry, x) == 1);
EF_STATIC_ASSERT(ef_ring_y_off, offsetof(ef_tactical_ring_entry, y) == 3);
EF_STATIC_ASSERT(ef_ring_kind_off, offsetof(ef_tactical_ring_entry, kind) == 5);
EF_STATIC_ASSERT(ef_ring_reserved_off, offsetof(ef_tactical_ring_entry, reserved) == 6);

/* An object has 20 three-byte activity slots at +0x19b. The category
 * selector is the byte immediately after the twentieth slot (+0x1d7).
 * Other fields of the object are still unknown and left opaque. */
typedef struct EF_PACKED ef_activity_slot {
    uint8_t code;
    int16_t strength;
} ef_activity_slot;
EF_STATIC_ASSERT(ef_activity_slot_size, sizeof(ef_activity_slot) == 3);
EF_STATIC_ASSERT(ef_activity_slot_strength_off, offsetof(ef_activity_slot, strength) == 1);

typedef struct EF_PACKED ef_activity_object {
    uint8_t unknown_00[0x19b];
    ef_activity_slot slots[20];
    int8_t selected_category;
} ef_activity_object;
EF_STATIC_ASSERT(ef_activity_object_slots_off, offsetof(ef_activity_object, slots) == 0x19b);
EF_STATIC_ASSERT(ef_activity_object_category_off,
                 offsetof(ef_activity_object, selected_category) == 0x1d7);
EF_STATIC_ASSERT(ef_activity_object_min_size, sizeof(ef_activity_object) == 0x1d8);

/* 0x1a7d33: 23-byte records scanned backwards by 0x62548. */
typedef struct EF_PACKED ef_unit_scan_record {
    uint8_t active_code;
    int16_t index;
    uint8_t unknown_03[0x14];
} ef_unit_scan_record;
EF_STATIC_ASSERT(ef_unit_record_size, sizeof(ef_unit_scan_record) == 0x17);
EF_STATIC_ASSERT(ef_unit_record_index_off, offsetof(ef_unit_scan_record, index) == 1);

#endif
