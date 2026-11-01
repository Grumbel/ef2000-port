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

        std::cout << "real-mode: guest return, memory exchange and bounded failure tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "real-mode self-test: " << error.what() << '\n';
        return 1;
    }
}
