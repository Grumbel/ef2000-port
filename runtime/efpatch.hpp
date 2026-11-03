// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

#pragma once

// Strict, dependency-free reader for tools/efpatch.py's in-place EFPATCH1.
// It deliberately cannot map or apply arena writes.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ef2000 {
struct PatchWrite {
    uint32_t offset{};
    std::vector<uint8_t> bytes;
};

inline uint32_t patch_u32(const std::vector<uint8_t>& blob, size_t& at) {
    if (at > blob.size() || blob.size() - at < 4)
        throw std::runtime_error("truncated EFPATCH1 integer");
    const uint32_t value = uint32_t(blob[at]) | (uint32_t(blob[at + 1]) << 8) |
                           (uint32_t(blob[at + 2]) << 16) | (uint32_t(blob[at + 3]) << 24);
    at += 4;
    return value;
}

inline void validate_patch_writes(const std::vector<PatchWrite>& writes,
                                  size_t original_size) {
    if (writes.empty() || writes.size() > 512)
        throw std::runtime_error("EFPATCH1 must contain 1..512 writes");
    size_t total = 0;
    std::vector<std::pair<size_t, size_t>> intervals;
    for (const auto& write : writes) {
        if (write.offset >= original_size || write.bytes.empty() ||
            write.bytes.size() > original_size - write.offset)
            throw std::runtime_error("EFPATCH1 write is outside initialized payload");
        total += write.bytes.size();
        if (total > 16u * 1024u * 1024u)
            throw std::runtime_error("EFPATCH1 exceeds 16 MiB limit");
        intervals.emplace_back(write.offset, size_t(write.offset) + write.bytes.size());
    }
    std::sort(intervals.begin(), intervals.end());
    for (size_t i = 1; i < intervals.size(); ++i)
        if (intervals[i].first < intervals[i - 1].second)
            throw std::runtime_error("overlapping EFPATCH1 writes");
}

inline std::vector<PatchWrite> decode_efpatch(
    const std::vector<uint8_t>& blob, const std::array<uint8_t, 32>& source_sha,
    size_t original_size) {
    constexpr char magic[] = "EF2000P1";
    if (blob.size() < 44 || blob.size() > 16u * 1024u * 1024u + 8192u ||
        std::memcmp(blob.data(), magic, 8))
        throw std::runtime_error("invalid EFPATCH1 magic or size");
    if (!std::equal(source_sha.begin(), source_sha.end(), blob.begin() + 8))
        throw std::runtime_error("EFPATCH1 belongs to a different original payload");
    size_t cursor = 40;
    const auto count = patch_u32(blob, cursor);
    if (count == 0 || count > 512)
        throw std::runtime_error("invalid EFPATCH1 write count");
    std::vector<PatchWrite> writes;
    writes.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t offset = patch_u32(blob, cursor);
        const uint32_t size = patch_u32(blob, cursor);
        if (size > blob.size() - cursor)
            throw std::runtime_error("truncated EFPATCH1 data");
        writes.push_back({offset, {blob.begin() + cursor, blob.begin() + cursor + size}});
        cursor += size;
    }
    if (cursor != blob.size())
        throw std::runtime_error("trailing bytes in EFPATCH1");
    validate_patch_writes(writes, original_size);
    return writes;
}

inline std::vector<PatchWrite> load_efpatch(
    const std::string& path, const std::array<uint8_t, 32>& source_sha,
    size_t original_size) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("cannot open EFPATCH1: " + path);
    const auto length = file.tellg();
    if (length < 44 || length > 16 * 1024 * 1024 + 8192)
        throw std::runtime_error("invalid EFPATCH1 file size: " + path);
    file.seekg(0);
    std::vector<uint8_t> blob(static_cast<size_t>(length));
    if (!file.read(reinterpret_cast<char*>(blob.data()), length))
        throw std::runtime_error("cannot read EFPATCH1: " + path);
    return decode_efpatch(blob, source_sha, original_size);
}

inline void apply_efpatches(std::vector<uint8_t>& guest,
                            const std::vector<PatchWrite>& writes) {
    // The caller must validate all batches together *before* mutation.
    for (const auto& write : writes)
        std::copy(write.bytes.begin(), write.bytes.end(), guest.begin() + write.offset);
}
}  // namespace ef2000
