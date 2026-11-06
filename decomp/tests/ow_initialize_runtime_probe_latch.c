/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#include "ef_abi.h"

#if defined(__WATCOMC__)
static void set_runtime_probe_latch(void);
#pragma aux set_runtime_probe_latch = \
    0x66 0xc7 0x05 0xd0 0x1e 0x12 0x00 0x96 0x00 __modify []
#endif

void initialize_runtime_probe_latch(void)
{
    uint16_t output_at_esp;
    uint16_t output_at_esp_plus_4;
    mouse_read_motion_ce02c((ef_addr32)(uintptr_t)&output_at_esp_plus_4,
                            (ef_addr32)(uintptr_t)&output_at_esp);
    set_runtime_probe_latch();
}
