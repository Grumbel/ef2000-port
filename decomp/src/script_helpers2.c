/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Script section runners. */

static void run_section_common(uint16_t mode, uint32_t flag_a, uint32_t flag_b, uint32_t flag_c,
                               uint32_t msg_a, uint32_t msg_b, uint32_t msg_c, uint32_t done_flag)
{
    EF_U16(0x00485dfcu) = mode;
    EF_U8(flag_a) = 0u;
    EF_U8(flag_b) = 0u;
    EF_U8(flag_c) = 0u;
    guest_fn_f46cc();
    if (EF_U8(0x00485e08u) != 0u) {
        do {
            run_matching_handler_f478c();
            guest_fn_f46cc();
        } while (EF_U8(0x00485e08u) != 0u);
    }
    if (EF_U8(flag_a) == 0u)
        guest_callptr2_x(EF_U32(0x00132a74u), msg_a, EF_U32(0x00161ea8u));
    if (EF_U8(flag_b) == 0u)
        guest_callptr2_x(EF_U32(0x00132a74u), msg_b, EF_U32(0x00161ea8u));
    if (EF_U8(flag_c) == 0u)
        guest_callptr2_x(EF_U32(0x00132a74u), msg_c, EF_U32(0x00161ea8u));
    EF_U8(done_flag) = 1u;
}

/* 0xf4c60, 169 bytes. Run the mode-1 script section, complaining about each missing part (flags 0x485f72, 0x485f6f, 0x485f73). */
void run_section_one_f4c60(void)
{
    run_section_common(1u, 0x00485f72u, 0x00485f6fu, 0x00485f73u, 0x00191fa4u, 0x00191fd4u, 0x00192004u, 0x00485f77u);
}

/* 0xf4d0c, 169 bytes. Run the mode-2 script section (flags 0x485f6d, 0x485f6c, 0x485f6e). */
void run_section_two_f4d0c(void)
{
    run_section_common(2u, 0x00485f6du, 0x00485f6cu, 0x00485f6eu, 0x00192034u, 0x00192064u, 0x00192094u, 0x00485f74u);
}
