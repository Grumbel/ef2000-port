/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */
#include "asm_x32_service.h"

unsigned ef_asm_x32_service(ef_lift_state *guest, uint32_t ip,
                           unsigned vector, void *context)
{
    struct x32_heap_result result;
    if (!context || vector != 0x21u || ip != 0x245u ||
        (guest->r[0] & 0xffffu) != 0x3508u)
        return 0x122u;
    if (!x32_heap_grow((struct x32_allocation *)context, guest->r[1],
                      guest->flags, &result))
        return 0x122u;
    guest->r[0] = result.stack_low;
    guest->r[1] = result.heap_end;
    guest->flags = result.flags;
    return 0;
}
