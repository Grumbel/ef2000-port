// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

#pragma once

// EFCAP001: an opt-in, uncompressed function-entry RAM snapshot. It contains
// original EF2000 code/data and MUST remain in the ignored local/ directory.
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ef2000 {
struct CapturedCall {
    std::array<uint8_t, 32> original_image_sha{};
    uint32_t address{};
    uint32_t sequence{};
    // EAX, EBX, ECX, EDX, ESI, EDI, EBP, ESP, EIP, EFLAGS, DS, ES, SS.
    std::array<uint32_t, 13> registers{};
    std::vector<uint8_t> guest_ram;
};

inline void write_u32(std::ofstream& stream, uint32_t value) {
    const char bytes[4] = {char(value), char(value >> 8),
                           char(value >> 16), char(value >> 24)};
    stream.write(bytes, sizeof(bytes));
}

inline void write_call_capture(const std::filesystem::path& path,
                               const CapturedCall& capture) {
    if (capture.guest_ram.empty() || capture.guest_ram.size() > 0x4000000 ||
        capture.guest_ram.size() % 0x1000 ||
        capture.address >= capture.guest_ram.size() ||
        capture.registers[8] != capture.address ||
        capture.registers[7] > capture.guest_ram.size() - 4)
        throw std::runtime_error("invalid EFCAP001 guest snapshot");
    // Never silently overwrite previous proprietary snapshots on repeated runs.
    if (std::filesystem::exists(path))
        throw std::runtime_error("EFCAP001 capture already exists: " + path.string());
    std::ofstream stream(path, std::ios::binary | std::ios::out);
    if (!stream)
        throw std::runtime_error("cannot create EFCAP001 capture: " + path.string());
    stream.write("EFCAP001", 8);
    stream.write(reinterpret_cast<const char*>(capture.original_image_sha.data()), 32);
    write_u32(stream, capture.address);
    write_u32(stream, capture.sequence);
    write_u32(stream, static_cast<uint32_t>(capture.guest_ram.size()));
    write_u32(stream, static_cast<uint32_t>(capture.registers.size()));
    for (uint32_t value : capture.registers) write_u32(stream, value);
    stream.write(reinterpret_cast<const char*>(capture.guest_ram.data()),
                 static_cast<std::streamsize>(capture.guest_ram.size()));
    stream.close();
    if (!stream) throw std::runtime_error("failed to write EFCAP001: " + path.string());
}
}  // namespace ef2000
