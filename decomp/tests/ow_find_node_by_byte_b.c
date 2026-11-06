/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#include "ef_abi.h"

ef_addr32 find_node_by_byte_b(uint8_t key, ef_addr32 head)
{
    ef_addr32 node = *(volatile uint32_t *)(uintptr_t)head;
    while (node != 0) {
        if (*(volatile uint8_t *)(uintptr_t)(node + 0xbu) == key)
            return node;
        node = *(volatile uint32_t *)(uintptr_t)(node + 4u);
    }
    return 0;
}
