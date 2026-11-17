// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
#include "asm_vga_port_service.hpp"
#include "asm_vga_retrace_service.hpp"
#include <array>
#include <cassert>
#include <cstring>

int main()
{
    std::array<uint8_t, 4096> ram{};
    ef_lift_state state{};
    state.memory = ram.data(); state.memory_size = ram.size();
    state.flags = 0xad7;
    for (unsigned i = 0; i < 8; ++i) state.r[i] = 0xaabb0000u + i;
    EfAsmVgaPortContext context{{}, 0x800};
    auto send = [&](uint16_t port, uint32_t value, unsigned width) {
        state.r[0] = value; state.r[2] = 0xbeef0000u | port;
        ram[0x100] = width == 16 ? 0x66 : width == 8 ? 0xee : 0xef;
        ram[0x101] = 0xef;
        const unsigned length = width == 16 ? 2 : 1;
        state.pc = 0x100 + length;
        const auto before = state;
        assert(ef_asm_vga_port_service(&state, 0x100,
               0x1000 | (length << 8) | (width == 8 ? 0xee : 0xef), &context) == 0);
        assert(std::memcmp(&state, &before, sizeof state) == 0);
    };
    // All palette entries, six-bit masking, word/dword forms, and wrap to zero.
    send(0x3c8, 0xdead00ff, 8);
    auto expected = ram;
    unsigned cursor = 765;
    for (unsigned i = 0; i < 780; ++i) {
        const unsigned width = i % 3 == 0 ? 8 : i % 3 == 1 ? 16 : 32;
        send(0x3c9, 0xface0000u | i, width);
        expected[0x100] = ram[0x100]; expected[0x101] = ram[0x101];
        expected[0x800 + cursor] = i & 63;
        cursor = (cursor + 1) % 768;
        assert(ram == expected);
        assert(context.dac.cursor() == cursor);
    }
    // Cursor selection uses only AL, independent of OUT's accumulator width.
    for (unsigned width : {8u, 16u, 32u}) {
        send(0x3c8, 0xffff0102, width);
        assert(context.dac.cursor() == 6);
    }
    auto reject = [&](unsigned request, uint32_t ip, void* configured) {
        auto before = state;
        const auto bytes = ram;
        const auto cursor_before = context.dac.cursor();
        assert(ef_asm_vga_port_service(&state, ip, request, configured) == 0x122);
        assert(std::memcmp(&state, &before, sizeof state) == 0);
        assert(ram == bytes);
        assert(context.dac.cursor() == cursor_before);
    };
    state.pc = 0x101; ram[0x100] = 0xee; state.r[2] = 0x3c9;
    reject(0x11ee, 0x100, nullptr);
    reject(0x21, 0x100, &context);
    reject(0x11ec, 0x100, &context); // IN has no implementation here.
    reject(0x12ee, 0x100, &context); // Incorrect encoded length.
    reject(0x11ef, 0x100, &context); // Opcode mismatch.
    reject(0x11ee, 0xffffffff, &context);
    state.pc = 0x102;
    reject(0x11ee, 0x100, &context); // Wrong continuation.
    state.pc = 0x101; state.r[2] = 0x201;
    reject(0x11ee, 0x100, &context);
    state.r[2] = 0x3c9; context.palette_address = 0xffffffff;
    reject(0x11ee, 0x100, &context);
    context.palette_address = ram.size() - 6; // Cursor 6 is just out of bounds.
    reject(0x11ee, 0x100, &context);
    // Immediate OUT ports cannot address 3c8/3c9.
    ram[0x100] = 0xe6; ram[0x101] = 0xc9; state.pc = 0x102;
    reject(0x12e6, 0x100, &context);
    // A failed backing-store callback does not advance the shared DAC.
    ef2000::VgaDac dac;
    try { dac.write(0x3c9, 1, [](uint32_t, uint8_t) { throw 1; }); }
    catch (int) {}
    assert(dac.cursor() == 0);
    // Retrace uses the exact sampled synthetic-clock policy. All IN widths
    // preserve the remaining accumulator bits and every other guest field.
    EfAsmVgaRetraceContext retrace;
    for (unsigned interval : {1u, 4u, 4096u}) {
        retrace = {};
        retrace.sample_interval = interval;
        uint64_t samples = 0;
        unsigned until = 1;
        uint32_t status = 0;
        for (unsigned i = 0; i < 1600; ++i) {
            const unsigned width = i % 3 == 0 ? 8 : i % 3 == 1 ? 16 : 32;
            ram[0x100] = width == 16 ? 0x66 : width == 8 ? 0xec : 0xed;
            ram[0x101] = 0xed;
            const unsigned length = width == 16 ? 2 : 1;
            state.pc = 0x100 + length; state.r[0] = 0xabcdef01;
            state.r[2] = 0xbeef03da;
            if (i == 777) { retrace.retrace.start_slice(); until = 1; }
            if (--until == 0) {
                until = interval;
                const int64_t ns = static_cast<int64_t>(samples++ * 100000);
                status = ns % 16666667 >= 15000000 ? 8 : 0;
            }
            auto before = state;
            const auto bytes = ram;
            assert(ef_asm_vga_retrace_service(&state, 0x100,
                   0x1000 | (length << 8) | (width == 8 ? 0xec : 0xed), &retrace) == 0);
            const uint32_t mask = width == 32 ? 0xffffffff : (1u << width) - 1;
            before.r[0] = (before.r[0] & ~mask) | status;
            assert(std::memcmp(&state, &before, sizeof state) == 0);
            assert(ram == bytes);
        }
    }
    struct Clock { int64_t ns; unsigned calls; } clock{14999999, 0};
    retrace = {};
    retrace.sample_interval = 4;
    retrace.clock_context = &clock;
    retrace.clock = [](void* data) -> int64_t {
        auto& clock = *static_cast<Clock*>(data);
        ++clock.calls; return clock.ns;
    };
    state.pc = 0x101; state.r[2] = 0x3da; ram[0x100] = 0xec;
    auto read_retrace = [&] {
        assert(ef_asm_vga_retrace_service(&state, 0x100, 0x11ec, &retrace) == 0);
        return state.r[0] & 255;
    };
    assert(read_retrace() == 0 && clock.calls == 1);
    clock.ns = 15000000;
    for (unsigned i = 0; i < 3; ++i) assert(read_retrace() == 0 && clock.calls == 1);
    assert(read_retrace() == 8 && clock.calls == 2);
    clock.ns = 16666667;
    retrace.retrace.start_slice();
    assert(read_retrace() == 0 && clock.calls == 3);
    auto reject_retrace = [&](unsigned request) {
        const auto before = state;
        const auto bytes = ram;
        assert(ef_asm_vga_retrace_service(&state, 0x100, request, &retrace) == 0x122);
        assert(std::memcmp(&state, &before, sizeof state) == 0);
        assert(ram == bytes && clock.calls == 3);
    };
    state.r[2] = 0x201; reject_retrace(0x11ec);
    state.r[2] = 0x3da; reject_retrace(0x11ee);
    retrace.sample_interval = 0; reject_retrace(0x11ec);

}
