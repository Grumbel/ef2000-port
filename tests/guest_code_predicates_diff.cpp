// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

// Opt-in exhaustive low-byte guest differential for eight command predicates.
// Neither the original game bytes nor the diagnostic replacements are checked
// into the repository. Passing here does not count as an OMF/replay verification.

#include <unicorn/unicorn.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kStack = 0x3fff000;
constexpr uint32_t kReturn = 0x3ffe000;
constexpr size_t kPayloadSize = 1662896;
constexpr std::array<int, 8> kRegs{
    UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP};

struct Function {
    const char* name;
    uint32_t entry;
    uint32_t size;
    bool two_args;
    bool scratch_edx;
};

constexpr std::array<Function, 8> kFunctions{{
    {"code_is_10_to_15", 0xcc5fc, 44, false, false},
    {"code_is_18_to_1b_1d_1e", 0xcc628, 44, false, false},
    {"code_is_18_or_1c_to_1f", 0xcc654, 39, false, false},
    {"code_is_20_or_21", 0xcc67c, 24, false, false},
    {"code_is_23_or_24", 0xcc694, 24, false, false},
    {"code_is_22", 0xcc6ac, 17, false, false},
    {"pair_matches_cc7e8", 0xcc7e8, 52, true, false},
    {"pair_matches_cc81c", 0xcc81c, 34, true, true},
}};

void require(uc_err error, const char* operation)
{
    if (error != UC_ERR_OK)
        throw std::runtime_error(std::string(operation) + ": " + uc_strerror(error));
}

std::vector<uint8_t> read_file(const char* name)
{
    std::ifstream file(name, std::ios::binary);
    if (!file) throw std::runtime_error(std::string("cannot open ") + name);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void validate_patch_regions(const std::vector<uint8_t>& original,
                            const std::vector<uint8_t>& candidate)
{
    if (original.size() != kPayloadSize || candidate.size() != kPayloadSize)
        throw std::runtime_error("incorrect original/patched image size");
    std::array<bool, kFunctions.size()> changed{};
    for (size_t offset = 0; offset < kPayloadSize; ++offset) {
        if (original[offset] == candidate[offset]) continue;
        bool belongs = false;
        for (size_t i = 0; i < kFunctions.size(); ++i) {
            const auto& function = kFunctions[i];
            if (offset >= function.entry && offset < function.entry + function.size) {
                changed[i] = true;
                belongs = true;
            }
        }
        if (!belongs) throw std::runtime_error("patched image modifies another function");
    }
    for (size_t i = 0; i < changed.size(); ++i)
        if (!changed[i]) throw std::runtime_error(std::string("unmodified ") + kFunctions[i].name);
}

struct RegisterResult {
    std::array<uint32_t, kRegs.size()> regs{};
    uint32_t pc{};
    unsigned writes{};
};

void write_watch(uc_engine*, uc_mem_type, uint64_t addr, int size, int64_t, void* data)
{
    if (addr < kStack - 0x10000 || addr + static_cast<unsigned>(size) > kStack + 8)
        ++*static_cast<unsigned*>(data);
}

RegisterResult execute(const std::vector<uint8_t>& image, const Function& function,
                       uint8_t code, uint8_t kind, unsigned salt)
{
    uc_engine* cpu = nullptr;
    require(uc_open(UC_ARCH_X86, UC_MODE_32, &cpu), "open");
    try {
        require(uc_mem_map(cpu, 0, 0x4000000, UC_PROT_ALL), "map");
        require(uc_mem_write(cpu, 0, image.data(), image.size()), "code");
        require(uc_mem_write(cpu, kStack, &kReturn, sizeof(kReturn)), "sentinel");
        const std::array<uint32_t, kRegs.size()> initial{{
            0x67ab0000u + (salt << 8) + code,
            0x11223344u, 0x22334455u,
            0x45810000u + (salt << 8) + kind,
            0x33445566u, 0x44556677u, 0x55667788u, kStack}};
        for (size_t i = 0; i < kRegs.size(); ++i)
            require(uc_reg_write(cpu, kRegs[i], &initial[i]), "register input");
        uint32_t flags = 0x202;
        require(uc_reg_write(cpu, UC_X86_REG_EFLAGS, &flags), "flags");
        RegisterResult result;
        uc_hook hook{};
        require(uc_hook_add(cpu, &hook, UC_HOOK_MEM_WRITE,
                            reinterpret_cast<void*>(write_watch), &result.writes, 1, 0), "write hook");
        require(uc_emu_start(cpu, function.entry, kReturn, 0, 10000), "execute");
        for (size_t i = 0; i < kRegs.size(); ++i)
            require(uc_reg_read(cpu, kRegs[i], &result.regs[i]), "register output");
        require(uc_reg_read(cpu, UC_X86_REG_EIP, &result.pc), "return address");
        if (result.pc != kReturn || result.regs.back() != kStack + 4 || result.writes != 0)
            throw std::runtime_error(std::string(function.name) + ": bad return/write");
        require(uc_close(cpu), "close");
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
        std::fprintf(stderr, "usage: %s ORIGINAL_PAYLOAD CODE_PREDICATES_ONLY_PATCHED_PAYLOAD\n", argv[0]);
        return 2;
    }
    try {
        const auto original = read_file(argv[1]);
        const auto candidate = read_file(argv[2]);
        validate_patch_regions(original, candidate);
        constexpr std::array<uint8_t, 16> kKinds{{
            0, 1, 2, 7, 8, 9, 0x0d, 0x0e, 0x10, 0x11, 0x13, 0x15,
            0x16, 0x17, 0x1a, 0xff}};
        unsigned total = 0;
        for (const auto& function : kFunctions) {
            unsigned cases = 0;
            unsigned scratch_differences = 0;
            for (unsigned code = 0; code < 256; ++code) {
                const unsigned limit = function.two_args ? kKinds.size() : 1;
                for (unsigned index = 0; index < limit; ++index) {
                    const uint8_t kind = function.two_args ? kKinds[index] : 0;
                    const uint32_t salt = code % 13;
                    const auto reference = execute(original, function, code, kind, salt);
                    const auto patched = execute(candidate, function, code, kind, salt);
                    for (size_t reg = 0; reg < reference.regs.size(); ++reg) {
                        if (function.scratch_edx && reg == 3) {
                            // cc81c destructively ANDs EDX with 255 and on the
                            // selected-kind branch loads the original AL into DL.
                            const uint32_t expected = kind == 8 || kind == 0x1a ? code : kind;
                            if (reference.regs[3] != expected)
                                throw std::runtime_error("cc81c scratch EDX model mismatch");
                            if (reference.regs[3] != patched.regs[3]) ++scratch_differences;
                        } else if (reference.regs[reg] != patched.regs[reg]) {
                            throw std::runtime_error(std::string(function.name) +
                                                     " diverged at code " + std::to_string(code) +
                                                     " kind " + std::to_string(kind) +
                                                     " register " + std::to_string(reg));
                        }
                    }
                    if (reference.pc != patched.pc)
                        throw std::runtime_error("return PC diverged");
                    ++cases;
                }
            }
            total += cases;
            std::printf("%s: %u/%u ABI cases pass, %u scratch EDX differences\n",
                        function.name, cases, cases, scratch_differences);
        }
        std::printf("%u/%u command-predicate guest differential cases passed\n", total, total);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "guest code predicate differential failed: %s\n", error.what());
        return 1;
    }
}
