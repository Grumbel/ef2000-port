/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Mode toggles, list lookups and deferred hook installation. */

/* 0x1a358, 54 bytes. Enter the mode once: apply the argument to the driver and refresh its table. */
void enter_mode_1a358(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t stack_arg)
{
    (void)u2;
    if (EF_U16(0x00122c78u) != 0)
        return;
    /* The flag is stored before the call; EAX/EDX/ECX pass through and EBX is 1. */
    EF_U16(0x00122c78u) = 1;
    guest_fn_d6340_x(u0, u1, 1, u3, stack_arg, stack_arg, stack_arg);
    guest_fn_d663c(0x001f74e0u);
}

/* 0x1a390, 57 bytes. Leave the mode: restore the saved driver parameters and refresh its table. */
void leave_mode_1a390(void)
{
    if (EF_U16(0x00122c78u) == 0)
        return;
    guest_fn_d6340_x(0, 0, 0, 0, EF_U32(0x00122c6cu), EF_U32(0x00122c70u), EF_U32(0x00122c74u));
    EF_U16(0x00122c78u) = 0;
    guest_fn_d663c(0x001f74e0u);
}

/* 0x3dccc, 56 bytes. Reset the drivers and step the four channels, then refresh the matching table. */
void reset_channels_3dccc(void)
{
    uint32_t i;

    guest_fn_d65e8();
    for (i = 0; i < 4; i++) {
        guest_fn_dc2c0_x(0, i);
        guest_fn_dc280_x(0, i + 1);
    }
    guest_fn_d663c(EF_U8(0x0012854cu) == 0 ? 0x001f74e0u : 0x00425190u);
}

/* 0xbc421, 47 bytes. Find the list node with the given key and open it; -1 when absent. */
uint32_t open_list_node_bc421(uint32_t key)
{
    ef_addr32 node = EF_U32(0x00485f9cu);

    while (node != 0) {
        if (EF_U32(node + 4u) == key)
            return guest_fn_bc450_x(EF_U32(node + 4u), 1);
        node = EF_U32(node);
    }
    return 0xffffffffu;
}

/* 0xa61b0, 58 bytes. Run the pending handler for every listed object that has one set. */
void run_pending_handlers_a61b0(void)
{
    uint32_t i;

    for (i = 0;; i++) {
        uint16_t id = EF_U16(0x0041eda8u + i * 2u);
        ef_addr32 obj;

        if (id == 0)
            return;
        obj = EF_U32(0x0041edfcu + (uint32_t)id * 4u);
        if (EF_U32(obj + 0xb5u) != 0)
            guest_fn_a60a8_x(obj);
    }
}
