// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

#include "../runtime/isa_audio.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
void require(bool predicate, const char* message) {
    if (!predicate) throw std::runtime_error(message);
}

template<class Function> void expect_error(const char* match, Function function) {
    try { function(); }
    catch (const std::runtime_error& error) {
        require(std::string(error.what()).find(match) != std::string::npos,
                "unexpected ISA audio failure");
        return;
    }
    throw std::runtime_error("expected ISA audio failure was not raised");
}
} // namespace

int main() {
    try {
        ef2000::AudioPIC pic;
        require(pic.read(0x21) == 0xf8 && pic.read(0xa1) == 0xff,
                "initial 8259 masks");
        pic.request(7);
        require(!pic.next_irq(), "masked IRQ7 must not trigger");
        pic.write(0x21, 0x78);
        require(pic.next_irq() == 7, "unmasked IRQ7 should trigger");
        pic.begin(7);
        pic.write(0x20, 0x0b);
        require(pic.read(0x20) == 0x80 && !pic.next_irq(), "IRQ7 in-service state");
        pic.write(0x20, 0x20);
        require(pic.read(0x20) == 0 && !pic.next_irq(), "non-specific EOI");
        pic.write(0x20, 0x0a);
        pic.request(7);
        require(pic.read(0x20) == 0x80 && pic.next_irq() == 7,
                "IRR selection and another IRQ7");
        pic.clear(7);
        require(pic.read(0x20) == 0, "clearing a stale pending IRQ");
        expect_error("unsupported audio PIC", [&] { pic.write(0x20, 0xe8); });

        ef2000::DMA8237 dma;
        require(ef2000::DMA8237::handles(0x8b) &&
                ef2000::DMA8237::handles(0xd8) &&
                !ef2000::DMA8237::handles(0xd7), "DMA port map");
        dma.write(0xd8, 0);       // Clear word-channel flip-flop.
        dma.write(0xc4, 0x20);    // Channel 5 word address 0020h.
        dma.write(0xc4, 0);
        dma.write(0xc6, 3);       // Four 16-bit DMA units.
        dma.write(0xc6, 0);
        dma.write(0x8b, 3);       // 16-bit channel discards page bit zero.
        dma.write(0xd6, 0x59);    // Channel 5 auto-initialize, memory-to-device.
        dma.write(0xd4, 1);       // Unmask channel 5.
        std::vector<std::pair<uint32_t, size_t>> reads;
        auto memory = [&](uint32_t address, size_t length) {
            reads.emplace_back(address, length);
            std::vector<uint8_t> bytes(length);
            for (size_t i = 0; i < length; ++i) bytes[i] = uint8_t(i);
            return bytes;
        };
        require(dma.transfer(5, 2, memory) == std::vector<uint8_t>({0, 1, 2, 3}) &&
                dma.count(5) == 1, "half-block 16-bit DMA transfer");
        require(dma.transfer(5, 2, memory) == std::vector<uint8_t>({0, 1, 2, 3}) &&
                dma.count(5) == 3 && dma.address(5) == 0x20,
                "16-bit DMA base reload after terminal count");
        require(reads == std::vector<std::pair<uint32_t, size_t>>({{0x20040, 4},
                                                                    {0x20044, 4}}),
                "16-bit DMA address and page calculation");

        // Program 8-bit channel 1; its count and address are measured in bytes.
        dma.write(0x0c, 0);
        dma.write(0x02, 0xff);
        dma.write(0x02, 0xff);
        dma.write(0x03, 1);
        dma.write(0x03, 0);
        dma.write(0x83, 1);
        dma.write(0x0b, 0x49);
        dma.write(0x0a, 1);
        reads.clear();
        require(dma.transfer(1, 2, memory) == std::vector<uint8_t>({0, 0}) &&
                dma.masked(1), "8-bit DMA terminal count and masking");
        require(reads == std::vector<std::pair<uint32_t, size_t>>({{0x1ffff, 1}, {0x10000, 1}}),
                "8-bit DMA crosses 64K physical page correctly");
        expect_error("masked", [&] { dma.transfer(1, 1, memory); });
        expect_error("unsupported DMA read", [&] { dma.read(0x0b); });

        std::cout << "isa-audio: 8259 masks/EOI and 8237 byte/word DMA passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "isa-audio test: " << error.what() << '\n';
        return 1;
    }
}
