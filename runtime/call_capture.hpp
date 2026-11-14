// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

#pragma once

// EFCAP001/002: opt-in, uncompressed function-entry RAM snapshots. They contain
// original EF2000 code/data and MUST remain in the ignored local/ directory.
#include <array>
#include <algorithm>
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
    bool has_x87{}; // EFCAP002, explicitly requested; legacy captures stay EFCAP001.
    std::array<uint32_t, 3> extra_registers{}; // FS, GS, CS.
    uint32_t producer_version{}; // Opaque uc_version() result.
    // CW, SW, tag, IP, CS, DP, DS, opcode; then eight physical low/high/SE triples.
    std::array<uint32_t, 32> x87{};
    // GDTR then LDTR: selector, base low/high, limit, flags.
    std::array<uint32_t, 10> table_registers{};
    uint32_t descriptor_valid_mask{};
    // ES, CS, SS, DS, FS, GS: raw selected table entries, not hidden caches.
    std::array<uint32_t, 12> segment_descriptors{};
    // Addressable GDT/LDT windows (at most 64 KiB each); empty means unavailable.
    std::array<std::vector<uint8_t>, 2> table_bytes;
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
    if (capture.has_x87) {
        for (size_t i = 10; i < 13; ++i)
            if (capture.registers[i] > 0xffffu) throw std::runtime_error("invalid EFCAP002 selector");
        for (auto value : capture.extra_registers)
            if (value > 0xffffu) throw std::runtime_error("invalid EFCAP002 selector");
        for (auto i : {0, 1, 2, 4, 6})
            if (capture.x87[i] > 0xffffu) throw std::runtime_error("invalid EFCAP002 x87 environment");
        if (capture.x87[7] > 0x7ffu || capture.descriptor_valid_mask > 63u)
            throw std::runtime_error("invalid EFCAP002 opcode or descriptor mask");
        for (size_t i = 0; i < 8; ++i)
            if (capture.x87[8+3*i+2] > 0xffffu) throw std::runtime_error("invalid EFCAP002 raw exponent");
        if (capture.table_registers[0] > 0xffffu || capture.table_registers[5] > 0xffffu)
            throw std::runtime_error("invalid EFCAP002 table selector");
        for (size_t i = 0; i < 6; ++i)
            if (!(capture.descriptor_valid_mask & (1u<<i)) &&
                    (capture.segment_descriptors[2*i] || capture.segment_descriptors[2*i+1]))
                throw std::runtime_error("invalid EFCAP002 unavailable descriptor");
        for (size_t i = 0; i < 2; ++i) {
            const uint64_t expected = std::min<uint64_t>(uint64_t(capture.table_registers[5*i+3])+1, 65536);
            if (!capture.table_bytes[i].empty() && capture.table_bytes[i].size() != expected)
                throw std::runtime_error("invalid EFCAP002 table window size");
        }
        const uint32_t selectors[] = {capture.registers[11], capture.extra_registers[2],
            capture.registers[12], capture.registers[10], capture.extra_registers[0], capture.extra_registers[1]};
        for (size_t i = 0; i < 6; ++i) {
            const size_t table = (selectors[i] & 4u) ? 1 : 0;
            const size_t offset = selectors[i] & 0xfff8u;
            const bool valid = (capture.descriptor_valid_mask & (1u<<i)) != 0;
            if (valid && offset+7u > capture.table_registers[5*table+3])
                throw std::runtime_error("invalid EFCAP002 descriptor bounds");
            const auto& bytes = capture.table_bytes[table];
            if (bytes.empty()) continue;
            const bool available = offset+8u <= bytes.size();
            uint64_t raw{};
            if (available) for (size_t byte=0; byte<8; ++byte) raw |= uint64_t(bytes[offset+byte])<<(8*byte);
            const uint64_t selected = uint64_t(capture.segment_descriptors[2*i]) |
                                      uint64_t(capture.segment_descriptors[2*i+1])<<32;
            if (available != valid || raw != selected)
                throw std::runtime_error("inconsistent EFCAP002 table descriptor");
        }
    }
    // Never silently overwrite previous proprietary snapshots on repeated runs.
    if (std::filesystem::exists(path))
        throw std::runtime_error("EFCAP001 capture already exists: " + path.string());
    std::ofstream stream(path, std::ios::binary | std::ios::out);
    if (!stream)
        throw std::runtime_error("cannot create EFCAP001 capture: " + path.string());
    stream.write(capture.has_x87 ? "EFCAP002" : "EFCAP001", 8);
    stream.write(reinterpret_cast<const char*>(capture.original_image_sha.data()), 32);
    write_u32(stream, capture.address);
    write_u32(stream, capture.sequence);
    write_u32(stream, static_cast<uint32_t>(capture.guest_ram.size()));
    write_u32(stream, capture.has_x87 ? 16u : 13u);
    for (uint32_t value : capture.registers) write_u32(stream, value);
    if (capture.has_x87) {
        for (auto value : capture.extra_registers) write_u32(stream, value);
        for (auto value : {capture.producer_version, 32u, 10u, 6u, capture.descriptor_valid_mask,
                          uint32_t(capture.table_bytes[0].size()), uint32_t(capture.table_bytes[1].size())})
            write_u32(stream, value);
        for (auto value : capture.x87) write_u32(stream, value);
        for (auto value : capture.table_registers) write_u32(stream, value);
        for (auto value : capture.segment_descriptors) write_u32(stream, value);
        for (const auto& table : capture.table_bytes)
            if (!table.empty()) stream.write(reinterpret_cast<const char*>(table.data()), table.size());
    }
    stream.write(reinterpret_cast<const char*>(capture.guest_ram.data()),
                 static_cast<std::streamsize>(capture.guest_ram.size()));
    stream.close();
    if (!stream) throw std::runtime_error("failed to write EFCAP001: " + path.string());
}
}  // namespace ef2000
