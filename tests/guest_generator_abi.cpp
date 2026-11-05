// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

// Original-only, opt-in guest execution to confirm the 0xa45b3 generator
// state update and the two scratch half-register outputs (AX and DX).

#include <unicorn/unicorn.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kEntry = 0xa45b3;
constexpr uint32_t kSeed = 0xa45a4;
constexpr uint32_t kStack = 0x3f0000;
constexpr uint32_t kReturn = 0x3f1000;
constexpr size_t kImageSize = 1662896;
constexpr std::array<int, 8> kRegisters{
    UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP};

void check(uc_err error, const char* label)
{
    if (error != UC_ERR_OK)
        throw std::runtime_error(std::string(label) + ": " + uc_strerror(error));
}

struct Observation {
    std::array<uint32_t, 8> registers{};
    uint32_t seed{};
    uint32_t pc{};
};

Observation execute(const std::vector<uint8_t>& image, uint32_t seed,
                    const std::array<uint32_t, 8>& initial)
{
    uc_engine* cpu = nullptr;
    check(uc_open(UC_ARCH_X86, UC_MODE_32, &cpu), "open");
    try {
        check(uc_mem_map(cpu, 0, 0x400000, UC_PROT_ALL), "map");
        check(uc_mem_write(cpu, 0, image.data(), image.size()), "image");
        check(uc_mem_write(cpu, kSeed, &seed, sizeof(seed)), "seed");
        check(uc_mem_write(cpu, kStack, &kReturn, sizeof(kReturn)), "return address");
        for (size_t i = 0; i < kRegisters.size(); ++i)
            check(uc_reg_write(cpu, kRegisters[i], &initial[i]), "register init");
        uint32_t flags = 0x202;
        check(uc_reg_write(cpu, UC_X86_REG_EFLAGS, &flags), "flags init");
        check(uc_emu_start(cpu, kEntry, kReturn, 0, 100), "execute");
        Observation observed;
        for (size_t i = 0; i < kRegisters.size(); ++i)
            check(uc_reg_read(cpu, kRegisters[i], &observed.registers[i]), "register read");
        check(uc_reg_read(cpu, UC_X86_REG_EIP, &observed.pc), "PC");
        check(uc_mem_read(cpu, kSeed, &observed.seed, sizeof(observed.seed)), "seed read");
        check(uc_close(cpu), "close");
        return observed;
    } catch (...) {
        uc_close(cpu);
        throw;
    }
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s ORIGINAL_PAYLOAD\n", argv[0]);
        return 2;
    }
    try {
        std::ifstream file(argv[1], std::ios::binary);
        if (!file) throw std::runtime_error("cannot open private payload");
        const std::vector<uint8_t> image{std::istreambuf_iterator<char>(file),
                                         std::istreambuf_iterator<char>()};
        if (image.size() != kImageSize)
            throw std::runtime_error("expected original 1,662,896-byte payload");
        std::mt19937 rng(0x31415926);
        constexpr unsigned cases = 512;
        for (unsigned index = 0; index < cases; ++index) {
            uint32_t seed = rng();
            if (index == 0) seed = 0;
            if (index == 1) seed = 0xffff0001;
            if (index == 2) seed = 0xffffffff;
            const std::array<uint32_t, 8> input{
                static_cast<uint32_t>(rng()), static_cast<uint32_t>(rng()),
                static_cast<uint32_t>(rng()), static_cast<uint32_t>(rng()),
                static_cast<uint32_t>(rng()), static_cast<uint32_t>(rng()),
                static_cast<uint32_t>(rng()), kStack};
            const Observation output = execute(image, seed, input);

            const uint32_t lo = seed >> 16;
            const uint32_t hi = seed & 0xffffu;
            const uint32_t sum = lo + hi;
            const uint32_t new_lo = sum & 0xffffu;
            const uint32_t new_hi = (hi + new_lo + (sum >> 16)) & 0xffffu;
            uint32_t expected = new_hi | (new_lo << 16);
            if ((new_hi | new_lo) == 0) expected = 0x31415926u;
            const uint32_t expected_eax = (input[0] & 0xffff0000u) | new_lo;
            const uint32_t expected_edx = (input[3] & 0xffff0000u) | (new_hi | new_lo);
            if (output.seed != expected || output.registers[0] != expected_eax ||
                output.registers[3] != expected_edx || output.pc != kReturn ||
                output.registers[7] != kStack + 4) {
                throw std::runtime_error("generator state/scratch registers diverged in case " +
                                         std::to_string(index));
            }
            for (size_t i : {1u, 2u, 4u, 5u, 6u}) {
                if (output.registers[i] != input[i])
                    throw std::runtime_error("generator unexpectedly clobbered preserved register");
            }
        }
        std::printf("step_generator_a45b3: %u/%u original guest state and ABI cases pass\n",
                    cases, cases);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "guest generator ABI: %s\n", error.what());
        return 1;
    }
}
