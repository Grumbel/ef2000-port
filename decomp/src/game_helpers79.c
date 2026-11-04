/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Secondary flag merge. */

/* 0xf7518, 98 bytes. Merge the pending flag word into the second flag field, or clear it (mode 2 skips the refresh). */
void merge_pending_flags2_f7518(void)
{
    ef_addr32 rec;

    if (EF_U8(0x0041f2e2u) != 2)
        guest_fn_f6124_x();
    rec = EF_U32(0x0041ecccu);
    if (EF_U32(0x0046438cu) != 0xffffu) {
        uint16_t flags = EF_U16(0x0046438cu);

        EF_U16(0x0041ec38u) = (uint16_t)(EF_U16(0x0041ec38u) | flags);
        EF_U16(rec + 0x10u) = (uint16_t)(EF_U16(rec + 0x10u) | flags);
        return;
    }
    EF_U16(0x0041ec38u) = 0;
    EF_U16(rec + 0x10u) = 0;
}
