/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"

uint32_t callback_stack1_contract_fixture(ef_addr32 fn, uint32_t keep)
{
    return guest_callptr_stack1_ret_x(fn, 41) + keep;
}
