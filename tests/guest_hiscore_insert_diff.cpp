// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

// Isolated original-vs-compiled-C insertion comparison using Unicorn x86.
// Both arguments are private, SHA-pinned EF2000 payloads. The second must
// differ by the C replacement at 0x433c; no original bytes are embedded.
// This does not substitute for the full modified-code replay requirement.

#include <unicorn/unicorn.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kEntry = 0x433c;
constexpr uint32_t kTable = 0x1970a4;
constexpr uint32_t kCurrent = 0x1970a0;
constexpr uint32_t kDifficulty = 0x121e44;
constexpr uint32_t kName = 0x250000;
constexpr uint32_t kStack = 0x3fff000;
constexpr uint32_t kReturn = 0x3ffe000;
constexpr size_t kPayloadSize = 1662896;
constexpr size_t kRowBytes = 25;
constexpr size_t kRows = 10;

void require(uc_err error, const char* operation)
{
    if (error != UC_ERR_OK) {
        throw std::runtime_error(std::string(operation) + ": " + uc_strerror(error));
    }
}

std::vector<uint8_t> read_file(const char* filename)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file) throw std::runtime_error(std::string("cannot open ") + filename);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

struct Case {
    std::string label;
    int32_t score{};
    std::array<int32_t, kRows> rankings{};
    std::string name;
};

std::vector<Case> cases()
{
    const std::array<int32_t, kRows> standard{
        1000, 900, 800, 700, 600, 500, 400, 300, 200, 100};
    std::vector<Case> tests{
        {"top", 1200, standard, "ACE"},
        {"middle", 650, standard, "MID-RANK"},
        {"last", 101, standard, "LAST"},
        {"reject", 99, standard, "NOPE"},
        {"tie", 500, standard, "EQUAL"},
        {"negative", -200, {-10, -30, -50, -80, -100, -120, -150, -200, -250, -300}, "MINUS"},
        {"all_min", std::numeric_limits<int32_t>::min(),
         {-1, -1, -1, -1, -1, -1, -1, -1, -1, std::numeric_limits<int32_t>::min()}, "MINIMUM"},
        {"min_wrap", std::numeric_limits<int32_t>::min(),
         {std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::min(),
          std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::min(),
          std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::min(),
          std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::min(),
          std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::min()}, "WRAPPED"},
        {"max", std::numeric_limits<int32_t>::max(), standard, "MAXIMUM"},
        {"long_name", 550, standard, "ABCDEFGHIJKLMNOPQRS"},
    };
    std::mt19937 rng(0xef2000);
    std::uniform_int_distribution<int32_t> score(-20000, 20000);
    for (unsigned i = 0; i < 90; ++i) {
        Case candidate;
        candidate.label = "random_" + std::to_string(i);
        candidate.score = score(rng);
        candidate.name = "TEST" + std::to_string(i);
        for (auto& value : candidate.rankings) value = score(rng);
        std::sort(candidate.rankings.begin(), candidate.rankings.end(), std::greater<int32_t>());
        tests.push_back(std::move(candidate));
    }
    return tests;
}

struct Output {
    std::array<uint32_t, 8> regs{};
    uint32_t eip{};
    std::array<uint8_t, kRows * kRowBytes> table{};
    std::array<uint8_t, 32> input{};
    int32_t score{};
    unsigned bad_writes{};
};

void observe_write(uc_engine*, uc_mem_type, uint64_t addr, int length,
                   int64_t, void* context)
{
    const uint64_t end = addr + static_cast<unsigned>(length);
    const bool table = addr >= kTable && end <= kTable + kRows * kRowBytes;
    const bool current = addr >= kCurrent && end <= kCurrent + 4;
    const bool stack = addr >= kStack - 0x10000 && end <= kStack + 8;
    if (!table && !current && !stack) ++*static_cast<unsigned*>(context);
}

