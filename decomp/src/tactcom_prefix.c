/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* Original 0x8b39c..0x8b472: the TACTCOM memory diagnostic, logging setup,
 * first resource pathname and missing-file decision. This is NOT the
 * complete 402-byte initializer. A private guest-local frame substitutes
 * for the Watcom stack until native startup has been reconstructed. */
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

    build_default_path_9c980(frame + 0xa0u);
    strcat_9ca57(frame + 0xa0u, 0x0018a848u);

    /* 0x8b45a: the installed indirect callback resolves through 0x9cb20
     * and 0xb3b90, falling back to the read-only host filesystem. */
    if (guest_tactcom_file_probe_8b45a(frame + 0xa0u) == 0)
        guest_tactcom_missing_resource_8b464(0x00132884u);

    /* 0x8b472: the next game-owned call is apply_options_82cec. */
    guest_tactcom_options_boundary_8b472();
}
