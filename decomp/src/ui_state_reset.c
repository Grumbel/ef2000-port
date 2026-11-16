/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xac0e0, 139 bytes. No inputs, EAX scratch, six other registers
 * preserved, plain RET. Caller sites 84c96 and 84cac. */
void reset_ui_state_ac0e0(void)
{
    if (flag_is_one_15f930() != 0u)
        reset_engine_e4264();
    init_pattern_display_e4188();
    init_pool_ab8e0(100000u);
    latch_c382c();
    load_record_file_ca200(0x0018db90u);
    EF_U32(0x001378ddu) = EF_U32(0x0041eea8u);
    build_glyph_runs_e17f8();
    EF_U32(0x0041eeacu) = 0u;
    /* Original signed halves truncate toward zero before subtracting one. */
    EF_U16(0x0013725eu) = (uint16_t)(EF_S32(0x0015b744u) / 2 - 1);
    EF_U16(0x00137260u) = (uint16_t)(EF_S32(0x0015b748u) / 2 - 1);
    EF_U8(0x0041f2e7u) = 1u;
    clear_ui_state_ac16c();
    EF_U8(0x0041f833u) = 0u;
    EF_U8(0x0041f2e6u) = 0u;
}
