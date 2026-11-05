// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

// Opt-in original vs GCC32 tests for startup-reached lowercase and strcpy.

#include <unicorn/unicorn.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kLower = 0xe7835;
constexpr uint32_t kCopy = 0x9d749;
constexpr uint32_t kSource = 0x210000;
constexpr uint32_t kTarget = 0x220000;
constexpr uint32_t kStack = 0x3f0000;
constexpr uint32_t kReturn = 0x3f1000;
constexpr size_t kPayloadSize = 1662896;
constexpr std::array<int, 8> kRegisters{
    UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP};

struct Output {
    std::array<uint32_t, 8> registers{};
    uint32_t pc{};
    std::array<uint8_t, 128> source{};
    std::array<uint8_t, 128> target{};
    unsigned illegal_writes{};
};

void check(uc_err error, const char* step)
{
    if (error != UC_ERR_OK)
        throw std::runtime_error(std::string(step) + ": " + uc_strerror(error));
}

std::vector<uint8_t> read_file(const char* path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error(std::string("cannot open ") + path);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void validate_patches(const std::vector<uint8_t>& original,
                      const std::vector<uint8_t>& candidate)
{
    if (original.size() != kPayloadSize || candidate.size() != kPayloadSize)
        throw std::runtime_error("unexpected original payload length");
    bool lower = false;
    bool copy = false;
    for (size_t i = 0; i < original.size(); ++i) {
        if (original[i] == candidate[i]) continue;
        if (i >= kLower && i < kLower + 14) lower = true;
        else if (i >= kCopy && i < kCopy + 31) copy = true;
        else throw std::runtime_error("candidate changes code outside tested leaves");
    }
    if (!lower || !copy) throw std::runtime_error("candidate leaves a function unmodified");
}

void watch_write(uc_engine*, uc_mem_type, uint64_t address,
                 int size, int64_t, void* context)
{
    auto* output = static_cast<Output*>(context);
    const uint64_t end = address + static_cast<unsigned>(size);
    if (address >= kStack - 0x100 && end <= kStack + 4) return;
    if (address >= kTarget && end <= kTarget + output->target.size()) return;
    ++output->illegal_writes;
}

Output execute(const std::vector<uint8_t>& image, uint32_t entry,
               const std::array<uint32_t, 8>& input,
               const std::array<uint8_t, 128>& source,
               const std::array<uint8_t, 128>& target)
{
    uc_engine* cpu = nullptr;
    check(uc_open(UC_ARCH_X86, UC_MODE_32, &cpu), "open");
    try {
        check(uc_mem_map(cpu, 0, 0x400000, UC_PROT_ALL), "map");
        check(uc_mem_write(cpu, 0, image.data(), image.size()), "code");
        check(uc_mem_write(cpu, kSource, source.data(), source.size()), "source");
        check(uc_mem_write(cpu, kTarget, target.data(), target.size()), "target");
        check(uc_mem_write(cpu, kStack, &kReturn, sizeof(kReturn)), "stack");
        for (size_t i = 0; i < kRegisters.size(); ++i)
            check(uc_reg_write(cpu, kRegisters[i], &input[i]), "registers");
        uint32_t flags = 0x202;
        check(uc_reg_write(cpu, UC_X86_REG_EFLAGS, &flags), "flags");
        Output result;
        uc_hook hook{};
        check(uc_hook_add(cpu, &hook, UC_HOOK_MEM_WRITE,
                          reinterpret_cast<void*>(watch_write), &result, 1, 0), "write watch");
        check(uc_emu_start(cpu, entry, kReturn, 0, 512), "execute");
        for (size_t i = 0; i < kRegisters.size(); ++i)
            check(uc_reg_read(cpu, kRegisters[i], &result.registers[i]), "read registers");
        check(uc_reg_read(cpu, UC_X86_REG_EIP, &result.pc), "read PC");
        check(uc_mem_read(cpu, kSource, result.source.data(), result.source.size()),
              "read source");
        check(uc_mem_read(cpu, kTarget, result.target.data(), result.target.size()),
              "read target");
        check(uc_close(cpu), "close");
        return result;
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
        validate_patches(original, candidate);
        std::mt19937 rng(0x9d749);
        constexpr unsigned cases = 512;
        unsigned scratch_edx_differences = 0;
        for (unsigned index = 0; index < cases; ++index) {
            std::array<uint8_t, 128> source{};
            std::array<uint8_t, 128> target{};
            for (auto& value : source) value = static_cast<uint8_t>(rng());
            for (auto& value : target) value = static_cast<uint8_t>(rng());
            const size_t length = index % 96;
            source[length] = 0;
            std::array<uint32_t, 8> input{
                static_cast<uint32_t>(rng()), 0x12345678u, 0x23456789u,
                static_cast<uint32_t>(rng()), 0x3456789au, 0x456789abu,
                0x56789abcu, kStack};

            // All signed int32 inputs, including negative and values around
            // alphabetic boundaries, should yield exactly the same full EAX.
            const auto old_lower = execute(original, kLower, input, source, target);
            const auto new_lower = execute(candidate, kLower, input, source, target);
            if (old_lower.registers != new_lower.registers ||
                old_lower.target != new_lower.target ||
                old_lower.source != new_lower.source ||
                old_lower.illegal_writes || new_lower.illegal_writes ||
                old_lower.pc != kReturn || new_lower.pc != kReturn)
                throw std::runtime_error("lowercase case " + std::to_string(index) +
                                         " differs");

            input[0] = kTarget;
            input[3] = kSource;
            const auto old_copy = execute(original, kCopy, input, source, target);
            const auto new_copy = execute(candidate, kCopy, input, source, target);
            if (old_copy.target != new_copy.target || old_copy.source != new_copy.source ||
                old_copy.illegal_writes || new_copy.illegal_writes ||
                old_copy.pc != kReturn || new_copy.pc != kReturn ||
                old_copy.registers[7] != kStack + 4 ||
                new_copy.registers[7] != kStack + 4)
                throw std::runtime_error("copy case " + std::to_string(index) +
                                         " memory/stack diverged");
            for (size_t i = 0; i < old_copy.registers.size(); ++i) {
                if (i == 3) continue; // Original strcpy-like copy advances EDX.
                if (old_copy.registers[i] != new_copy.registers[i])
                    throw std::runtime_error("copy case " + std::to_string(index) +
                                             " preserved register diverged");
            }
            if (old_copy.registers[3] != new_copy.registers[3])
                ++scratch_edx_differences;
        }
        std::printf("startup leaves: %u/%u lowercase and %u/%u strcpy cases pass; "
                    "%u non-contract EDX differences for strcpy\n",
                    cases, cases, cases, cases, scratch_edx_differences);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "guest startup leaves: %s\n", error.what());
        return 1;
    }
}
