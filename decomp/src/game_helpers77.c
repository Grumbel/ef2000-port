/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Page handler for the mission dialog stack. */

/* 0x34f94, 403 bytes. Per-frame update of the mission dialog page: reload, timers and dialog switching. */
void update_mission_page_34f94(void)
{
    uint32_t dlg;
    uint32_t now;
    uint32_t kind;

    guest_fn_a4138_x();
    if (EF_U8(0x0019f5aeu) != 0 && (guest_fn_a41dc_x() & 0xffu) == 1) {
        EF_U8(0x0019f5aeu) = 0;
        EF_U8(0x00125902u) = 1;
        EF_U8(0x00125935u) = 1;
        guest_fn_33630();
        EF_U8(0x0019f59bu) = EF_U8(0x001357b0u);
        guest_fn_a42c4_x();
    }
    if (EF_U8(0x0013578cu) != 0 && EF_U8(0x00135798u) == 1) {
        guest_fn_a4030();
        guest_fn_34f30_x();
        if (EF_U8(0x0019f59bu) != EF_U16(0x001357b0u))
            guest_fn_33630();
        if (EF_U8(0x001357c8u) != 0) {
            now = ((uint32_t (*)(void))EF_U32(0x0015b620u))();
            if (now - EF_U32(0x0019f334u) > 0x32u) {
                guest_fn_a3f0c_x();
                EF_U32(0x0019f334u) = ((uint32_t (*)(void))EF_U32(0x0015b620u))();
            }
        }
    }
    if (EF_U8(0x0013578cu) == 0) {
        EF_U8(0x0019f59bu) = 0xff;
        if (EF_U8(0x0012598du) != EF_U8(0x0013578du)) {
            guest_fn_33630();
            EF_U8(0x0012598du) = EF_U8(0x0013578du);
        }
        if (EF_U8(0x0013578du) != 0) {
            dlg = guest_fn_a46cc_x(0x00182f34u);
            guest_destroy_named_dialog(dlg);
            dlg = guest_fn_a46cc_x(0x00182d00u);
            guest_fn_33b5c_x(dlg);
            dlg = guest_fn_a46cc_x(0x00182cd4u);
            guest_fn_33888_x(dlg);
            kind = guest_fn_a3bdc_x() & 0xffu;
            if (kind < 0x16u || kind > 0x17u) {
                guest_fn_34b2c_x();
            } else if (kind == 0x17u) {
                EF_U8(0x0019f5aeu) = 1;
                dlg = guest_fn_a46cc_x(0x00182d00u);
                guest_fn_33b5c_x(dlg);
                dlg = guest_fn_a46cc_x(0x00182cd4u);
                guest_fn_33888_x(dlg);
            }
        }
    }
    if (EF_U8(0x0013578du) != EF_U8(0x0019f59au)) {
        EF_U8(0x0019f59au) = EF_U8(0x0013578du);
        guest_fn_ac9e8();
    }
    if (EF_U8(0x0013583du) == 2) {
        dlg = guest_fn_a46cc_x(0x00182f40u);
        guest_emit_dialog_flag(EF_U8(dlg + 0x10u));
        guest_fn_34ea4_x();
        guest_fn_a3da0();
        guest_fn_33630();
    }
}
