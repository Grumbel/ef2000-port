/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_STRUCTS_CLOCK_H
#define EF_STRUCTS_CLOCK_H

#include "ef_types.h"

/* Stored as wrapping words; arithmetic and comparisons interpret them signed. */
typedef struct EF_PACKED ef_clock {
    uint16_t hours;
    uint16_t minutes;
    uint16_t seconds;
    uint16_t ticks;
} ef_clock;

EF_STATIC_ASSERT(ef_clock_hours_off, offsetof(ef_clock, hours) == 0);
EF_STATIC_ASSERT(ef_clock_minutes_off, offsetof(ef_clock, minutes) == 2);
EF_STATIC_ASSERT(ef_clock_seconds_off, offsetof(ef_clock, seconds) == 4);
EF_STATIC_ASSERT(ef_clock_ticks_off, offsetof(ef_clock, ticks) == 6);
EF_STATIC_ASSERT(ef_clock_size, sizeof(ef_clock) == 8);

#endif
