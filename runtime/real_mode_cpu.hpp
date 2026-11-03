// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

#pragma once

#include "sound_blaster.hpp"
#include <unicorn/unicorn.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ef2000 {

enum class SoundMode { Off, Probe, Dma };

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
    std::vector<AudioBlock> audio_blocks;
};

// No memory buffer is returned if only DMA completed. When an IRQ handler
// executes, the resulting conventional RAM must be copied into the X-32 map.
struct AudioServiceResult {
    std::vector<uint8_t> memory;
    std::vector<AudioBlock> audio_blocks;
};

class RealModeCpu {
public:
    explicit RealModeCpu(SoundMode mode = SoundMode::Off);
    explicit RealModeCpu(bool sb16_probe) : RealModeCpu(sb16_probe ? SoundMode::Probe : SoundMode::Off) {}
    ~RealModeCpu();
    RealModeCpu(const RealModeCpu&) = delete;
    RealModeCpu& operator=(const RealModeCpu&) = delete;

    RealResult execute(uint32_t entry, const std::array<uint32_t, 256>& vectors,
                       const std::vector<uint8_t>& memory, const RealRegisters& regs,
                       uint64_t instruction_limit = 1000000);
    bool audio_service_due(double now) const;
    AudioServiceResult service_audio(double now, const std::array<uint32_t, 256>& vectors,
                                     const std::vector<uint8_t>& memory);
    double next_audio_deadline() const;

private:
    static void interrupt(uc_engine*, uint32_t vector, void* user);
    static uint32_t port_in(uc_engine*, uint32_t port, int size, void* user);
    static void port_out(uc_engine*, uint32_t port, int size, uint32_t value, void* user);
    static void audio_block(uc_engine*, uint64_t address, uint32_t size, void* user);
    void inject_audio_irq(unsigned irq);
    void fail(const std::string& message);
    void check(uc_err error, const char* operation);

    uc_engine* cpu_{};
    std::string error_;
    SoundMode sound_mode_{};
    std::optional<unsigned> pending_audio_irq_;
    SB16Probe sb16_probe_;
    SoundBlaster16 sb16_dma_;
};

} // namespace ef2000
