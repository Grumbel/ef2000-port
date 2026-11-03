/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"
#include "ef_structs/object_relation.h"

/* 0x0002541c  size 0x4d  drafted. AX is a signed slot, DL the mode.
 * Only objects of kind 2 (+0x16c) accept mode writes. Modes 1 and 2
 * dispatch both original subroutines; other modes set the +0x16e flag. */
void set_indexed_object_mode(int16_t index, uint8_t mode)
{
    ef_addr32 address = lookup_filtered_indexed_object(index);
    if (address != 0) {
        uint8_t *object = (uint8_t *)ef_guest_memory_ptr(address);
        if (object[0x16c] == 2) {
            object[0x16d] = mode;
            if (mode == 1 || mode == 2) {
                process_object_state_29(address);
                process_object_state_30(address);
            } else {
                object[0x16e] = 1;
            }
        }
    }
}

/* 0x000238a0  size 0x1f  drafted. The predicate includes its own filtered
 * lookup and global effects; this wrapper does not call it on rejection. */
void sync_current_object_slot(void)
{
    if (check_current_object_relation() != 0) {
        ef_object_relation_fields *object =
            (ef_object_relation_fields *)ef_guest_memory_ptr(g_current_object);
        object->active_slot = object->pending_slot;
    }
}
