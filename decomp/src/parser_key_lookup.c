/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

static uint32_t signed_key_index(uint16_t index)
{
    return (uint32_t)(int32_t)(int16_t)index;
}

/* 0xa7ec4, 158 bytes. EAX mutable name -> EAX slot address;
 * preserves all six other GPRs. Callers 47ec/4c41 retain their value
 * in EDX across lookup. The copy at count is the search sentinel. */
ef_addr32 lookup_key_a7ec4(ef_addr32 name)
{
    uint16_t index = 0;
    int32_t count;

    guest_fn_eb478(name);
    count = EF_S32(0x004194c2u) >> 16;
    guest_fn_9d749(0x00419218u + (uint32_t)count * 21u, name);
    while (guest_compare_parser_token(name,
               0x00419218u + signed_key_index(index) * 21u) != 0)
        index++;
    if ((int16_t)index >= EF_S16(0x004194c4u)) {
        if (EF_U8(0x001371f9u) == 0u)
            return 0x001371e8u;
        guest_callptr2_x(EF_U32(0x00132a74u), 0x0018d584u, name);
    }
    if (EF_U8(0x0041948eu + signed_key_index(index)) != 0u)
        guest_callptr2_x(EF_U32(0x00132a74u), 0x0018d5acu, name);
    return 0x00418708u + signed_key_index(index) * 4u;
}

/* 0xa7f64, 173 bytes. EAX mutable name -> EAX stored pointer;
 * all six other GPRs preserved, plain RET. Sole caller 27ffe stores
 * EAX, retaining EDX/EBX/ECX inputs. Original scratch text is 80 bytes. */
ef_addr32 find_key_checked_a7f64(ef_addr32 name)
{
    uint8_t message[80];
    ef_addr32 text = (ef_addr32)(uintptr_t)message;
    uint16_t index = 0;

    guest_fn_eb478(name);
    for (;;) {
        if (guest_compare_parser_token(name,
                0x00419218u + signed_key_index(index) * 21u) == 0)
            break;
        /* The comparison happens before the bound check, even at count. */
        if ((int16_t)index >= EF_S16(0x004194c4u))
            break;
        index++;
    }
    if ((int16_t)index >= EF_S16(0x004194c4u)) {
        if (EF_U8(0x001371f9u) == 0u)
            return 0x001371d8u;
        guest_sprintf_a4342(text, 0x0018d5dcu, name);
        guest_callptr1_x(EF_U32(0x00132a74u), text);
    }
    if (EF_U8(0x0041948eu + signed_key_index(index)) != 1u) {
        guest_sprintf_a4342(text, 0x0018d604u, name);
        guest_callptr1_x(EF_U32(0x00132a74u), text);
    }
    /* ORIGINAL BUG: returning error callbacks still permit this access. */
    return EF_U32(0x00418708u + signed_key_index(index) * 4u);
}
