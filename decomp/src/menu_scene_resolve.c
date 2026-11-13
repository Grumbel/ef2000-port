/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#include "ef_abi.h"
#include "ef_mem.h"

/* 0xaa350, 140 bytes. Walk the group/child counts using signed word
 * counters. Diagnose opcode-3 references, then resolve them even if the
 * diagnostic callback returns. Opcode 10 resolves a list reference only. */
void resolve_scene_groups_aa350(void)
{
    ef_addr32 group = EF_U32(0x419200u);
    uint16_t i, j;
    for (i = 0u; (int32_t)(int16_t)i < (int32_t)EF_U16(0x4194c2u); ++i) {
        ef_addr32 node = EF_U32(group + 8u);
        for (j = 0u; (int32_t)(int16_t)j < (int32_t)EF_U16(group + 12u); ++j) {
            uint8_t opcode = EF_U8(node);
            if (opcode == 3u) {
                if (guest_fn_aa2f8(EF_U32(node + 9u)) != 0u) {
#ifdef __WATCOMC__
                    typedef void (__cdecl *Reporter)(ef_addr32, int32_t, int32_t);
#else
                    typedef void (*Reporter)(ef_addr32, int32_t, int32_t);
#endif
                    ((Reporter)(uintptr_t)EF_U32(0x132a74u))(
                        0x18d888u, (int32_t)(int16_t)i, (int32_t)(int16_t)j);
                }
                resolve_child_aa2b4(EF_U32(node + 9u));
                guest_fn_aa198(EF_U32(node + 9u));
            } else if (opcode == 10u) {
                guest_fn_aa1c0(EF_U32(node + 9u));
            }
            node = EF_U32(node + 1u);
        }
        group = EF_U32(group);
    }
}
