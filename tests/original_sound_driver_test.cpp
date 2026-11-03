// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

// Optional integration test: supply the original (non-redistributable)
// SB16.DIG from a local EF2000 installation. Never copy it into a build.
#include "../runtime/real_mode_cpu.hpp"

#include <openssl/sha.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

uint16_t word(const std::vector<uint8_t>& memory, size_t offset) {
    return uint16_t(memory.at(offset) | (unsigned(memory.at(offset + 1)) << 8));
}

void set_word(std::vector<uint8_t>& memory, size_t offset, uint16_t value) {
    memory.at(offset) = value & 255;
    memory.at(offset + 1) = value >> 8;
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 1) {
            std::cout << "original-sb16: skipped (pass /path/to/SB16.DIG to test installed assets)\n";
            return 0;
        }
        if (argc != 2) throw std::runtime_error("usage: original-sb16-test /path/to/SB16.DIG");
        std::ifstream file(argv[1], std::ios::binary);
        if (!file) throw std::runtime_error("cannot open original SB16.DIG");
        const std::vector<uint8_t> driver((std::istreambuf_iterator<char>(file)), {});
        require(driver.size() == 2836 &&
                std::equal(driver.begin(), driver.begin() + 7, "AIL3DIG"),
                "original sound driver does not match supported AIL3DIG release");
        unsigned char digest[SHA256_DIGEST_LENGTH];
        SHA256(driver.data(), driver.size(), digest);
        char hex[SHA256_DIGEST_LENGTH * 2 + 1]{};
        for (size_t i = 0; i < SHA256_DIGEST_LENGTH; ++i)
            std::snprintf(hex + i * 2, 3, "%02x", digest[i]);
        require(std::string(hex) ==
                "c8a1e7a0e14c8b696e665520e338711e4c20cc3bb4733f83527add41dd6f6b38",
                "unsupported SB16.DIG checksum");

        std::vector<uint8_t> memory(0x100000);
        std::copy(driver.begin(), driver.end(), memory.begin() + 0x11000);
        set_word(memory, 0x463, 0x3d4); // BIOS CRT controller base used by delay loop.
        set_word(memory, 0x11016, 0x220);
        set_word(memory, 0x11018, 0xffff);
        set_word(memory, 0x1101a, 0xffff);
        set_word(memory, 0x1101c, 0xffff);
        const std::array<uint32_t, 256> vectors{};
        ef2000::RealModeCpu engine(ef2000::SoundMode::Dma);
        ef2000::RealRegisters regs;
        regs.ax = 0x300;
        regs.flags = 0x202;
        constexpr uint32_t ENTRY = 0x1100016a;
        auto initialized = engine.execute(ENTRY, vectors, memory, regs);
        require((initialized.registers.ax & 0xffff) == 1, "original driver initialization");
        regs.ax = 0x304;
        const auto tested = engine.execute(ENTRY, vectors, initialized.memory, regs);
        require((tested.registers.ax & 0xffff) == 0x0405,
                "original SB16.DIG IRQ/DMA self-test returned failure");
        const auto& result = tested.memory;
        require(word(result, 0x11016) == 0x220 && word(result, 0x11018) == 7 &&
                word(result, 0x1101a) == 1 && word(result, 0x1101c) == 5 &&
                word(result, 0x1157a) == 0x0405,
                "original driver failed to detect DSP 4.5 and IRQ7/DMA1/5 resources");
        require(word(result, 0x1157c) == 1, "original driver's IRQ handler did not run");
        require(tested.audio_blocks.size() == 1 &&
                tested.audio_blocks[0].pcm == std::vector<uint8_t>({0x80, 0x80}),
                "original driver did not produce its two-byte DMA self-test PCM");
        std::cout << "original-sb16: original 2836-byte driver passed DSP 4.5, "
                     "DMA1/5, IRQ7 self-test and 2-byte guest PCM capture\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "original-sb16: " << error.what() << '\n';
        return 1;
    }
}
