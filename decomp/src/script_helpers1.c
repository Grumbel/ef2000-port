/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Text script helpers. */

/* 0x1093d8, 95 bytes. Run a line-oriented text script: each non-empty line not starting with ';' is passed to the current handler (0x4dd890), which a '[' line resets to the default selector 0x10922c. */
void run_text_script_1093d8(ef_addr32 text, uint32_t table, uint32_t count)
{
    uint8_t line[0x50];
    /* Keep the cursor across callbacks with guest-specific register clobbers. */
    volatile uint32_t p = text;
    EF_U32(0x004dd898u) = table;
    EF_U32(0x004dd894u) = count;
    strip_cr_inplace(text);
    EF_U32(0x004dd890u) = 0x0010922cu;
    do {
        p = copy_line((uint32_t)(uintptr_t)line, p);
        if (line[0] != 0u && line[0] != 0x3bu) {
            if (line[0] == 0x5bu)
                EF_U32(0x004dd890u) = 0x0010922cu;
            guest_callptr_eax_x(EF_U32(0x004dd890u), (uint32_t)(uintptr_t)line);
        }
    } while (EF_U8(p) != 0u);
}

/* 0x9cbc4, 98 bytes. Try the two optional handlers (0x40d1f0 gated by bit 0 and 0x40d1f4 gated by bit 1 of 0x132ab0); return 1 if one accepts, else fall back to 0xb3c04. */
uint32_t try_handlers_9cbc4(uint32_t a, uint32_t b)
{
    uint32_t h = EF_U32(0x0040d1f0u);
    if (h != 0u && (EF_U8(0x00132ab0u) & 1u) && (uint8_t)guest_fn_eba64_x(a, b, h))
        return 1u;
    h = EF_U32(0x0040d1f4u);
    if (h != 0u && (EF_U8(0x00132ab0u) & 2u) && (uint8_t)guest_fn_eba64_x(a, b, h))
        return 1u;
    return guest_fn_b3c04_x(a, b);
}
