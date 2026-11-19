/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_structs/camera_chain.h"

/* 0x97b18, 71 bytes. Advance the chain clock by the step (single-precision
 * x87 add, step + clock, stored as float), then apply every camera record
 * from the chain head while its depth is at least 2. Both exits use the
 * POP EDX/POP EBX/RET at 0x97a8e. EAX keeps the last sign-extended index
 * (-1 or the stopping record). Sole caller 0x3dd30@0x3e141. */
int32_t advance_camera_chain_97b18(void)
{
    int16_t index;

    g_camera_chain_clock = g_camera_chain_step + g_camera_chain_clock;
    index = g_camera_chain_head;
    while (index != -1) {
        if (g_camera_chain_records[index].depth < 2)
            break;
        apply_camera_record_97a94(index);
        index = g_camera_chain_records[index].next;
    }
    return index;
}
