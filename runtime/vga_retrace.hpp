// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
#pragma once
#include <cstdint>

namespace ef2000 {
// Existing hybrid retrace policy, including its sampled clock and cached reads.
class VgaRetrace {
public:
    void start_slice() { reads_until_check_ = 1; }
    int64_t next_guest_time_ns() { return static_cast<int64_t>(guest_samples_++ * 100000); }
    template<class Clock>
    uint32_t read(unsigned interval, Clock clock) {
        if (--reads_until_check_ == 0) {
            reads_until_check_ = interval;
            const int64_t ns = clock();
            status_ = ns % 16666667 >= 15000000 ? 8 : 0;
        }
        return status_;
    }
private:
    unsigned reads_until_check_{1};
    uint32_t status_{};
    uint64_t guest_samples_{};
};
} // namespace ef2000
