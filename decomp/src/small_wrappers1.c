/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Small forwarding wrappers. */

/* 0xd605c, 23 bytes. Forward three register values to the AIL trace call 0x103c28 with the global handle at 0x420a6c. */
void forward_trace_call_d605c(uint32_t a, uint32_t b, uint32_t c)
{
    trace_call_103c28(0u, 0u, 0u, 0u, EF_U32(0x00420a6cu), 0u, a, b, c);
}
