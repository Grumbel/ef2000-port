/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"

static void call_cdecl_stack1(uint32_t fn, uint32_t arg);
#pragma aux call_cdecl_stack1 parm [eax] [edx] modify [eax ecx edx] = \
    "push edx" \
    "call eax" \
    "add esp,4"

static void call_register0(uint32_t fn);
#pragma aux call_register0 parm [eax] modify [eax ecx edx] = \
    "call eax"

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

static void call_cdecl_stack3(uint32_t fn, uint32_t arg0, uint32_t arg1,
                              uint32_t arg2);
#pragma aux call_cdecl_stack3 parm [eax] [edx] [ebx] [ecx] \
    modify [eax ecx edx] = \
    "push ecx" \
    "push ebx" \
    "push edx" \
    "call eax" \
    "add esp,12"

#pragma aux guest_callptr2_x parm [eax] [edx] [ebx] modify [eax ecx edx]
void guest_callptr2_x(ef_addr32 fn, uint32_t arg0, uint32_t arg1)
{
    call_cdecl_stack2(fn, arg0, arg1);
}

#pragma aux guest_callptr3_x parm [eax] [edx] [ebx] [ecx] \
    modify [eax ecx edx]
void guest_callptr3_x(ef_addr32 fn, uint32_t arg0, uint32_t arg1,
                      uint32_t arg2)
{
    call_cdecl_stack3(fn, arg0, arg1, arg2);
}

