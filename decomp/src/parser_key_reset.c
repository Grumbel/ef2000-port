/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xa9bfc, 209 bytes. No inputs or consumed output, six GPRs
 * preserved, plain RET. Sole caller aafb4 retains EDI and ECX.
 * Install the original ten 15-byte option buffers in their original order. */
void reset_key_tables_a9bfc(void)
{
    EF_U8(0x00137205u) = 0;
    EF_U8(0x001371fbu) = 0;
    EF_U32(0x00419214u) = 0;
    EF_U16(0x004194acu) = 0;
    EF_U8(0x001371edu) = 0;
    EF_U32(0x00417254u) = 0x0041952fu;
    EF_U32(0x00417258u) = 0x00419502u;
    EF_U32(0x0041725cu) = 0x00419511u;
    EF_U32(0x00417260u) = 0x004194e4u;
    EF_U16(0x004194c4u) = 0;
    EF_U32(0x00417250u) = 0x00419520u;
    EF_U32(0x00417264u) = 0x004194f3u;
    EF_U32(0x00417270u) = 0x0041953eu;
    EF_U32(0x00417274u) = 0x0041954du;
    EF_U32(0x00417268u) = 0x004194c6u;
    EF_U32(0x0041726cu) = 0x004194d5u;
    latch_b3f48();
    EF_U8(0x001371f9u) = return_true_3ce2c();
    EF_U8(0x001371eeu) = 0;
    EF_U8(0x001371ffu) = 0;
    EF_U8(0x00137200u) = 0;
    EF_U8(0x00137201u) = 0;
}
