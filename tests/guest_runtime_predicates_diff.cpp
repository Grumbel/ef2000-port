// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

// Compare seven independently GCC32-compiled recovered C functions against
// the untouched EF2000 guest routines. Opt-in: requires private original and
// locally patched image arguments. This is not Watcom/OMF or campaign replay.

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

constexpr uint32_t kStack = 0x3fff000;
constexpr uint32_t kReturn = 0x3ffe000;
constexpr uint32_t kSource = 0x280000;
constexpr uint32_t kDestination = 0x281000;
constexpr size_t kPayloadSize = 1662896;
constexpr std::array<int, 8> kRegisters{
    UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP};

enum class Kind { Clock, Command, Clear, Reset, Advance, Count, Sample,
                  EventByte, DialogReset };

struct Function {
    const char* name;
    uint32_t entry;
    uint32_t size;
    Kind kind;
    unsigned cases;
};

constexpr std::array<Function, 9> kFunctions{{
    {"reference_clock_reaches", 0x40820, 78, Kind::Clock, 96},
    {"command_code_is_group_1b", 0x41da4, 79, Kind::Command, 256},
    {"clear_tactical_accumulators", 0x49054, 61, Kind::Clear, 40},
    {"reset_tactical_slots", 0x4d8f4, 42, Kind::Reset, 40},
    {"advance_first_tactical_status", 0x4dc98, 61, Kind::Advance, 80},
    {"count_external_grid_markers", 0x544c0, 53, Kind::Count, 40},
    {"sample_active_coordinates", 0x5f1ec, 52, Kind::Sample, 80},
    {"read_event_slot_byte", 0x14d3c, 26, Kind::EventByte, 64},
    {"reset_dialog_latches", 0x151a0, 36, Kind::DialogReset, 40},
}};

struct Span {
    uint32_t address;
    std::vector<uint8_t> bytes;
};

struct Case {
    uint32_t eax{};
    uint32_t edx{};
    std::vector<Span> spans;
};

struct Output {
    std::array<uint32_t, 8> registers{};
    uint32_t pc{};
    std::vector<Span> spans;
    unsigned unexpected_writes{};
};

void require(uc_err err, const char* operation)
{
    if (err != UC_ERR_OK) {
        throw std::runtime_error(std::string(operation) + ": " + uc_strerror(err));
    }
}

