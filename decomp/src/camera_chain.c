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

/* 0x974b8, 121 bytes. Advance the chain clock; once it exceeds 1.0
 * (unordered also continues: FLD1/FCOMPS/SAHF/JAE), subtract one through
 * the double constant -1.0 at 0x18ac52 and step every record's animation
 * with 0x97860. A record whose step returns nonzero makes its successor
 * the new head; the walk always continues with the successor reloaded
 * after the call. Exits use POP EDX/POP EBX/RET at 0x94a70; EAX scratch.
 * Sole caller draw_chain_97464. */
void step_camera_chain_974b8(void)
{
    int16_t index;

    g_camera_chain_clock = g_camera_chain_step + g_camera_chain_clock;
    if (1.0f >= g_camera_chain_clock)
        return;
    g_camera_chain_clock = (float)(g_camera_chain_clock + g_camera_chain_wrap);
    index = g_camera_chain_head;
    while (index != -1) {
        if (step_animation_97860((ef_addr32)(uintptr_t)&g_camera_chain_records[index]) != 0)
            g_camera_chain_head = g_camera_chain_records[index].next;
        index = g_camera_chain_records[index].next;
    }
}
