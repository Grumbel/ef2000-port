/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"

/* These helpers read the live selectors in the guest's protected-mode state. */
static uint32_t read_es(void);
#pragma aux read_es = 0x31 0xc0 0x8c 0xc0 __value [eax] __modify [eax]
static uint32_t read_cs(void);
#pragma aux read_cs = 0x31 0xc0 0x8c 0xc8 __value [eax] __modify [eax]
static uint32_t read_ss(void);
#pragma aux read_ss = 0x31 0xc0 0x8c 0xd0 __value [eax] __modify [eax]
static uint32_t read_ds(void);
#pragma aux read_ds = 0x31 0xc0 0x8c 0xd8 __value [eax] __modify [eax]
static uint32_t read_fs(void);
#pragma aux read_fs = 0x31 0xc0 0x8c 0xe0 __value [eax] __modify [eax]
static uint32_t read_gs(void);
#pragma aux read_gs = 0x31 0xc0 0x8c 0xe8 __value [eax] __modify [eax]

uint32_t guest_seg_x(uint32_t which)
{
    switch (which) {
    case 0: return read_es();
    case 1: return read_cs();
    case 2: return read_ss();
    case 3: return read_ds();
    case 4: return read_fs();
    case 5: return read_gs();
    default: return 0;
    }
}
