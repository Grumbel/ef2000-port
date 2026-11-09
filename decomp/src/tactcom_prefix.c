/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* Original 0x8b39c..0x8b52d: the TACTCOM memory diagnostic, logging setup,
 * first resource pathname, missing-file decision and options call. Not the
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

    /* 0x8b472: proceed through recovered 0x82cec until the first unbound
     * DOS hardware/initialization service; do not skip its side effects. */
    apply_options_82cec();
    /* 0x8b477: initialize the keyboard controller, key tables, request
     * buffers and timer; the cold probe stops in the missing X-32 pool. */
    guest_fn_cd4d4_x();
    guest_tactcom_post_options_boundary_8b477();
}

/* 0x8b47c..0x8b52d: conditional page/input setup and final mode selection.
 * 0xce08c is a recovered six-byte global read; its result is discarded by
 * this caller. The other handlers retain their original control-flow order,
 * but are dispatched via a bounded host callback until their dependencies
 * (notably DOS input/timer state) can be exercised in the native probe. */
void tactcom_after_input_8b47c(void)
{
    uint32_t input_device;

    (void)get_global_ce08c();
    if (EF_U8(0x0015bac6u) != 0u)
        guest_tactcom_unbound_service_8b47c(0x0007feb0u);
    if (EF_U8(0x0015bac7u) != 0u)
        guest_tactcom_unbound_service_8b47c(0x00081110u);

    input_device = EF_U8(0x00420a72u);
    if (input_device != 0u) {
        if (input_device == 2u)
            guest_tactcom_unbound_service_8b47c(0x000982a0u);
        guest_tactcom_unbound_service_8b47c(0x00097f90u);
        guest_tactcom_unbound_service_8b47c(0x00098000u);
    }

    /* The timer service has a real one-time initialization guard. An
     * already-initialized instance can return; first-time hardware setup
     * must remain an explicit host boundary. */
    guest_tactcom_unbound_service_8b47c(0x000e28e8u);

    /* 0x8b4c9: probe according to the hardware mode word. Modes 0 and
     * values above 3 do nothing; 1..3 require actual device probes. */
    guest_tactcom_unbound_service_8b47c(0x000e9ac0u);

    /* 0x8b4ce..0x8b511: the CS jump table at 0x8b38c maps modes
     * 0/1/2/3 to 0/'0'/'F'/'G'; out-of-range modes reset to zero. */
    switch (EF_U16(0x00464472u)) {
    case 1u: EF_U16(0x00464472u) = 0x30u; break;
    case 2u: EF_U16(0x00464472u) = 0x46u; break;
    case 3u: EF_U16(0x00464472u) = 0x47u; break;
    default: EF_U16(0x00464472u) = 0u; break;
    }

    /* 0x8b511..0x8b523: the signed high word of 0x464470 aliases the
     * remapped mode word at 0x464472. Call the actual recovered C bodies.
     * The second function calls 0xd6694 even with no joystick configured;
     * the bounded native host has a disconnected-gameport reader. */
    select_tables_1d3c4((uint16_t)(EF_S32(0x00464470u) >> 16));
    scale_words_1c60c();
}
