// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

#ifndef EF2000_DOS_CLOCK_HPP
#define EF2000_DOS_CLOCK_HPP

#include <chrono>
#include <cstdint>

namespace ef2000 {

// Diagnostic DOS clock: 2000-01-01 12:00:00 UTC plus the guest PIT time.
// Converts *whole PIT ticks* (not host milliseconds), so repeated bounded
// instruction-counted runs get the same DOS INT 21h AH=2Ah/2Ch responses.
inline std::chrono::system_clock::time_point guest_dos_clock(uint64_t ticks,
                                                              uint32_t divisor)
{
    constexpr uint64_t pit_hz = 1193182;
    constexpr int64_t epoch_milliseconds = 946728000000LL;
    const uint64_t counts = ticks * divisor;
    const uint64_t millis = (counts / pit_hz) * 1000 +
                            ((counts % pit_hz) * 1000) / pit_hz;
    return std::chrono::system_clock::time_point(
        std::chrono::milliseconds(epoch_milliseconds + static_cast<int64_t>(millis)));
}

} // namespace ef2000

#endif
