/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#include "ef_abi.h"
#include "ef_mem.h"
#include <stddef.h>

typedef struct MenuPointerPayload {
    uint8_t prefix[30], first[4], second[4], third[4];
} MenuPointerPayload;
typedef char menu_pointer_payload_layout[
    sizeof(MenuPointerPayload) == 42 &&
    offsetof(MenuPointerPayload, first) == 30 &&
    offsetof(MenuPointerPayload, second) == 34 &&
    offsetof(MenuPointerPayload, third) == 38 ? 1 : -1];

/* 0xaa4b8, 26 bytes. Nonpositive indices select the first child. */
ef_addr32 walk_chain_from_8(int16_t steps, ef_addr32 group)
{
    ef_addr32 node = EF_U32(group + 8u);
    int16_t i = 0;
    while (i < steps) {
        node = EF_U32(node + 1u);
        ++i;
    }
    return node;
}

/* 0xaa4d4, 175 bytes. Resolve the signed high word at record+2
 * (i.e. the word at +4), then overwrite record+0 with that pointer.
 * Each opcode-14 payload has mandatory +0x1e and optional +0x22/+0x26. */
void patch_group_pointers_aa4d4(void)
{
    ef_addr32 group = EF_U32(0x419200u);
    uint16_t i, j;
    for (i = 0; (int32_t)(int16_t)i < (int32_t)EF_U16(0x4194c2u); ++i) {
        ef_addr32 node = EF_U32(group + 8u);
        for (j = 0; (int32_t)(int16_t)j < (int32_t)EF_U16(group + 0xcu); ++j) {
            if (EF_U8(node) == 14u) {
                ef_addr32 payload = EF_U32(node + 9u);
                ef_addr32 record = EF_U32(payload + 0x1eu);
                EF_U32(record) = walk_chain_from_8(EF_S16(record + 4u), group);
                record = EF_U32(payload + 0x22u);
                if (record != 0u)
                    EF_U32(record) = walk_chain_from_8(EF_S16(record + 4u), group);
                record = EF_U32(payload + 0x26u);
                if (record != 0u)
                    EF_U32(record) = walk_chain_from_8(EF_S16(record + 4u), group);
            }
            node = EF_U32(node + 1u);
        }
        group = EF_U32(group);
    }
}
