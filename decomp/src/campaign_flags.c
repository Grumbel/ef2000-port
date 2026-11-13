/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x40ebc, 8 bytes: set the campaign-performance flag. */
void gc_start_new_campaign_40ebc(void)
{
    EF_U8(0x1a364cu) = 1u;
}

/* 0x40ec4, 9 bytes: clear the campaign-performance flag. */
uint32_t gc_performance_yes_40ec4(uint32_t eax)
{
    /* XOR AH,AH also returns the incoming EAX with bits 8..15 cleared. */
    eax &= 0xffff00ffu;
    EF_U8(0x1a364cu) = 0u;
    return eax;
}

