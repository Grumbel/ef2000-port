/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"

static void call_cdecl_stack1(uint32_t fn, uint32_t arg);
#pragma aux call_cdecl_stack1 parm [eax] [edx] modify [eax ecx edx] = \
    "push edx" \
    "call eax" \
    "add esp,4"

/* Preserve the original cdecl callback contract: one stack argument and
 * caller cleanup.  The callback's EAX/ECX/EDX clobbers pass through. */
#pragma aux guest_callptr1_x parm [eax] [edx] modify [eax ecx edx]
void guest_callptr1_x(ef_addr32 fn, ef_addr32 arg)
{
    call_cdecl_stack1(fn, arg);
}
