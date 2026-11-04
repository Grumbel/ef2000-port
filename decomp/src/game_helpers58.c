/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Record initialisers. */

/* 0x112b1c, 158 bytes. Reset a table record: clear the slots, fill the index tables with -1 and set defaults. */
void reset_table_record_112b1c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 stack_rec)
{
    uint32_t i;

    (void)u0; (void)u1; (void)u2; (void)u3;
    for (i = 0; i < 16; i++)
        EF_U32(stack_rec + 0x94u + i * 4u) = 0;
    guest_fn_b3fb0(stack_rec + 0xd4u, 0xffffffffu, 0x480u);
    for (i = 1; i <= 4; i++)
        EF_U32(stack_rec + i * 4u + 0x80u) = 0xffffffffu;
    for (i = 1; i <= 32; i++)
        EF_U32(stack_rec + i * 4u + 0x554u) = 0xffffffffu;
    EF_U32(stack_rec + 0x554u) = 0;
    EF_U32(stack_rec + 0x30u) = 0;
    EF_U32(stack_rec + 0x5cu) = 0;
    EF_U32(stack_rec + 0x60u) = 0xffffffffu;
    EF_U32(stack_rec + 0x6cu) = 0;
    EF_U32(stack_rec + 0x68u) = 0;
    EF_U32(stack_rec + 0x64u) = 4;
    EF_U32(stack_rec + 0x70u) = 0x7a1200u;
    EF_U32(stack_rec + 0x34u) = 0;
}