#pragma aux guest_callptr0_x parm [eax] modify [eax ecx edx]
void guest_callptr0_x(ef_addr32 fn)
{
    call_register0(fn);
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

#pragma aux guest_callptr_eax_x parm [eax] [edx] modify [eax ecx edx]
void guest_callptr_eax_x(ef_addr32 fn, uint32_t arg)
{
    (void)call_register_eax(fn, arg);
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

#pragma aux guest_callptr2r_x parm [eax] [edx] [ebx] \
    modify [eax ecx edx]
void guest_callptr2r_x(ef_addr32 fn, int32_t arg0, int32_t arg1)
{
    (void)call_register_eaxedx(fn, (uint32_t)arg0, (uint32_t)arg1);
}

static void call_register_eaxedxebx(uint32_t fn, uint32_t arg0,
                                    uint32_t arg1, uint32_t arg2);
#pragma aux call_register_eaxedxebx parm [eax] [edx] [ebx] [ecx] \
    modify [eax ecx edx] = \
    "xchg eax,ecx" \
    "xchg eax,edx" \
    "xchg edx,ebx" \
    "call ecx"

#pragma aux guest_callptr3r_x parm [eax] [edx] [ebx] [ecx] \
    modify [eax ecx edx]
void guest_callptr3r_x(ef_addr32 fn, uint32_t arg0, uint32_t arg1,
                       uint32_t arg2)
{
    call_register_eaxedxebx(fn, arg0, arg1, arg2);
}

/* Preserve the register callback's clobbers when its return value is unused. */
#pragma aux guest_callptr_eaxedx_x parm [eax] [edx] [ebx] \
    modify [eax ecx edx]
void guest_callptr_eaxedx_x(ef_addr32 fn, uint32_t arg0, uint32_t arg1)
{
    (void)call_register_eaxedx(fn, arg0, arg1);
}

static uint32_t call_register_array4(uint32_t fn, const uint32_t *args);
static uint32_t call_cdecl_array(uint32_t fn, const uint32_t *args,
                                 uint32_t count);
#pragma aux call_register_array4 parm [eax] [edx] value [eax] \
    modify [eax ebx ecx edx] = \
    "push eax" \
    "mov eax,[edx]" \
    "mov ecx,[edx+12]" \
    "mov ebx,[edx+8]" \
    "mov edx,[edx+4]" \
    "call dword ptr [esp]" \
    "add esp,4"

#pragma aux guest_callptr4_x parm [eax] [edx] [ebx] [ecx] \
    modify [eax ebx ecx edx]
void guest_callptr4_x(ef_addr32 fn, uint32_t arg0, uint32_t arg1,
                      uint32_t arg2, uint32_t arg3)
{
    uint32_t args[4];
    args[0] = arg0; args[1] = arg1; args[2] = arg2; args[3] = arg3;
    (void)call_register_array4(fn, args);
}

#pragma aux guest_callptr4_ret_x parm [eax] [edx] [ebx] [ecx] \
    value [eax] modify [eax ebx ecx edx]
uint32_t guest_callptr4_ret_x(ef_addr32 fn, uint32_t arg0, uint32_t arg1,
                              uint32_t arg2, uint32_t arg3)
{
    uint32_t args[4];
    args[0] = arg0; args[1] = arg1; args[2] = arg2; args[3] = arg3;
    return call_register_array4(fn, args);
}

#pragma aux guest_callptr4c_x parm [eax] [edx] [ebx] [ecx] \
    modify [eax ebx ecx edx]
void guest_callptr4c_x(ef_addr32 fn, uint32_t arg0, uint32_t arg1,
                       uint32_t arg2, uint32_t arg3)
{
    uint32_t args[4];
    args[0] = arg0; args[1] = arg1; args[2] = arg2; args[3] = arg3;
    (void)call_cdecl_array(fn, args, 4u);
}

#pragma aux guest_callptr5_x parm [eax] [edx] [ebx] [ecx] \
    value [eax] modify [eax ebx ecx edx]
uint32_t guest_callptr5_x(ef_addr32 fn, uint32_t arg0, uint32_t arg1,
                          uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    uint32_t args[5];
    args[0] = arg0; args[1] = arg1; args[2] = arg2; args[3] = arg3;
    args[4] = arg4;
    return call_cdecl_array(fn, args, 5u);
}

static uint32_t call_cdecl_array(uint32_t fn, const uint32_t *args,
                                 uint32_t count);
#pragma aux call_cdecl_array parm [eax] [edx] [ebx] value [eax] \
    modify [eax ecx edx] = \
    "push esi" \
    "push edi" \
    "mov esi,eax" \
    "mov edi,edx" \
    "mov ecx,ebx" \
    "test ecx,ecx" \
    "jz short call_target" \
    "push_args:" \
    "dec ecx" \
    "push dword ptr [edi+ecx*4]" \
    "test ecx,ecx" \
    "jnz short push_args" \
    "call_target:" \
    "call esi" \
    "lea esp,[esp+ebx*4]" \
    "pop edi" \
    "pop esi"

#pragma aux guest_callptr26_x parm [eax] [edx] [ebx] [ecx] \
    modify [eax ebx ecx edx]
void guest_callptr26_x(ef_addr32 fn,
                       uint32_t a0, uint32_t a1, uint32_t a2,
                       uint32_t a3, uint32_t a4, uint32_t a5,
                       uint32_t a6, uint32_t a7, uint32_t a8,
                       uint32_t a9, uint32_t a10, uint32_t a11,
                       uint32_t a12, uint32_t a13, uint32_t a14,
                       uint32_t a15, uint32_t a16, uint32_t a17,
                       uint32_t a18, uint32_t a19, uint32_t a20,
                       uint32_t a21, uint32_t a22, uint32_t a23,
                       uint32_t a24, uint32_t a25)
{
    uint32_t args[26];
    args[0] = a0; args[1] = a1; args[2] = a2; args[3] = a3;
    args[4] = a4; args[5] = a5; args[6] = a6; args[7] = a7;
    args[8] = a8; args[9] = a9; args[10] = a10; args[11] = a11;
    args[12] = a12; args[13] = a13; args[14] = a14; args[15] = a15;
    args[16] = a16; args[17] = a17; args[18] = a18; args[19] = a19;
    args[20] = a20; args[21] = a21; args[22] = a22; args[23] = a23;
    args[24] = a24; args[25] = a25;
    (void)call_cdecl_array(fn, args, 26u);
}

#pragma aux guest_callptr7_x parm [eax] [edx] [ebx] [ecx] \
    modify [eax ebx ecx edx]
void guest_callptr7_x(ef_addr32 fn, uint32_t a, uint32_t b, uint32_t c,
                      uint32_t d, uint32_t e, uint32_t f, uint32_t g)
{
    uint32_t args[7];
    args[0] = a; args[1] = b; args[2] = c; args[3] = d;
    args[4] = e; args[5] = f; args[6] = g;
    (void)call_cdecl_array(fn, args, 7u);
}
