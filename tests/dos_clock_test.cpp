// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

#include "../runtime/dos_clock.hpp"

#include <chrono>
#include <cstdio>

int main()
{
    const auto milliseconds = [](uint64_t ticks, uint32_t divisor) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            ef2000::guest_dos_clock(ticks, divisor).time_since_epoch()).count();
    };
    if (milliseconds(0, 65536) != 946728000000LL ||
        milliseconds(1193182, 1) != 946728001000LL ||
        milliseconds(1, 65536) != 946728000054LL ||
        milliseconds(20, 65536) != 946728001098LL ||
        milliseconds(1193182 * 60ULL, 1) != 946728060000LL) {
        std::fputs("dos-clock: deterministic PIT conversion mismatch\n", stderr);
        return 1;
    }
    std::puts("dos-clock: deterministic PIT calendar conversion passed");
    return 0;
}
