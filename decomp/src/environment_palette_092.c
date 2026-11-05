/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_environment_092.h"
#include "ef_mem.h"

#include <stdint.h>

/* 0xda5cc: the table at 15bd40 has fourteen fixed (name,callback)
 * pairs and a fifteenth fallback at 15bdb0. The original overwrites
 * that last name pointer with the stack-buffer address; comparing the
 * input string against itself then selects callback 0xdbf2c (error).
 * Our adapter cannot put a native host-stack pointer in guest RAM, so
 * it models the fallback explicitly after fourteen failed comparisons. */
void env_palette_dispatch_da5cc(void)
{
    ef_environment_fields_090 f;
    uint32_t descriptor;
    unsigned i;
    env_scan_line_090(0xda5ccu, 0x190000u, &f);
    /* Original 15bdb0 points into the guest stack, which has no address in
     * the standalone host test. Do not forge a guest stack pointer here. */
    for (i = 0u; i < 14u; ++i) {
        descriptor = 0x15bd40u + i * 8u;
        if (env_engine_call_090(0xa4380u, EF_U32(descriptor), 0u, 0u) == 0u) {
            (void)env_engine_call_090(EF_U32(descriptor + 4u), 0u, 0u, 0u);
            return;
        }
    }
    (void)env_engine_call_090(0xdbf2cu, 0u, 0u, 0u);
}

/* 0xda644: first cockpit palette is a special case, including a 0x300
 * word/block copy. Subsequent palettes select 768-byte destination banks
 * and 0x4004-byte source banks. The signed selector check is <12; any
 * out-of-range selector still increments the original 16-bit counter. */
void env_cockpit_palette_da644(void)
{
    ef_environment_fields_090 f;
    int32_t index;
    uint32_t entry;
    env_scan_line_090(0xda644u, 0x190004u, &f);
    index = EF_S16(0x425bcau);
    if (index == 0) {
        (void)env_engine_call_090(0xdbf3cu, 0x422760u, 0u, 0x433f30u);
        (void)env_engine_call_090(0xdc832u, 0x464060u, 0x422760u, 0x300u);
        EF_U32(0x425b98u) = f.number[0];
        EF_U32(0x424e60u) = f.number[0];
    } else if (index < 12) {
        (void)env_engine_call_090(0xdbf3cu,
                                  0x422760u + (uint32_t)index * 768u,
                                  0u,
                                  0x433f30u + (uint32_t)index * 0x4004u);
        entry = 0x424e60u + (uint32_t)(EF_S32(0x425bc8u) >> 16) * 4u;
        EF_U32(entry) = f.number[0];
    } else {
        env_report_090(0x19000cu);
    }
    EF_U16(0x425bcau) = (uint16_t)(index + 1);
}
