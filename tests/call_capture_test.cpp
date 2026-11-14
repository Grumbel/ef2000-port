// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

#include "call_capture.hpp"
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

static void require(bool value) { if (!value) throw std::runtime_error("capture format assertion failed"); }

int main(int argc, char** argv) {
    require(argc == 2);
    const std::filesystem::path path(argv[1]);
    require(!std::filesystem::exists(path));
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
    require(bytes.size() == 108 + 0x3000);
    require(std::string(bytes.begin(), bytes.begin() + 8) == "EFCAP001");
    require(static_cast<unsigned char>(bytes[8]) == 0x31);
    require(static_cast<unsigned char>(bytes[40]) == 0);
    require(static_cast<unsigned char>(bytes[41]) == 1);
    require(static_cast<unsigned char>(bytes[108]) == 0xab);
    bool refused = false;
    try { ef2000::write_call_capture(path, sample); }
    catch (const std::runtime_error&) { refused = true; }
    require(refused);
    input.close();
    std::filesystem::remove(path);
    sample.has_x87=true;
    sample.extra_registers={7,16,8};
    sample.x87[0]=0x37f; sample.x87[1]=5u<<11; sample.x87[2]=0xffff;
    sample.x87[7]=0x321;
    for (size_t i=0;i<8;++i) {
        sample.x87[8+3*i]=uint32_t(i);
        sample.x87[8+3*i+1]=0x80000000u;
        sample.x87[8+3*i+2]=uint32_t(0x3fff+i);
    }
    ef2000::write_call_capture(path,sample);
    std::ifstream v2(path,std::ios::binary);
    const std::vector<char> extended{std::istreambuf_iterator<char>(v2),std::istreambuf_iterator<char>()};
    require(extended.size()==364+0x3000);
    require(std::string(extended.begin(),extended.begin()+8)=="EFCAP002");
    require(static_cast<unsigned char>(extended[52])==16);
    require(static_cast<unsigned char>(extended[108])==7);
    require(static_cast<unsigned char>(extended[124])==32);
    require(static_cast<unsigned char>(extended[148])==0x7f);
    require(static_cast<unsigned char>(extended[364])==0xab);
    v2.close();
    std::filesystem::remove(path);
    sample.x87[10]=0x10000;
    refused=false;
    try { ef2000::write_call_capture(path,sample); } catch (const std::runtime_error&) { refused=true; }
    require(refused && !std::filesystem::exists(path));
}
