/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#pragma aux callback_contract_fixture parm [eax] [edx] [ecx] value [eax] modify [eax ecx edx]
uint32_t callback_contract_fixture(uint32_t fn, uint32_t arg, uint32_t keep)
{
    return guest_callptr_eax_ret_x(fn, arg) + keep;
}
