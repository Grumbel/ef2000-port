/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xfba6c, 105 bytes. EAX/EDX match values, EBX list-head address;
 * EAX count, EBX/EDX scratch. ECX/ESI/EDI/EBP preserved, plain RET.
 * Callers bfcb6 and fba63 consume the low AX result. */
uint32_t count_matching_entries_fba6c(uint32_t a, uint32_t b, ef_addr32 listp)
{
    ef_addr32 node, entry;
    uint32_t total = 0;
    uint16_t index;

    node = EF_U32(listp);
    while (node != 0u) {
        index = 0;
        while ((int16_t)index < (int16_t)EF_U8(node + 0x12u)) {
            entry = EF_U32(node + 0x13u)
                  + (uint32_t)(int32_t)(int16_t)index * 0x7cu;
            if (EF_U32(entry + 8u) == a && EF_U32(entry + 12u) == b
                && EF_U8(node + 9u) == 2u) {
                if ((uint16_t)is_kind_bcbc0(EF_U8(entry)) != 0u)
                    total++;
            }
            index++;
        }
        node = EF_U32(node + 4u);
    }
    return total;
}
