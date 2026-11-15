/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#pragma aux callback_zero_contract_fixture parm [eax] [edx] value [eax] modify [eax ecx edx]
uint32_t callback_zero_contract_fixture(uint32_t fn, uint32_t keep)
{
    guest_callptr0_x(fn);
    return keep;
}
