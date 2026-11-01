// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../runtime/real_mode_cpu.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

template <class Callable> void expect_error(const std::string& phrase, Callable callable) {
    try { callable(); }
    catch (const std::runtime_error& error) {
        require(std::string(error.what()).find(phrase) != std::string::npos,
                "unexpected real-mode failure");
        return;
    }
    throw std::runtime_error("missing real-mode failure");
}
} // namespace

int main() {
    try {
        ef2000::RealModeCpu engine;
        std::vector<uint8_t> memory(0x100000);
        std::array<uint32_t, 256> vectors{};
        vectors[0x66] = 0x21000000;
        constexpr uint32_t entry = 0x21000000;
        const uint8_t driver[] = {
            0xb8, 0x34, 0x12,                   // MOV AX,1234h
            0xc6, 0x06, 0x80, 0x00, 0x5a,      // MOV byte [0080h],5Ah
            0xcf};                             // IRET through synthetic far frame
        std::copy(std::begin(driver), std::end(driver), memory.begin() + 0x21000);
        memory[0x9fefa] = 0x66;
        memory[0xf0000] = 0x55;
        const ef2000::RealRegisters regs{0, 0xbeef, 0x101, 0, 0, 0, 0x2100, 0x2200,
                                         0, 0, 0x202};
        const auto result = engine.execute(entry, vectors, memory, regs);
        require((result.registers.ax & 0xffff) == 0x1234, "16-bit return AX");
        require((result.registers.bx & 0xffff) == 0xbeef, "preserve guest BX");
        require(result.memory[0x21080] == 0x5a && memory[0x21080] == 0,
                "driver writes to copied conventional memory");
        require(result.memory[0x9fefa] == 0x66 && result.memory[0xf0000] == 0x55,
                "synthetic driver return mechanism must not pollute guest RAM");
        require(result.memory[0x66 * 4] == 0 && result.memory[0x66 * 4 + 2] == 0,
                "real-mode IVT representation");
        require(result.memory[0x66 * 4 + 3] == 0x21, "high segment byte in IVT");

        // Replacing driver bytes between calls must invalidate Unicorn's
        // translated blocks and genuinely execute the new guest instructions.
        memory[0x21001] = 0x78;
        memory[0x21002] = 0x56;
        const auto changed = engine.execute(entry, vectors, memory, regs);
        require((changed.registers.ax & 0xffff) == 0x5678, "stale translated driver code");

        memory[0x21000] = 0xcd; // INT 80h has no nested DOS handler yet.
        memory[0x21001] = 0x80;
        expect_error("nested real-mode INT 128", [&] {
            engine.execute(entry, vectors, memory, regs);
        });
        memory[0x21000] = 0xeb; // JMP $: verify bounded execution.
        memory[0x21001] = 0xfe;
        expect_error("instruction/time limit", [&] {
            engine.execute(entry, vectors, memory, regs, 1000);
        });
        expect_error("invalid real-mode driver", [&] {
            engine.execute(0x00001234, vectors, memory, regs);
        });

        // An original-mode guest drives the optional SB16 discovery model
        // through actual IN/OUT instructions, not host-side canned register
        // values or Python glue.
        ef2000::RealModeCpu sb16(true);
        std::vector<uint8_t> sound_memory(0x100000);
        const uint8_t reset[] = {
            0xba, 0x26, 0x02, // MOV DX,226h
            0xb0, 0x01,       // MOV AL,1
            0xee,             // OUT DX,AL
            0x30, 0xc0,       // XOR AL,AL
            0xee,             // OUT DX,AL
            0xba, 0x2a, 0x02, // MOV DX,22Ah
            0xec,             // IN AL,DX
            0xcf};            // IRET
        std::copy(std::begin(reset), std::end(reset), sound_memory.begin() + 0x21000);
        const auto response = sb16.execute(entry, vectors, sound_memory, regs);
        require((response.registers.ax & 0xff) == 0xaa, "16-bit SB16 reset handshake");
        const uint8_t version[] = {
            0xba, 0x2c, 0x02, // MOV DX,22Ch
            0xb0, 0xe1, 0xee, // DSP version command
            0xba, 0x2a, 0x02, // MOV DX,22Ah
            0xec,             // IN AL,DX (major version)
            0xcf};
        std::copy(std::begin(version), std::end(version), sound_memory.begin() + 0x21000);
        const auto major = sb16.execute(entry, vectors, sound_memory, regs);
        require((major.registers.ax & 0xff) == 4, "16-bit SB16 DSP major version");

        // The DMA-mode bridge forwards ordinary guest x86 port instructions
        // to the original 8237 register/flip-flop interface as well.
        ef2000::RealModeCpu dma(ef2000::SoundMode::Dma);
        const uint8_t configure_dma[] = {
            0xba, 0x0c, 0x00, // MOV DX,0Ch (DMA flip-flop clear)
            0x30, 0xc0, 0xee, // XOR AL,AL / OUT DX,AL
            0xba, 0x02, 0x00, // MOV DX,02h (channel 1 address)
            0xb0, 0x34, 0xee, // Low address byte
            0xb0, 0x12, 0xee, // High address byte
            0xba, 0x0c, 0x00, 0xee, // Reset flip-flop
            0xba, 0x02, 0x00, 0xec, // IN AL,DX -> 34h
            0xcf};           // IRET
        std::copy(std::begin(configure_dma), std::end(configure_dma),
                  sound_memory.begin() + 0x21000);
        const auto configured = dma.execute(entry, vectors, sound_memory, regs);
        require((configured.registers.ax & 0xff) == 0x34,
                "guest 16-bit IN/OUT accesses live C++ DMA state");

        // A complete synthetic original-mode playback cycle: guest programs
        // PIC, word DMA and DSP via OUT, waits for IRQ7, acknowledges the DSP
        // and PIC from its own 16-bit ISR, then returns to the X-32 caller.
        ef2000::RealModeCpu playback(ef2000::SoundMode::Dma);
        std::vector<uint8_t> playback_memory(0x100000);
        for (unsigned i = 0; i < 8; ++i) playback_memory[0x10000 + i] = uint8_t(i + 1);
        std::vector<uint8_t> playback_code;
        auto emit_out = [&](uint16_t port, uint8_t value) {
            playback_code.insert(playback_code.end(),
                                 {0xba, uint8_t(port), uint8_t(port >> 8),
                                  0xb0, value, 0xee});
        };
        emit_out(0x226, 1);
        emit_out(0x226, 0);
        emit_out(0x21, 0x78);  // Unmask PIC IRQ7.
        for (const auto& [port, value] :
                std::array<std::pair<uint16_t, uint8_t>, 8>{{{0xd8, 0}, {0xc4, 0},
                    {0xc4, 0x80}, {0xc6, 3}, {0xc6, 0}, {0x8b, 0},
                    {0xd6, 0x49}, {0xd4, 1}}}) emit_out(port, value);
        for (const uint8_t command : {0x41, 0xff, 0xff, 0xb0, 0x30, 3, 0})
            emit_out(0x22c, command);
        playback_code.insert(playback_code.end(), {
            0xfb,                         // STI
            0x80, 0x3e, 0x00, 0x02, 0x01, // CMP byte [0200h],1
            0x75, 0xf9,                   // JNE back to CMP
            0xcf});                       // IRET
        std::copy(playback_code.begin(), playback_code.end(), playback_memory.begin() + 0x21000);
        const uint8_t irq7_handler[] = {
            0x1e,                   // PUSH DS
            0xb8, 0x00, 0x21,       // MOV AX,2100h
            0x8e, 0xd8,             // MOV DS,AX (IRQ may interrupt any segment)
            0xfe, 0x06, 0x00, 0x02, // INC byte [0200h]
            0x1f,                   // POP DS
            0xba, 0x2f, 0x02, 0xec, // Read 16-bit DSP acknowledgement port
            0xba, 0x20, 0x00, 0xb0, 0x20, 0xee, // PIC EOI
            0xcf};                   // IRET back to interrupted code
        std::copy(std::begin(irq7_handler), std::end(irq7_handler),
                  playback_memory.begin() + 0x22000);
        vectors[0x0f] = 0x22000000;
        const auto played = playback.execute(entry, vectors, playback_memory, regs);
        require(played.memory[0x21200] == 1, "original-mode IRQ7 handler must run");
        require(played.audio_blocks.size() == 1 &&
                played.audio_blocks[0].pcm == std::vector<uint8_t>({1, 2, 3, 4, 5, 6, 7, 8}),
                "16-bit guest DSP+DMA+IRQ cycle captures exact programmed PCM");

        // The usual game path returns to 32-bit execution before playback
        // completes. A later protected-mode timer tick must transfer PCM and
        // run the installed 16-bit IRQ handler without a guest BIOS call.
        ef2000::RealModeCpu background(ef2000::SoundMode::Dma);
        std::vector<uint8_t> background_memory = playback_memory;
        std::vector<uint8_t> start_code;
        auto background_out = [&](uint16_t port, uint8_t value) {
            start_code.insert(start_code.end(), {0xba, uint8_t(port), uint8_t(port >> 8),
                                                  0xb0, value, 0xee});
        };
        background_out(0x226, 1);
        background_out(0x226, 0);
        background_out(0x21, 0x78);
        for (const auto& [port, value] :
                std::array<std::pair<uint16_t, uint8_t>, 8>{{{0xd8, 0}, {0xc4, 0},
                    {0xc4, 0x80}, {0xc6, 3}, {0xc6, 0}, {0x8b, 0},
                    {0xd6, 0x49}, {0xd4, 1}}}) background_out(port, value);
        for (const uint8_t command : {0x41, 0, 1, 0xb0, 0x30, 3, 0})
            background_out(0x22c, command); // 1 Hz: completion in 2s.
        start_code.push_back(0xcf);
        std::copy(start_code.begin(), start_code.end(), background_memory.begin() + 0x21000);
        const auto started = background.execute(entry, vectors, background_memory, regs);
        require(started.audio_blocks.empty() && started.memory[0x21200] == 0,
                "background DMA must not finish inside the initial real-mode call");
        const double scheduled = background.next_audio_deadline();
        require(scheduled > 0 && !background.audio_service_due(scheduled - 0.5) &&
                background.audio_service_due(scheduled), "audio background deadline");
        const auto serviced = background.service_audio(scheduled, vectors, started.memory);
        if (serviced.memory.size() != 0x100000 || serviced.memory[0x21200] != 1 ||
            serviced.audio_blocks.size() != 1 ||
            serviced.audio_blocks[0].pcm !=
                std::vector<uint8_t>({1, 2, 3, 4, 5, 6, 7, 8}))
            throw std::runtime_error("protected-mode audio tick: memory=" +
                std::to_string(serviced.memory.size()) + " irq_count=" +
                (serviced.memory.empty() ? "?" : std::to_string(serviced.memory[0x21200])) +
                " blocks=" + std::to_string(serviced.audio_blocks.size()));
        require(!background.audio_service_due(scheduled),
                "completed one-shot DMA must not retrigger after guest EOI");

        std::cout << "real-mode: guest return, memory exchange and bounded failure tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "real-mode self-test: " << error.what() << '\n';
        return 1;
    }
}
