// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

#include "call_capture.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::filesystem::path path(argv[1]);
    assert(!std::filesystem::exists(path));
    ef2000::CapturedCall sample;
    sample.original_image_sha.fill(0x31);
    sample.address = 0x100;
    sample.sequence = 2;
    sample.registers[7] = 0x2000;
    sample.registers[8] = 0x100;
    sample.guest_ram.resize(0x3000, 0xab);
    ef2000::write_call_capture(path, sample);
    std::ifstream input(path, std::ios::binary);
    const std::vector<char> bytes{std::istreambuf_iterator<char>(input),
                                  std::istreambuf_iterator<char>()};
    assert(bytes.size() == 108 + 0x3000);
    assert(std::string(bytes.begin(), bytes.begin() + 8) == "EFCAP001");
    assert(static_cast<unsigned char>(bytes[8]) == 0x31);
    assert(static_cast<unsigned char>(bytes[40]) == 0);
    assert(static_cast<unsigned char>(bytes[41]) == 1);
    assert(static_cast<unsigned char>(bytes[108]) == 0xab);
    bool refused = false;
    try { ef2000::write_call_capture(path, sample); }
    catch (const std::runtime_error&) { refused = true; }
    assert(refused);
    input.close();
    std::filesystem::remove(path);
}
