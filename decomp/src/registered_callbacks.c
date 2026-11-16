/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* EAX=0, ECX=record offset; retain both loop registers after the call.
 * Read the target again at the CALL, as the original does after its test. */
#ifdef __WATCOMC__
static uint64_t invoke_registered_clear(uint32_t offset, uint32_t index);
#pragma aux invoke_registered_clear = \
    "mov ecx,ebx" \
    "xor eax,eax" \
    "call dword ptr [ecx+41e971h]" \
    "mov eax,ebx" \
    parm [ebx] [edx] value [edx eax] modify [eax ebx ecx edx]
#else
extern uint64_t invoke_registered_clear(uint32_t offset, uint32_t index);
#endif

/* 0xabec4, 84 bytes. No inputs or consumed result; plain RET.
 * Callers ac429 and ac78c neither supply arguments nor use EAX. */
void clear_registered_callbacks(void)
{
    uint32_t index = 0u;
    uint32_t offset = 0u;

    if (EF_U16(0x0041eeccu) == 0u)
        return;

    while ((int32_t)index < (int32_t)EF_U16(0x0041eeccu)) {
        if (EF_U32(0x0041e971u + offset) != 0u) {
            union { uint64_t pair; uint32_t word[2]; } state;
            state.pair = invoke_registered_clear(offset, index);
            offset = state.word[0];
            index = state.word[1];
        }
        offset += 0x15u;
        index += 1u;
    }
    EF_U16(0x0041eeccu) = 0u;
    guest_fn_b3fb0(0x0041e960u, 0u, 0x150u);
}
