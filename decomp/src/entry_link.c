/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xbe320,210 bytes. EAX first-list pointer, EDX second-list pointer;
 * EAX/EDX scratch, five preserved registers, RET. Sole caller b15e4. */
void link_matching_entries_be320(ef_addr32 listp, ef_addr32 otherp)
{
    ef_addr32 node = EF_U32(listp);
    ef_addr32 other;
    ef_addr32 entry;
    uint32_t index;
    uint32_t kind;
    uint16_t value;

    while (node != 0u) {
        if (EF_U8(node + 9u) == 0u) {
            kind = EF_U8(node + 10u);
            if (kind != 15u && kind != 17u && kind != 16u) {
                other = EF_U32(otherp);
                while (other != 0u) {
                    if (EF_U8(other + 11u) == 0u && EF_U16(other + 9u) != 0u) {
                        index = 0u;
                        while ((int32_t)(int16_t)index < (int32_t)EF_U8(node + 18u)) {
                            entry = EF_U32(node + 19u)
                                + (uint32_t)(int32_t)(int16_t)index * 124u;
                            kind = EF_U8(entry + 2u);
                            if (kind != 6u && kind != 8u && kind != 7u && kind != 9u
                                && EF_U16(other + 0x19u) == EF_U16(node + 0x10u)
                                && (uint16_t)index == EF_U16(other + 0x21u)) {
                                value = (uint16_t)guest_fn_bb7c0(entry);
                                EF_U16(other + 0x53u) = 0u;
                                EF_U16(other + 0x51u) = value;
                            }
                            index++;
                        }
                    }
                    other = EF_U32(other + 4u);
                }
            }
        }
        node = EF_U32(node + 4u);
    }
}
