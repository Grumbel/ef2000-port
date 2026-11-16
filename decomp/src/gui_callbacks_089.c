/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x4b54, 39 bytes. EAX widget; six other GPRs preserved. The two
 * terminal branches enter complete functions with no extra stack arguments. */
void gui_kots_init_4b54(ef_addr32 widget)
{
    ef_addr32 child = EF_U32(widget + 0x51u);
    close_widget_a4b80(EF_U8(child + 0x10u));
    if (hiscore_qualifies() != 0u)
        select_first_entry_487c();
    else
        set_pending_name_ac7a8(0x0017f79cu);
}

/* 0x84ddc, 13 bytes. EAX widget; terminal JMP closes its byte-sized ID. */
void gui_show_summary_info_84ddc(ef_addr32 widget)
{
    close_widget_a4b80(EF_U8(widget + 0x10u));
}

/* 0xda2b0, 8 bytes. No semantic inputs, all seven GPRs preserved, plain
 * RET. The C identity input/output explicitly retains incoming EAX. */
uint32_t env_clouds_da2b0(uint32_t preserved_eax)
{
    EF_U8(0x00425c88u) = 1u;
    return preserved_eax;
}
