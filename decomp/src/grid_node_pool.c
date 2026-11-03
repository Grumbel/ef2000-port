/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

/* 0x0002a934  size 0x39  drafted. Callers 0x2a7f8 and 0x2a8ac.
 * The original requires a nonempty queue: there is deliberately no
 * null-head check. The oldest node moves to the free list, leaving the
 * original next pointer intact until the free-list head is installed. */
void recycle_oldest_grid_node(void)
{
    ef_addr32 old = g_grid_node_head;
    ef_grid_pool_node *node = (ef_grid_pool_node *)ef_guest_memory_ptr(old);
    if (old == g_grid_node_tail) {
        g_grid_node_head = 0;
        g_grid_node_tail = 0;
    } else {
        g_grid_node_head = node->next;
    }
    {
        ef_addr32 free_head = g_grid_node_free;
        g_grid_node_free = old;
        node->next = free_head;
    }
}
