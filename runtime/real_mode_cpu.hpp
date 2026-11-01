// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "sb16_probe.hpp"
#include <unicorn/unicorn.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ef2000 {

// General-purpose 16-bit driver entry ABI used by the X-32 real-mode bridge.
// Driver code executes in Unicorn directly, never in a Python subprocess.
struct RealRegisters {
    uint32_t ax{}, bx{}, cx{}, dx{}, si{}, di{};
    uint16_t ds{}, es{}, fs{}, gs{};
    uint32_t flags{};
};

struct RealResult {
    RealRegisters registers;
    std::vector<uint8_t> memory;
};

class RealModeCpu {
public:
    explicit RealModeCpu(bool sb16_probe = false);
    ~RealModeCpu();
    RealModeCpu(const RealModeCpu&) = delete;
    RealModeCpu& operator=(const RealModeCpu&) = delete;

    RealResult execute(uint32_t entry, const std::array<uint32_t, 256>& vectors,
                       const std::vector<uint8_t>& memory, const RealRegisters& regs,
                       uint64_t instruction_limit = 1000000);

private:
    static void interrupt(uc_engine*, uint32_t vector, void* user);
    static uint32_t port_in(uc_engine*, uint32_t port, int size, void* user);
    static void port_out(uc_engine*, uint32_t port, int size, uint32_t value, void* user);
    void fail(const std::string& message);
    void check(uc_err error, const char* operation);

    uc_engine* cpu_{};
    std::string error_;
    bool sb16_probe_enabled_{};
    SB16Probe sb16_probe_;
};

} // namespace ef2000