std::vector<uint8_t> read_file(const char* path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error(std::string("cannot open ") + path);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void validate_patch_regions(const std::vector<uint8_t>& original,
                            const std::vector<uint8_t>& candidate)
{
    if (original.size() != kPayloadSize || candidate.size() != kPayloadSize)
        throw std::runtime_error("incorrect private payload size");
    std::array<bool, kFunctions.size()> changed{};
    for (size_t offset = 0; offset < kPayloadSize; ++offset) {
        if (original[offset] == candidate[offset]) continue;
        bool belongs = false;
        for (size_t i = 0; i < kFunctions.size(); ++i) {
            const auto& function = kFunctions[i];
            if (offset >= function.entry && offset < function.entry + function.size) {
                changed[i] = belongs = true;
            }
        }
        if (!belongs) throw std::runtime_error("patched image modifies another function");
    }
    for (size_t i = 0; i < changed.size(); ++i) {
        if (!changed[i]) throw std::runtime_error(std::string("unmodified ") + kFunctions[i].name);
    }
}

void add_span(Case& test, uint32_t address, size_t size, std::mt19937& rng)
{
    Span span{address, std::vector<uint8_t>(size)};
    for (auto& b : span.bytes) b = static_cast<uint8_t>(rng());
    test.spans.push_back(std::move(span));
}

Span& find_span(Case& test, uint32_t address)
{
    for (auto& span : test.spans) {
        if (span.address == address) return span;
    }
    throw std::runtime_error("missing test span");
}

void write_le16(uint8_t* buffer, int16_t value)
{
    const auto bits = static_cast<uint16_t>(value);
    buffer[0] = static_cast<uint8_t>(bits);
    buffer[1] = static_cast<uint8_t>(bits >> 8);
}

void write_le32(uint8_t* buffer, uint32_t value)
{
    for (unsigned b = 0; b < 4; ++b) buffer[b] = static_cast<uint8_t>(value >> (8 * b));
}

Case make_case(Kind kind, unsigned index)
{
    std::mt19937 rng(0xef2000u + index * 47u + static_cast<unsigned>(kind) * 10007u);
    Case test;
    test.eax = 0xa5390000u | (rng() & 0xffffu);
    test.edx = 0xb8430000u | (rng() & 0xffffu);
    switch (kind) {
    case Kind::Clock: {
        test.eax = kSource;
        add_span(test, kSource, 10, rng);
        add_span(test, 0x12583c, 6, rng);
        const std::array<int16_t, 6> values{{0, 1, -1, 32767, -32768, 127}};
        for (unsigned word = 0; word < 3; ++word) {
            const int16_t a = index < 30 ? values[(index + word) % values.size()]
                                         : static_cast<int16_t>(rng());
            const int16_t b = index % 3 == 0 ? a : index % 3 == 1
                ? static_cast<int16_t>(a + 1) : static_cast<int16_t>(a - 1);
            write_le16(&find_span(test, 0x12583c).bytes[word * 2], a);
            write_le16(&find_span(test, kSource).bytes[word * 2], b);
        }
        break;
    }
    case Kind::Command:
        test.eax = (rng() & 0xffffff00u) | index;
        break;
    case Kind::Clear:
        add_span(test, 0x1a523c, 20, rng); // three counters, with guard bytes
        add_span(test, 0x1a5278, 10, rng); // two words + one byte, guarded
        add_span(test, 0x127b1c, 12, rng); // runtime count, guarded
        break;
    case Kind::Reset:
        add_span(test, 0x1a5540, 24, rng); // slots, guarded
        add_span(test, 0x1a552e, 9, rng); // reset mode, guarded
        add_span(test, 0x1a5594, 9, rng); // low flag, guarded
        find_span(test, 0x1a552e).bytes[4] = index % 2 == 0 ? 0 : 1;
        break;
    case Kind::Advance: {
        add_span(test, 0x1a5670, 30, rng); // 25 entries and guards
        add_span(test, 0x1a5704, 9, rng); // changed flag and guards
        auto& bytes = find_span(test, 0x1a5670).bytes;
        for (unsigned i = 1; i < 25; ++i) bytes[i + 2] = 7;
        if (index % 5 != 0) bytes[2 + (index % 4 == 0 ? 24 : (index % 4 == 1 ? 1 : 12))] = 8;
        if (index % 5 == 4) bytes[2 + 22] = 8; // multiple matches, first must win
        break;
    }
    case Kind::Count: {
        add_span(test, 0x125838, 8, rng);
        add_span(test, 0x127da0, 9, rng);
        add_span(test, 0x1a5c2c, 8 * 27 + 4, rng);
        const uint8_t count = static_cast<uint8_t>(index % 9);
        find_span(test, 0x125838).bytes[2] = count;
        auto& records = find_span(test, 0x1a5c2c).bytes;
        for (unsigned i = 0; i < count; ++i)
            records[2 + i * 27 + 4] = index % 3 == 0 ? 0 : (i % 3 == 0 ? 1 : 0);
        break;
    }
    case Kind::Sample: {
        test.eax = kDestination;
        add_span(test, 0x1a6b04, 12, rng); // pointer, guarded
        add_span(test, kSource, 20, rng);
        add_span(test, kDestination, 12, rng);
        write_le32(&find_span(test, 0x1a6b04).bytes[4], index % 4 == 0 ? 0 : kSource);
        if (index % 8 == 0) write_le32(&find_span(test, kSource).bytes[8], 0xffffffffu);
        if (index % 8 == 1) write_le32(&find_span(test, kSource).bytes[12], 0x80000000u);
        break;
    }
    case Kind::EventByte:
        test.eax = (rng() & 0xffff0000u) |
                   static_cast<uint16_t>(static_cast<int16_t>(index) - 32);
        add_span(test, 0x122340, 0x500, rng); // 19*AX byte table plus guards
        break;
    case Kind::DialogReset:
        add_span(test, 0x12289a, 12, rng); // three latches, guarded
        add_span(test, 0x197c4c, 9, rng); // copied latch, guarded
        add_span(test, 0x18172c, 9, rng); // original input byte, guarded
        break;
    }
    return test;
}

struct WriteContext {
    const Case* test{};
    unsigned unexpected{};
};

void observe_write(uc_engine*, uc_mem_type, uint64_t addr, int size,
                   int64_t, void* raw)
{
    auto& context = *static_cast<WriteContext*>(raw);
    const uint64_t end = addr + static_cast<unsigned>(size);
    if (addr >= kStack - 0x10000 && end <= kStack + 8) return;
    for (const auto& span : context.test->spans) {
        if (addr >= span.address && end <= span.address + span.bytes.size()) return;
    }
    ++context.unexpected;
}

Output run(const std::vector<uint8_t>& image, const Function& function, const Case& test)
{
    uc_engine* cpu = nullptr;
    require(uc_open(UC_ARCH_X86, UC_MODE_32, &cpu), "open");
    try {
        require(uc_mem_map(cpu, 0, 0x4000000, UC_PROT_ALL), "map");
        require(uc_mem_write(cpu, 0, image.data(), image.size()), "code");
        for (const auto& span : test.spans)
            require(uc_mem_write(cpu, span.address, span.bytes.data(), span.bytes.size()),
                    "fixture memory");
        require(uc_mem_write(cpu, kStack, &kReturn, 4), "sentinel");
        const std::array<uint32_t, 8> initial{{
            test.eax, 0x11223344, 0x22334455, test.edx,
            0x33445566, 0x44556677, 0x55667788, kStack}};
        for (size_t i = 0; i < kRegisters.size(); ++i)
            require(uc_reg_write(cpu, kRegisters[i], &initial[i]), "initial registers");
        uint32_t flags = 0x202;
        require(uc_reg_write(cpu, UC_X86_REG_EFLAGS, &flags), "flags");
        WriteContext writes{&test, 0};
        uc_hook hook{};
        require(uc_hook_add(cpu, &hook, UC_HOOK_MEM_WRITE,
                            reinterpret_cast<void*>(observe_write), &writes, 1, 0), "hook");
        require(uc_emu_start(cpu, function.entry, kReturn, 0, 100000), "execute");
        Output output;
        output.spans = test.spans;
        for (auto& span : output.spans)
            require(uc_mem_read(cpu, span.address, span.bytes.data(), span.bytes.size()),
                    "output memory");
        for (size_t i = 0; i < kRegisters.size(); ++i)
            require(uc_reg_read(cpu, kRegisters[i], &output.registers[i]), "output registers");
        require(uc_reg_read(cpu, UC_X86_REG_EIP, &output.pc), "output PC");
        output.unexpected_writes = writes.unexpected;
        if (output.pc != kReturn || output.registers.back() != kStack + 4 ||
            output.unexpected_writes != 0)
            throw std::runtime_error(std::string(function.name) + ": bad return/stack/write");
        require(uc_close(cpu), "close");
        return output;
    } catch (...) {
        uc_close(cpu);
        throw;
    }
}

void compare(const Output& original, const Output& candidate,
             const Function& function, unsigned index, unsigned& eax_different)
{
    auto fail = [&](const char* reason) {
        throw std::runtime_error(std::string(function.name) + " case " +
                                 std::to_string(index) + ": " + reason);
    };
    for (size_t i = 0; i < original.spans.size(); ++i) {
        if (original.spans[i].bytes != candidate.spans[i].bytes) fail("memory diverged");
    }
    for (size_t i = 1; i < original.registers.size(); ++i) {
        if (original.registers[i] != candidate.registers[i]) fail("preserved register diverged");
    }
    if (original.pc != candidate.pc) fail("return PC diverged");
    if (function.kind == Kind::Command && original.registers[0] != candidate.registers[0])
        fail("full EAX result diverged");
    if (function.kind == Kind::Clock &&
        (original.registers[0] & 0xff) != (candidate.registers[0] & 0xff))
        fail("AL result diverged");
    if (function.kind == Kind::EventByte &&
        (original.registers[0] & 0xff) != (candidate.registers[0] & 0xff))
        fail("event AL result diverged");
    if (original.registers[0] != candidate.registers[0]) ++eax_different;
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
        validate_patch_regions(original, patched);
        unsigned all_passed = 0;
        for (const auto& function : kFunctions) {
            unsigned eax_different = 0;
            for (unsigned index = 0; index < function.cases; ++index) {
                const auto test = make_case(function.kind, index);
                compare(run(original, function, test), run(patched, function, test),
                        function, index, eax_different);
            }
            all_passed += function.cases;
            std::printf("%s: %u/%u isolated cases pass, %u scratch/full EAX differences\n",
                        function.name, function.cases, function.cases, eax_different);
        }
        std::printf("%u/%u original vs GCC32 guest executions passed\n",
                    all_passed, all_passed);
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "guest runtime predicates failed: %s\n", e.what());
        return 1;
    }
}
