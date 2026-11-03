// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

#include "../runtime/sb16_probe.hpp"

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

template <class Function> void expect_failure(const char* phrase, Function fn) {
    try { fn(); }
    catch (const std::runtime_error& error) {
        require(std::string(error.what()).find(phrase) != std::string::npos,
                "wrong SB16 probe failure");
        return;
    }
    throw std::runtime_error("missing SB16 probe failure");
}
}

int main() {
    try {
        ef2000::SB16Probe dsp;
        expect_failure("before reset", [&] { dsp.write(0x22c, 0xe1); });
        dsp.write(0x226, 1);
        require(dsp.read(0x22e) == 0x7f, "reset write buffer status");
        dsp.write(0x226, 0);
        require(dsp.read(0x22e) == 0xff && dsp.read(0x22a) == 0xaa,
                "DSP reset response");
        require(dsp.read(0x22c) == 0x7f, "write buffer availability");
        dsp.write(0x22c, 0xe1);
        require(dsp.read(0x22a) == 4 && dsp.read(0x22a) == 5,
                "original 4.5 DSP version");
        for (const auto& [index, expected] :
                std::array<std::pair<uint8_t, uint8_t>, 3>{{{0x80, 0x04},
                                                             {0x81, 0x22}, {0x82, 0x20}}}) {
            dsp.write(0x224, index);
            require(dsp.read(0x225) == expected, "DOSBox-X reference mixer resource");
        }
        require(dsp.read(0x22e) == 0x7f, "empty DSP data status");
        expect_failure("no DMA/audio", [&] { dsp.write(0x22c, 0x41); });
        dsp.write(0x226, 1);
        dsp.write(0x226, 0);
        require(dsp.read(0x22a) == 0xaa && dsp.read(0x22a) == 0xff,
                "reset clears stale responses");
        dsp.write(0x224, 0x00);
        expect_failure("unsupported SB16 probe IN", [&] { dsp.read(0x225); });
        std::cout << "sb16: measured reset/version/resource profile passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "sb16 probe test: " << error.what() << '\n';
        return 1;
    }
}
