/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_globals.h"
#include "ef_abi.h"
#include "ef_guest_memory.h"

/* Byte offsets are observed original record accesses, not a recovered
 * complete object structure. Preserve all untouched bytes and lookup
 * counter side effects. None of these drafts are guest-verified. */

/* 0x0006a354  size 0x22  drafted. */
void set_indexed_object_activation(int16_t index)
{
    ef_addr32 address = lookup_filtered_indexed_object(index);
    if (address != 0) {
        uint8_t *object = (uint8_t *)ef_guest_memory_ptr(address);
        if (object[0x140] != 0) {
            object[0x141] = 1;
            object[0x142] = 0;
        }
    }
}

/* 0x0006a378  size 0x1b  drafted. */
void clear_indexed_object_activation(int16_t index)
{
    ef_addr32 address = lookup_filtered_indexed_object(index);
    if (address != 0) {
        uint8_t *object = (uint8_t *)ef_guest_memory_ptr(address);
        if (object[0x140] != 0) {
            object[0x141] = 0;
        }
    }
}

/* 0x0006a394  size 0x39  drafted. The byte at +0xe9 selects a table
 * row of stride 0xe5. Only a nonzero flag at row offset zero enables the
 * three state writes. */
void set_indexed_object_state(int16_t index)
{
    ef_addr32 address = lookup_filtered_indexed_object(index);
    if (address != 0) {
        uint8_t *object = (uint8_t *)ef_guest_memory_ptr(address);
        uint8_t kind = object[0xe9];
        if (g_object_type_flags[(uint32_t)kind * 0xe5] != 0) {
            object[0x12a] = 1;
            object[0x13d] = 3;
            object[0x13e] = 8;
        }
    }
}

/* 0x0006a3d0  size 0x12  drafted. This wrapper does not check the
 * +0x140 enable byte, unlike the activation pair. */
void clear_indexed_object_state(int16_t index)
{
    ef_addr32 address = lookup_filtered_indexed_object(index);
    if (address != 0) {
        uint8_t *object = (uint8_t *)ef_guest_memory_ptr(address);
        object[0x12a] = 0;
    }
}
