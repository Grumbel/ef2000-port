/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_gui_options_096.h"
#include "ef_abi.h"
#include "ef_mem.h"

#include <stdint.h>

enum { INPUT_LINE = 0x485956, COMMAND = 0x48592c };

/* B1 direct-call recipe: replace guest_fn adapters with recovered typed
 * calls, then preserve every caller-live register in the public ABI.
 * 0x000f2d74, 86 bytes including its fallthrough at f2da4. The original
 * lexer has already advanced the underlying file pointer by one line when
 * the comment, short-line, or label checks request another iteration.
 * Unlike f46cc, this is the OPTIONS file's separate tokenizer. */
void gui_options_read_command_f2d74(void)
{
    for (;;) {
        uint32_t length;
        uint32_t i;
        read_script_line_f2c78();
        if (EF_U8(INPUT_LINE) == ';')
            continue;
        length = strlen_ds(INPUT_LINE);
        if (length < 3u)
            continue;
        read_script_token_f2cd0();
        if (path_ends_with_colon() != 0u)
            continue;
        /* The accepted command falls into the f2da4 code. That code uses
         * guest helper e7835 (ASCII A-Z to a-z), not the uppercase fold
         * used by the label matcher in f44b6. */
        for (i = 0u; EF_U8(COMMAND + i) != 0u; ++i) {
            uint8_t ch = EF_U8(COMMAND + i);
            EF_U8(COMMAND + i) = (uint8_t)to_lower_ascii(ch);
        }
        return;
    }
}

/* 0x000f6124, 38 bytes. Call the current cdecl formatter with the four
 * outgoing arguments in their original stack order. Its return value is
 * unused by all callers; the wrapper preserves EDX around the callback. */
void format_global_option_line(void)
{
#ifdef EF_DECOMP_HOST_TEST
    (void)gui_options_format_096(0x192c4cu, EF_U32(0x464390u),
                                 EF_U16(0x16007cu), 0x464396u);
#else
    typedef void (EF_CDECL *GuiOptionFormatter)(ef_addr32, uint32_t,
                                                uint32_t, ef_addr32);
    GuiOptionFormatter formatter = (GuiOptionFormatter)(uintptr_t)EF_U32(0x132a74u);
    formatter(0x192c4cu, EF_U32(0x464390u), EF_U16(0x16007cu), 0x464396u);
#endif
}
