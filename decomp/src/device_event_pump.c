/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xa410c,107 owned bytes in three spans:
 * a410c..a412c, a39d9..a39eb, a3583..a35b9.
 * No inputs; preserves all six non-EAX GPRs. Sole caller a4057
 * ignores EAX and retains EBX's running total after the call. */
void pump_device_events_a410c(void)
{
    uint32_t index = 0u;

    if (EF_U8(0x001357c8u) == 0u) {
        find_lowest_rank_a38f8();
        accumulate_offsets_a36b0();
    } else {
        dispatch_all_a33f0();
        accumulate_samples_a35bc();
    }
    /* The original tests only DX, reloading the bound and list each time. */
    while ((uint32_t)(uint16_t)index < EF_U8(EF_U32(0x00136b88u))) {
        handle_event_record_a348c(EF_U8(EF_U32(0x00136b84u) + (uint16_t)index));
        ++index;
    }
    if (EF_U8(0x0013583cu) != 0u)
        (void)scan_entries_a2ef4();
}
