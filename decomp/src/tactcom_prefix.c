/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* Original 0x8b39c..0x8b43c only: the TACTCOM memory diagnostic and its
 * following logging callback setup.  This is deliberately NOT the complete
 * 402-byte 0x8b39c function.  The guest-local frame is supplied by the
 * native probe until its Watcom stack is implemented. */
void tactcom_prefix_8b39c(ef_addr32 frame)
{
    int32_t available_kb = (int32_t)dos_call_350b_880() / 1024;

    format_number_commas_8b328(frame + 0x50u, 40, available_kb);
    guest_fn_cd12e(0x0018a810u, frame + 0x50u, 0);
    (void)forward_or_clear_ea4a7(0x0016043eu);

    if (available_kb < 30000) {
        /* The original formats the shortfall and the 30,000 KB threshold,
         * then invokes the current 0x132a74 diagnostic callback. */
        format_number_commas_8b328(frame, 40, 30000 - available_kb);
        format_number_commas_8b328(frame + 0x50u, 40, 30000);
        guest_tactcom_low_memory_8b417(frame + 0x50u, frame);
    }

    EF_U32(0x00132a74u) = 0x000ea50cu;
    EF_U32(0x00132a78u) = 0x0018a838u;

    /* Next original call is 0x9c980: construct the configuration path.
     * Stop there until the native filesystem/path bindings are ready. */
    guest_fn_9c980_sfx(frame + 0xa0u, 0);
}
