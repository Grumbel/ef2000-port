/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"

uint32_t status_cdecl_contract_fixture(void)
{
    format_ui_status_pair(0x5000, 0x6000, 11, 22);
    format_ui_status_float(0x5000, 0x6000, 1.25);
    format_ui_status_value(0x5000, 0x6000, 33);
    guest_fn_a4342_plain(0x5000, 0x6000);
    return draw_tactical_mode_item(1, 0x6000);
}
