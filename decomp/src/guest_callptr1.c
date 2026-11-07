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

static void call_cdecl_stack2(uint32_t fn, uint32_t arg0, uint32_t arg1);
#pragma aux call_cdecl_stack2 parm [eax] [edx] [ebx] modify [eax ecx edx] = \
    "push ebx" \
    "push edx" \
    "call eax" \
    "add esp,8"

#pragma aux guest_callptr2_x parm [eax] [edx] [ebx] modify [eax ecx edx]
void guest_callptr2_x(ef_addr32 fn, uint32_t arg0, uint32_t arg1)
{
    call_cdecl_stack2(fn, arg0, arg1);
}

static uint32_t call_register_eax(uint32_t fn, uint32_t arg);
#pragma aux call_register_eax parm [eax] [edx] modify [eax ecx edx] = \
    "mov ecx,eax" \
    "mov eax,edx" \
    "call ecx"

#pragma aux guest_callptr_eax_ret_x parm [eax] [edx] modify [eax ecx edx]
uint32_t guest_callptr_eax_ret_x(ef_addr32 fn, uint32_t arg)
{
    return call_register_eax(fn, arg);
}

static uint32_t call_register_eaxedx(uint32_t fn, uint32_t arg0,
                                     uint32_t arg1);
#pragma aux call_register_eaxedx parm [eax] [edx] [ebx] \
    modify [eax ecx edx] = \
    "mov ecx,eax" \
    "mov eax,edx" \
    "mov edx,ebx" \
    "call ecx"

#pragma aux guest_callptr_eaxedx_ret_x parm [eax] [edx] [ebx] \
    modify [eax ecx edx]
uint32_t guest_callptr_eaxedx_ret_x(ef_addr32 fn, uint32_t arg0,
                                    uint32_t arg1)
{
    return call_register_eaxedx(fn, arg0, arg1);
}
