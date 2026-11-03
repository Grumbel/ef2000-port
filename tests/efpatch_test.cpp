// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

#include "efpatch.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

static void append_u32(std::vector<uint8_t>& out, uint32_t value) {
    for (int i = 0; i < 4; ++i) out.push_back(uint8_t(value >> (i * 8)));
}

static bool fails(const std::vector<uint8_t>& blob, const std::array<uint8_t, 32>& sha) {
    try { (void)ef2000::decode_efpatch(blob, sha, 128); }
    catch (const std::runtime_error&) { return true; }
    return false;
}

static void expect(bool okay) {
    if (!okay) throw std::runtime_error("EFPATCH1 C++ wire-format test failed");
}

int main() {
    const std::array<uint8_t, 32> sha = {};
    std::vector<uint8_t> blob = {'E', 'F', '2', '0', '0', '0', 'P', '1'};
    blob.insert(blob.end(), sha.begin(), sha.end());
    append_u32(blob, 1);
    append_u32(blob, 16);
    append_u32(blob, 2);
    blob.push_back(0xcc);
    blob.push_back(0x90);
    const auto writes = ef2000::decode_efpatch(blob, sha, 128);
    expect(writes.size() == 1 && writes[0].offset == 16);
    expect(writes[0].bytes == std::vector<uint8_t>({0xcc, 0x90}));
    std::vector<uint8_t> image(128, 0);
    ef2000::apply_efpatches(image, writes);
    expect(image[16] == 0xcc && image[17] == 0x90 && image[18] == 0);

    auto bad = blob;
    bad[8] = 1;  // incorrect source payload identity
    expect(fails(bad, sha));
    bad = blob;
    bad.pop_back();
    expect(fails(bad, sha));
    bad = blob;
    bad[44] = 127;  // write starts outside image after two bytes
    expect(fails(bad, sha));
    bad = blob;
    append_u32(bad, 30);
    expect(fails(bad, sha));
    const std::vector<ef2000::PatchWrite> overlap = {{16, {1, 2}}, {17, {3}}};
    bool rejected = false;
    try { ef2000::validate_patch_writes(overlap, 128); }
    catch (const std::runtime_error&) { rejected = true; }
    expect(rejected);
}
