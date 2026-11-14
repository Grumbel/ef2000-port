// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
#include "call_capture_unicorn.hpp"
#include <cstring>
#include <string>

#define REQUIRE(condition) do { if (!(condition)) throw std::runtime_error(#condition); } while (false)

int main(int argc, char** argv) {
    REQUIRE(argc == 2 || argc == 4);
    uc_engine* cpu{};
    REQUIRE(uc_open(UC_ARCH_X86, UC_MODE_32, &cpu) == UC_ERR_OK);
    auto write = [cpu](int id, const void* value) { REQUIRE(uc_reg_write(cpu, id, value) == UC_ERR_OK); };
    constexpr uint64_t base = 0x10000000;
    REQUIRE(uc_mem_map(cpu, base, 0x1000, UC_PROT_ALL) == UC_ERR_OK);
    const uint64_t code = 0x00cf9a000000ffffULL, data = 0x00cf92000000ffffULL;
    const uint64_t local = 0x0040f2345000ffffULL;
    REQUIRE(uc_mem_write(cpu, base+8, &code, 8) == UC_ERR_OK);
    REQUIRE(uc_mem_write(cpu, base+16, &data, 8) == UC_ERR_OK);
    REQUIRE(uc_mem_write(cpu, base+0x100, &local, 8) == UC_ERR_OK);
    uc_x86_mmr gdtr{0,base,23,0}, ldtr{24,base+0x100,7,0x82};
    write(UC_X86_REG_GDTR,&gdtr); write(UC_X86_REG_LDTR,&ldtr);
    uint32_t selector = 8;
    write(UC_X86_REG_CS,&selector);
    selector = 16;
    for (auto id : {UC_X86_REG_DS,UC_X86_REG_ES,UC_X86_REG_SS,UC_X86_REG_GS}) write(id,&selector);
    selector = 7; write(UC_X86_REG_FS,&selector);
    // Loaded data segments remain cached; the selected table entry is now
    // unavailable. Captures must distinguish table context from hidden caches.
    gdtr.limit = 15; write(UC_X86_REG_GDTR,&gdtr);
    struct Raw { uint64_t mantissa; uint16_t exponent; };
    for (int i=0;i<8;++i) {
        Raw raw{0x8000000000000000ULL+uint64_t(i),uint16_t(0x3fff+i)};
        write(UC_X86_REG_FP0+i,&raw);
    }
    uint64_t control=0x37f,status=5u<<11,tag=0xffff,ip=0x123456,cs=8,dp=0x234567,ds=16,opcode=0x321;
    const int ids[] = {UC_X86_REG_FPCW,UC_X86_REG_FPSW,UC_X86_REG_FPTAG,UC_X86_REG_FIP,
                      UC_X86_REG_FCS,UC_X86_REG_FDP,UC_X86_REG_FDS,UC_X86_REG_FOP};
    const uint64_t values[] = {control,status,tag,ip,cs,dp,ds,opcode};
    for (size_t i=0;i<8;++i) write(ids[i],&values[i]);
    ef2000::CapturedCall capture;
    capture.address=0x100; capture.sequence=3;
    capture.registers={11,22,33,44,55,66,77,0x2000,0x100,0x202,16,16,16};
    capture.guest_ram.resize(0x3000,0xab);
    capture.original_image_sha.fill(0x31);
    if (argc==4) {
        REQUIRE(std::string(argv[2])=="--keep");
        const std::string hash=argv[3]; REQUIRE(hash.size()==64);
        for (size_t i=0;i<32;++i) capture.original_image_sha[i]=uint8_t(std::stoul(hash.substr(2*i,2),nullptr,16));
    }
    ef2000::capture_x87_context(cpu,capture);
    REQUIRE(capture.has_x87 && capture.extra_registers[0]==7 && capture.extra_registers[1]==16 && capture.extra_registers[2]==8);
    REQUIRE(capture.descriptor_valid_mask==0x12);
    REQUIRE(capture.table_bytes[0].size()==16 && capture.table_bytes[1].size()==8);
    REQUIRE(capture.table_registers[1]==base && capture.table_registers[3]==15);
    REQUIRE(capture.segment_descriptors[2]==uint32_t(code));
    REQUIRE(capture.segment_descriptors[3]==uint32_t((code | (1ULL<<40))>>32));
    REQUIRE(capture.segment_descriptors[8]==uint32_t(local));
    REQUIRE(capture.segment_descriptors[9]==uint32_t((local | (1ULL<<40))>>32));
    for (size_t i=0;i<8;++i) REQUIRE(capture.x87[i]==values[i]);
    for (size_t i=0;i<8;++i) {
        REQUIRE(capture.x87[8+3*i]==i);
        REQUIRE(capture.x87[8+3*i+1]==0x80000000u);
        REQUIRE(capture.x87[8+3*i+2]==0x3fff+i);
    }
    // ST0 is logical TOP, unlike the captured physical FP0..FP7 payloads.
    uint8_t st[16]{}; REQUIRE(uc_reg_read(cpu,UC_X86_REG_ST0,st)==UC_ERR_OK);
    uint64_t significand{}; std::memcpy(&significand,st,8);
    REQUIRE(significand==0x8000000000000005ULL);
    ef2000::write_call_capture(argv[1],capture);
    REQUIRE(std::filesystem::file_size(argv[1])==364+24+0x3000);
    bool refused=false;
    try { ef2000::write_call_capture(argv[1],capture); } catch (const std::runtime_error&) { refused=true; }
    REQUIRE(refused);
    if (argc==2) std::filesystem::remove(argv[1]);
    REQUIRE(uc_close(cpu)==UC_ERR_OK);
}
