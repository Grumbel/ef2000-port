/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* Every guest call records its original address. The host-only hook is a
 * semantic boundary; EDX preservation across an unresolved call is not yet
 * a confirmed guest calling convention. */
static uint32_t guest(uint32_t entry, uint32_t eax, uint32_t edx)
{
    return gui_guest_call_089(entry, eax, edx);
}

/* 0x4b54, 39 bytes, two alternative external JMPs at 487c / ac7a8. */
void gui_kots_init_4b54(ef_addr32 widget)
{
    uint32_t child = EF_U32(widget + 0x51u);
    uint32_t decision;
    guest(0xa4b80u, EF_U8(child + 0x10u), 0u);
    decision = guest(0x432cu, 0u, 0u);
    if ((uint8_t)decision != 0u)
        guest(0x487cu, decision, 0u);
    else
        guest(0xac7a8u, 0x17f79cu, 0u);
}

/* 0x84ddc, 13 bytes: widget child ID followed by external terminal JMP. */
void gui_show_summary_info_84ddc(ef_addr32 widget)
{
    guest(0xa4b80u, EF_U8(widget + 0x10u), 0u);
}

/* 0xda2b0, 8 bytes: enable cloud rendering. */
void env_clouds_da2b0(void)
{
    EF_U8(0x425c88u) = 1u;
}
