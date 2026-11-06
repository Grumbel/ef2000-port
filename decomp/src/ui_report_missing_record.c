/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

#pragma aux guest_callptr1_x parm [eax] [edx] modify [eax ecx edx]
#pragma aux ui_report_missing_record parm [eax] modify [eax ecx edx]
void ui_report_missing_record(ef_addr32 message)
{
    guest_callptr1_x(EF_U32(0x00132a74u), message);
}
