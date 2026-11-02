// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../runtime/bios_disk.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>

int main() {
    // The virtual DOS drive does not provide BIOS sector access. Even the
    // usual discovery/reset calls must not imply that a BIOS disk exists.
    for (const std::uint8_t function : {0x00, 0x01, 0x02, 0x03, 0x04,
                                        0x08, 0x15, 0x41, 0x42, 0x43}) {
        const std::uint32_t eax = 0x1234005au | (std::uint32_t(function) << 8);
        const auto result = ef2000::absent_bios_disk(eax, 0x3202);
        assert(result.ax == 0x1234015a); // AH=01, AL and high half unchanged
        assert(result.flags == 0x3203);  // only CF set
    }
    // CF was already set, including caller flags unrelated to the BIOS call.
    const auto already_failed = ef2000::absent_bios_disk(0xfeed4f00, 0x8d5);
    assert(already_failed.ax == 0xfeed0100);
    assert(already_failed.flags == 0x8d5);
    std::cout << "bios-disk: missing sector device fails safely (CF=1 AH=01)\n";
}
