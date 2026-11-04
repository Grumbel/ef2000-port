/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Resource slot allocators and small deferred actions. */

/* 0xa7900, 72 bytes. Open a resource through the first free one of five slots (name A). */
int32_t open_slot_a7900(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t stack_arg)
{
    uint32_t i;

    (void)u0; (void)u1; (void)u2; (void)u3;
    for (i = 0; i < 5; i++) {
        if (EF_U32(0x001371a4u + i * 4u) == 0) {
            uint32_t h = guest_fn_bc36a_w(stack_arg, 0x0018d52cu);
            EF_U32(0x001371a4u + i * 4u) = h;
            if (h == 0)
                return -1;
            return (int32_t)(i + 0x64u);
        }
    }
    return -1;
}

/* 0xa7948, 72 bytes. Open a resource through the first free one of five slots (name B). */
int32_t open_slot_a7948(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t stack_arg)
{
    uint32_t i;

    (void)u0; (void)u1; (void)u2; (void)u3;
    for (i = 0; i < 5; i++) {
        if (EF_U32(0x001371a4u + i * 4u) == 0) {
            uint32_t h = guest_fn_bc36a_w(stack_arg, 0x0018d530u);
            EF_U32(0x001371a4u + i * 4u) = h;
            if (h == 0)
                return -1;
            return (int32_t)(i + 0x64u);
        }
    }
    return -1;
}

/* 0xeae10, 54 bytes. Release the pending handle and the status message. */
void release_pending_handle_eae10(void)
{
    if (EF_U16(0x0048446au) != 0)
        guest_fn_de648_w(0x00425c8cu);
    if (EF_U32(0x00160644u) != 0xffffffffu) {
        guest_fn_9ce84(EF_U32(0x00160644u));
        EF_U32(0x00160644u) = 0xffffffffu;
    }
}

/* 0xf1d54, 60 bytes. Apply the stored selection when the pending-input bit is set. */
void apply_selection_f1d54(void)
{
    if (EF_U16(0x0041f284u) != 0)
        return;
    if (EF_U16(0x0041f290u) != 0)
        return;
    if (EF_U16(0x0041eed6u) == 0)
        return;
    if ((EF_U8(0x0041f2abu) & 0x10) == 0)
        return;
    guest_fn_a4b80(EF_U16(0x0041ed84u));
}
