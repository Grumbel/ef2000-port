// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
#include "asm_dos_file_service.hpp"
#include "dos_drive.hpp"
#include <array>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iterator>

int main(int argc, char **argv)
{
    assert(argc == 2);
    const std::filesystem::path root(argv[1]);
    std::filesystem::create_directories(root / "game");
    std::filesystem::create_directories(root / "overlay");
    { std::ofstream file(root / "game/FILE.DAT"); file << "original"; }
    ef2000::DosDrive drive(root / "game", {}, root / "overlay");
    const uint16_t handle = drive.open("FILE.DAT", 2, false);
    std::array<uint8_t, 4096> ram{};
    ef_lift_state state{};
    state.memory = ram.data(); state.memory_size = ram.size();
    state.pc = 0x1103e2; state.flags = 0x246;
    for (unsigned i = 0; i < 8; ++i) state.r[i] = 0xaabb0000u + i;
    state.r[0] = 0xffff4000; state.r[1] = 3; state.r[2] = 0x300;
    state.r[3] = 0xbeef0000u | handle;
    std::memcpy(ram.data() + 0x300, "NEW", 3);
    auto before = state;
    assert(ef_asm_dos_file_service(&state, 0x1103e0, 0x21, &drive) == 0);
    before.r[0] = 3;
    assert(std::memcmp(&state, &before, sizeof state) == 0);
    assert(std::memcmp(ram.data() + 0x300, "NEW", 3) == 0);

    // Seek uses signed CX:DX, preserves high DX and ignores high BX.
    state.r[0] = 0xabcd4202; state.r[1] = 0xffffffff; state.r[2] = 0x8765ffff;
    before = state;
    assert(ef_asm_dos_file_service(&state, 0xfa973, 0x21, &drive) == 0);
    before.r[0] = 7; before.r[2] = 0x87650000;
    assert(std::memcmp(&state, &before, sizeof state) == 0);
    state.r[0] = 0x4202; state.r[1] = 0; state.r[2] = 0;
    assert(ef_asm_dos_file_service(&state, 0x1103b7, 0x21, &drive) == 0);
    assert(state.r[0] == 8);

    for (const uint16_t h : {uint16_t(0), handle, uint16_t(0xffff)}) {
        state.r[0] = 0xface4400; state.r[3] = h; state.flags = 0xad7;
        before = state;
        assert(ef_asm_dos_file_service(&state, 0x1101c7, 0x21, &drive) == 0);
        before.r[0] = h == 0 ? 0x80d3 : h == handle ? 2 : 6;
        if (h != 0xffff) before.r[2] = (before.r[2] & 0xffff0000) | before.r[0];
        before.flags = (before.flags & ~1u) | (h == 0xffff);
        assert(std::memcmp(&state, &before, sizeof state) == 0);
    }
    for (const uint32_t address : {0u, 0x260u, 4096u, 0xfffffffeu, 4095u}) {
        state.r[0] = 0x4000; state.r[1] = 3; state.r[2] = address; state.r[3] = handle;
        before = state;
        assert(ef_asm_dos_file_service(&state, 0x1103e0, 0x21, &drive) == 0);
        before.r[0] = 5; before.flags |= 1;
        assert(std::memcmp(&state, &before, sizeof state) == 0);
    }
    // Rejections leave state and file position untouched.
    state.r[0] = 0x4201; state.r[1] = state.r[2] = 0; state.r[3] = handle;
    before = state;
    assert(ef_asm_dos_file_service(&state, 0x1103e0, 0x21, &drive) == 0x122);
    assert(ef_asm_dos_file_service(&state, 0xfa973, 0x31, &drive) == 0x122);
    assert(ef_asm_dos_file_service(&state, 0xfa973, 0x21, nullptr) == 0x122);
    assert(ef_asm_dos_file_service(&state, 0x123, 0x21, &drive) == 0x122);
    assert(std::memcmp(&state, &before, sizeof state) == 0);
    assert(drive.seek(handle, 0, 1) == 8);
    state.r[0] = 0xabcd3e00; state.r[3] = handle;
    before = state;
    assert(ef_asm_dos_file_service(&state, 0xfa9a8, 0x21, &drive) == 0);
    before.r[0] = 0x3e01; before.flags &= ~1u;
    assert(std::memcmp(&state, &before, sizeof state) == 0);
    assert(!drive.is_open(handle));
    state.r[0] = 0x3e00; before = state;
    assert(ef_asm_dos_file_service(&state, 0xfa9a8, 0x21, &drive) == 0);
    before.r[0] = 6; before.flags |= 1u;
    assert(std::memcmp(&state, &before, sizeof state) == 0);
    std::ifstream overlay(root / "overlay/EF2000/FILE.DAT", std::ios::binary);
    assert(std::string(std::istreambuf_iterator<char>(overlay), {}) == "NEWginal");
    std::ifstream original(root / "game/FILE.DAT", std::ios::binary);
    assert(std::string(std::istreambuf_iterator<char>(original), {}) == "original");
}
