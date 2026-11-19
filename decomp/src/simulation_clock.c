/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_guest_memory.h"
#include "ef_structs/clock.h"

/* Shared returns need no separate C boundary: restore the entry's preserved
 * registers through its ABI, then return normally.
 * 0x407c4, 94 bytes including the shared return at 0x40770.
 * EAX is preserved; only EDX is scratch. Every field update wraps at 16 bits.
 * ORIGINAL BUG: ticks carry only above 50, leaving an exact 50 unnormalized.
 * Other fields are normalized only when the preceding field carries. */
ef_addr32 advance_clock_ticks_407c4(ef_addr32 clock, int32_t delta)
{
    ef_clock *time = (ef_clock *)ef_guest_memory_ptr(clock);
    time->ticks = (uint16_t)(time->ticks + (uint16_t)delta);
    while ((int16_t)time->ticks > 50) {
        time->seconds = (uint16_t)(time->seconds + 1u);
        time->ticks = (uint16_t)(time->ticks - 50u);
        while ((int16_t)time->seconds > 59) {
            time->seconds = (uint16_t)(time->seconds - 60u);
            time->minutes = (uint16_t)(time->minutes + 1u);
            while ((int16_t)time->minutes > 59) {
                time->minutes = (uint16_t)(time->minutes - 60u);
                time->hours = (uint16_t)(time->hours + 1u);
                while ((int16_t)time->hours > 23)
                    time->hours = (uint16_t)(time->hours - 24u);
            }
        }
    }
    return clock;
}
