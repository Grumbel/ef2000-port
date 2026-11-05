// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

// Private-input opt-in comparison: original vs GCC32 C at 0x604.
// This is an isolated CPU/ABI test, not an OpenWatcom or gameplay verification.

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

constexpr uint32_t kEntry = 0x604;
constexpr uint32_t kSize = 36;
constexpr uint32_t kBase = 0x120cc6;
constexpr uint32_t kStack = 0x3f0000;
constexpr uint32_t kReturn = 0x3f1000;
constexpr size_t kPayloadSize = 1662896;
constexpr std::array<int, 8> kRegisters{
    UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP};

struct Output {
    std::array<uint32_t, 8> registers{};
    uint32_t pc{};
    uint32_t global{};
    uint32_t flags{};
    unsigned foreign_writes{};
};

void check(uc_err error, const char* label)
{
    if (error != UC_ERR_OK)
        throw std::runtime_error(std::string(label) + ": " + uc_strerror(error));
}

std::vector<uint8_t> read_file(const char* filename)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file) throw std::runtime_error(std::string("cannot open ") + filename);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void validate_images(const std::vector<uint8_t>& original,
                     const std::vector<uint8_t>& changed)
{
    if (original.size() != kPayloadSize || changed.size() != kPayloadSize)
        throw std::runtime_error("incorrect private payload size");
    bool found = false;
    for (size_t i = 0; i < original.size(); ++i) {
        if (original[i] == changed[i]) continue;
        if (i < kEntry || i >= kEntry + kSize)
            throw std::runtime_error("candidate changes code outside 0x604..0x627");
        found = true;
    }
    if (!found) throw std::runtime_error("candidate does not replace the function");
}

void note_write(uc_engine*, uc_mem_type, uint64_t address,
                int size, int64_t, void* opaque)
{
    auto* output = static_cast<Output*>(opaque);
    if (address < kStack - 0x100 || address + static_cast<unsigned>(size) > kStack + 4)
        ++output->foreign_writes;
}

Output execute(const std::vector<uint8_t>& image,
               uint32_t pointer, uint32_t base, uint32_t flags)
{
    uc_engine* cpu = nullptr;
    check(uc_open(UC_ARCH_X86, UC_MODE_32, &cpu), "open");
    try {
        check(uc_mem_map(cpu, 0, 0x400000, UC_PROT_ALL), "map");
        check(uc_mem_write(cpu, 0, image.data(), image.size()), "payload");
        check(uc_mem_write(cpu, kBase, &base, sizeof(base)), "base");
        check(uc_mem_write(cpu, kStack, &kReturn, sizeof(kReturn)), "return address");
        const std::array<uint32_t, 8> init{
            pointer, 0x11223344u, 0x22334455u, 0x33445566u,
            0x44556677u, 0x55667788u, 0x66778899u, kStack};
        for (size_t i = 0; i < kRegisters.size(); ++i)
            check(uc_reg_write(cpu, kRegisters[i], &init[i]), "register init");
        check(uc_reg_write(cpu, UC_X86_REG_EFLAGS, &flags), "flags init");
        Output out;
        uc_hook hook{};
        check(uc_hook_add(cpu, &hook, UC_HOOK_MEM_WRITE,
                          reinterpret_cast<void*>(note_write), &out, 1, 0), "write watch");
        check(uc_emu_start(cpu, kEntry, kReturn, 0, 100), "execute");
        for (size_t i = 0; i < kRegisters.size(); ++i)
            check(uc_reg_read(cpu, kRegisters[i], &out.registers[i]), "register read");
        check(uc_reg_read(cpu, UC_X86_REG_EIP, &out.pc), "PC");
        check(uc_reg_read(cpu, UC_X86_REG_EFLAGS, &out.flags), "flags");
        check(uc_mem_read(cpu, kBase, &out.global, sizeof(out.global)), "global");
        check(uc_close(cpu), "close");
        return out;
    } catch (...) {
        uc_close(cpu);
        throw;
    }
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::fprintf(stderr, "usage: %s ORIGINAL_PAYLOAD PATCHED_PAYLOAD\n", argv[0]);
        return 2;
    }
    try {
        const auto original = read_file(argv[1]);
        const auto candidate = read_file(argv[2]);
        validate_images(original, candidate);
        std::mt19937 rng(0xef2000);
        unsigned different_flags = 0;
        constexpr unsigned cases = 1024;
        for (unsigned i = 0; i < cases; ++i) {
            const uint32_t base = i < 16 ? 0x20000u : rng();
            const uint32_t pointer = i < 16 ? base + i : rng();
            const uint32_t flags = 0x202u | ((i & 1u) << 0u);
            const Output old = execute(original, pointer, base, flags);
            const Output replacement = execute(candidate, pointer, base, flags);
            if (old.registers != replacement.registers || old.pc != replacement.pc ||
                old.global != replacement.global || old.foreign_writes != 0 ||
                replacement.foreign_writes != 0 || old.pc != kReturn ||
                old.registers[7] != kStack + 4) {
                throw std::runtime_error("0x604 case " + std::to_string(i) +
                                         ": registers, memory, or return diverged");
            }
            if (old.flags != replacement.flags) ++different_flags;
        }
        std::printf("encode_runtime_segment_offset: %u/%u register/memory cases pass, "
                    "%u EFLAGS differences (not asserted by ABI)\n",
                    cases, cases, different_flags);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "guest segment differential: %s\n", error.what());
        return 1;
    }
}