Output execute(const std::vector<uint8_t>& payload, const Case& test)
{
    std::array<uint8_t, kRows * kRowBytes> table{};
    for (size_t row = 0; row < kRows; ++row) {
        for (size_t col = 0; col < kRowBytes; ++col)
            table[row * kRowBytes + col] = static_cast<uint8_t>(0x90 + row * 7 + col);
        const std::string name = "Pilot" + std::to_string(row);
        std::memcpy(table.data() + row * kRowBytes, name.c_str(), name.size() + 1);
        table[row * kRowBytes + 20] = static_cast<uint8_t>(1 + row % 3);
        std::memcpy(table.data() + row * kRowBytes + 21,
                    &test.rankings[row], sizeof(int32_t));
    }
    std::array<uint8_t, 32> name{};
    name.fill(0x5a);
    std::memcpy(name.data(), test.name.c_str(), test.name.size() + 1);
    const uint8_t difficulty = 2;
    uc_engine* cpu = nullptr;
    require(uc_open(UC_ARCH_X86, UC_MODE_32, &cpu), "open");
    try {
        require(uc_mem_map(cpu, 0, 0x4000000, UC_PROT_ALL), "map");
        require(uc_mem_write(cpu, 0, payload.data(), payload.size()), "payload");
        require(uc_mem_write(cpu, kTable, table.data(), table.size()), "table");
        require(uc_mem_write(cpu, kCurrent, &test.score, 4), "score");
        require(uc_mem_write(cpu, kName, name.data(), name.size()), "name");
        require(uc_mem_write(cpu, kDifficulty, &difficulty, 1), "difficulty");
        require(uc_mem_write(cpu, kStack, &kReturn, 4), "sentinel");
        constexpr std::array<int, 8> ids{
            UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
            UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP};
        constexpr std::array<uint32_t, 8> initial{
            kName, 0x1234aabb, 0x2345bbcc, 0x3456ccdd,
            0x4567ddee, 0x5678eeff, 0x6789ff00, kStack};
        for (size_t i = 0; i < ids.size(); ++i)
            require(uc_reg_write(cpu, ids[i], &initial[i]), "input registers");
        uint32_t flags = 0x202;
        require(uc_reg_write(cpu, UC_X86_REG_EFLAGS, &flags), "flags");
        Output out;
        uc_hook writes{};
        require(uc_hook_add(cpu, &writes, UC_HOOK_MEM_WRITE,
                            reinterpret_cast<void*>(observe_write),
                            &out.bad_writes, 1, 0), "write hook");
        require(uc_emu_start(cpu, kEntry, kReturn, 0, 100000), "execute");
        require(uc_mem_read(cpu, kTable, out.table.data(), out.table.size()), "result table");
        require(uc_mem_read(cpu, kName, out.input.data(), out.input.size()), "result name");
        require(uc_mem_read(cpu, kCurrent, &out.score, 4), "result current score");
        for (size_t i = 0; i < ids.size(); ++i)
            require(uc_reg_read(cpu, ids[i], &out.regs[i]), "result registers");
        require(uc_reg_read(cpu, UC_X86_REG_EIP, &out.eip), "return PC");
        if (out.eip != kReturn || out.regs.back() != kStack + 4 || out.bad_writes)
            throw std::runtime_error("bad return/stack or unexpected non-stack guest write");
        require(uc_close(cpu), "close");
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
        const auto patched = read_file(argv[2]);
        if (original.size() != kPayloadSize || patched.size() != original.size())
            throw std::runtime_error("incorrect original/patched payload size");
        const auto inputs = cases();
        for (const auto& test : inputs) {
            const auto reference = execute(original, test);
            const auto candidate = execute(patched, test);
            if (reference.regs != candidate.regs || reference.eip != candidate.eip ||
                reference.table != candidate.table || reference.score != candidate.score ||
                reference.input != candidate.input) {
                throw std::runtime_error(test.label + ": original/C guest divergence");
            }
            std::printf("%s: PASS slot=%d\n", test.label.c_str(),
                        static_cast<int32_t>(candidate.regs[0]));
        }
        std::printf("%zu original-vs-C guest cases passed\n", inputs.size());
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "guest high-score insertion failed: %s\n", error.what());
        return 1;
    }
}
