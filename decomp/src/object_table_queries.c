/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

/* 0x8c09c, size 0x20, five caller functions. Test whether the object
 * at EAX is the entry returned for its +0x14 signed high-word index. */
uint8_t indexed_object_is_canonical(ef_addr32 object)
{
    if (object == 0) { return 0; }
    const ef_scene_node *node = (const ef_scene_node *)ef_guest_memory_ptr(object);
    int16_t index = (int16_t)((uint32_t)node->index_packed >> 16);
    return (uint8_t)(lookup_indexed_object(index) == object);
}

/* 0x8c0bc, size 0x09, three caller functions. The original sign-extends
 * AX, then indexes the raw fixed-address table without a bounds check. */
ef_addr32 indexed_object_direct_slot(int16_t slot)
{
    return g_indexed_object_table[slot];
}
