// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
#pragma once

#include <cstdint>

namespace ef2000 {

struct BiosDiskResult {
    std::uint32_t ax;
    std::uint32_t flags;
};

// The Unicorn port exposes DOS files, not any BIOS-addressable sector device.
// Emulate the error return from INT 13h for a drive absent from the BIOS,
// rather than aborting the entire game or accessing host block devices.
// This never claims successful read/write/geometry/EDD access.
// AH=01h (invalid command/parameter), CF=1; other flags, high AX and AL
// remain untouched. A future real disk-image implementation can replace it.
[[nodiscard]] constexpr BiosDiskResult absent_bios_disk(std::uint32_t ax,
                                                       std::uint32_t flags) noexcept {
    return {(ax & 0xffff00ffu) | 0x00000100u, flags | 1u};
}

} // namespace ef2000
