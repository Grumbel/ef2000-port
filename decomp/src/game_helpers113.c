/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Random sub-entry recharge. */

/* 0xcac4c, 270 bytes. Add a random amount (mod 35 for kinds 0x35/0x37, mod 10 for elite records, else mod 5) to every sub-entry counter below 100, capped at 100. */
void recharge_sub_entries_cac4c(ef_addr32 list)
{
    ef_addr32 rec = EF_U32(list);

    while (rec != 0) {
        int16_t i = 0;

        while (i < EF_S16(rec + 0x2au)) {
            ef_addr32 sub = EF_U32(rec + 0x2cu) + (uint32_t)(int32_t)i * 0x15u;

            if (EF_U8(sub + 5u) < 100) {
                uint8_t code = EF_U8(sub);
                uint32_t div;
                uint32_t r;
                uint32_t v;

                if (code == 0x35 || code == 0x37)
                    div = 0x23;
                else if ((uint16_t)record_is_elite(sub) != 0)
                    div = 10;
                else
                    div = 5;
                r = (uint16_t)guest_fn_a45b3() % div;
                EF_U8(sub + 5u) = (uint8_t)(EF_U8(sub + 5u) + r);
                v = EF_U8(EF_U32(rec + 0x2cu) + (uint32_t)(int32_t)i * 0x15u + 5u);
                if (v > 100)
                    v = 100;
                EF_U8(EF_U32(rec + 0x2cu) + (uint32_t)(int32_t)i * 0x15u + 5u) = (uint8_t)v;
            }
            i++;
        }
        rec = EF_U32(rec + 4u);
    }
}
