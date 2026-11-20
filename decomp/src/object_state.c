/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_globals.h"
#include "ef_abi.h"
#include "ef_guest_memory.h"
#include "ef_mem.h"

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

#define TYPE_RECORD_SIZE   0xe5u
#define TYPE_FIELD_B2      0x00128643u /* dword per object type */

/* Object record fields. */
#define OBJECT_ROUTE       0x1cu  /* dword */
#define OBJECT_FLAGS_25    0x25u
#define OBJECT_FLAGS_26    0x26u
#define OBJECT_ANNOUNCE    0x2au
#define OBJECT_TYPE        0xe9u
#define OBJECT_STATE       0x16cu
#define OBJECT_STATE_FLAG  0x16eu

#define FLAGS25_HOLD       0x04u
#define FLAGS25_LINKED     0x10u
#define FLAGS25_LINK_MASK  0x30u
#define FLAGS26_ACTIVE     0x04u

enum {
    STATE_LAUNCH = 2,
    STATE_WAYPOINT = 5,
    STATE_GRID = 6,
    STATE_ATTACK = 9,
    STATE_IDLE = 0xc,
    STATE_HOLD = 0xe,
    STATE_HOLD_ALT = 0x10
};

/* 0x6a1d0, 388 bytes. Move object INDEX into STATE: clear its hold flag
   (and its link bits when linked), run the per-state entry actions and
   store the new state, then mark the object active unless it is idle. */
void transition_indexed_object_state(int16_t index, uint8_t state)
{
    ef_addr32 object = lookup_filtered_indexed_object(index);
    uint8_t flags;

    if (object == 0)
        return;
    flags = (uint8_t)(EF_U8(object + OBJECT_FLAGS_25) & ~FLAGS25_HOLD);
    EF_U8(object + OBJECT_FLAGS_25) = flags;
    if ((flags & FLAGS25_LINKED) != 0)
        EF_U8(object + OBJECT_FLAGS_25) = (uint8_t)(flags & ~FLAGS25_LINK_MASK);

    switch (state) {
    case STATE_LAUNCH:
        set_indexed_object_state(index);
        if (guest_fn_6aeb4(index) == 0 && EF_U32(object + OBJECT_ROUTE) == 0)
            break;
        if (EF_U8(object + OBJECT_ANNOUNCE) != 0)
            draw_object_entries_46168((uint32_t)(int32_t)index, 0x28, 0, 0, 0);
        EF_U8(object + OBJECT_STATE) = STATE_LAUNCH;
        if (EF_U32(TYPE_FIELD_B2 + TYPE_RECORD_SIZE * EF_U8(object + OBJECT_TYPE)) == 0)
            set_indexed_object_state(index);
        break;
    case STATE_GRID:
        if (indexed_object_has_grid_link(index) != 0) {
            EF_U8(object + OBJECT_STATE) = STATE_GRID;
            load_waypoint_752d0(index);
        }
        break;
    case STATE_WAYPOINT:
        load_waypoint_752d0(index);
        update_unit_vector_75344(index);
        EF_U8(object + OBJECT_STATE) = STATE_WAYPOINT;
        break;
    case STATE_HOLD:
    case STATE_HOLD_ALT:
        EF_U8(object + OBJECT_STATE) = state;
        EF_U8(object + OBJECT_FLAGS_25) |= FLAGS25_HOLD;
        break;
    case STATE_ATTACK:
        EF_U8(object + OBJECT_STATE) = STATE_ATTACK;
        EF_U8(object + OBJECT_STATE_FLAG) = 1;
        draw_object_entries_46168((uint32_t)(int32_t)index, 0x5c, 0, 0, 0);
        break;
    default:
        EF_U8(object + OBJECT_STATE) = state;
        break;
    }

    if (EF_U8(object + OBJECT_STATE) == STATE_IDLE)
        EF_U8(object + OBJECT_FLAGS_26) &= (uint8_t)~FLAGS26_ACTIVE;
    else
        EF_U8(object + OBJECT_FLAGS_26) |= FLAGS26_ACTIVE;
}
