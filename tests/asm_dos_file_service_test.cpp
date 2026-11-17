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
    // Actual read RAM effects, short EOF, and no position change on bad spans.
    drive.seek(handle, 0, 0);
    state.r[0] = 0xabcd3f00; state.r[1] = 20; state.r[2] = 0x400; state.r[3] = handle;
    ram.fill(0xa5); before = state;
    assert(ef_asm_dos_file_service(&state, 0x10f0ef, 0x21, &drive) == 0);
    before.r[0] = 8; before.r[1] = before.r[2] = 0; before.flags &= ~1u;
    assert(std::memcmp(&state, &before, sizeof state) == 0);
    assert(std::memcmp(ram.data() + 0x400, "NEWginal", 8) == 0);
    for (unsigned i = 0; i < ram.size(); ++i)
        if (i < 0x400 || i >= 0x408) assert(ram[i] == 0xa5);
    state.r[0] = 0x3f00; state.r[1] = 1; state.r[2] = 0x400;
    assert(ef_asm_dos_file_service(&state, 0x10f0ef, 0x21, &drive) == 0);
    assert(state.r[0] == 0 && ram[0x400] == 'N');
    state.r[0] = 0x3f00; state.r[1] = 0xffffffff; state.r[2] = 0x400;
    before = state;
    assert(ef_asm_dos_file_service(&state, 0x10f0ef, 0x21, &drive) == 0);
    before.r[0] = 5; before.flags |= 1u;
    assert(std::memcmp(&state, &before, sizeof state) == 0);
    assert(drive.seek(handle, 0, 1) == 8);
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
    for (const uint32_t ip : {0xfa4dbu, 0xfa5dfu}) {
        std::memcpy(ram.data() + 0x300, "FILE.DAT", 9);
        state.r[0] = 0xabcd3d00; state.r[2] = 0x300; state.flags |= 1u;
        before = state;
        assert(ef_asm_dos_file_service(&state, ip, 0x21, &drive) == 0);
        const auto opened = state.r[0];
        before.r[0] = opened; before.r[2] = 0; before.flags &= ~1u;
        assert(std::memcmp(&state, &before, sizeof state) == 0);
        assert(drive.is_open(opened)); drive.close(opened);
    }
    for (const uint32_t ip : {0xfa518u, 0xfa553u, 0xfa5c4u}) {
        std::memcpy(ram.data() + 0x300, "CREATED.DAT", 12);
        state.r[0] = 0x3c00; state.r[2] = 0x300; before = state;
        assert(ef_asm_dos_file_service(&state, 0xfa5b0, 0x21, &drive) == 0);
        const auto opened = state.r[0]; before.r[0] = opened; before.flags &= ~1u;
        assert(std::memcmp(&state, &before, sizeof state) == 0);
        state.r[0] = 0x4000; state.r[1] = 0; state.r[2] = 0x300; state.r[3] = opened;
        assert(ef_asm_dos_file_service(&state, 0xfa541, 0x21, &drive) == 0);
        assert(state.r[0] == 0);
        state.r[0] = 0x3e00;
        assert(ef_asm_dos_file_service(&state, ip, 0x21, &drive) == 0);
        assert(!drive.is_open(opened));
    }
    // The generic intdos site has the same file policy, but cannot accept
    // arbitrary requests merely because they arrived through this gateway.
    std::memcpy(ram.data() + 0x300, "GATEWAY.DAT", 12);
    state.r[0] = 0x3c00; state.r[2] = 0x300;
    assert(ef_asm_dos_file_service(&state, 0x2f0, 0x21, &drive) == 0);
    const auto gateway_handle = state.r[0];
    state.r[0] = 0x4000; state.r[1] = 3; state.r[2] = 0x400;
    state.r[3] = gateway_handle;
    std::memcpy(ram.data() + 0x400, "DOS", 3);
    assert(ef_asm_dos_file_service(&state, 0x2f0, 0x21, &drive) == 0);
    assert(state.r[0] == 3);
    state.r[0] = 0x4200; state.r[1] = state.r[2] = 0;
    assert(ef_asm_dos_file_service(&state, 0x2f0, 0x21, &drive) == 0);
    assert(state.r[0] == 0);
    state.r[0] = 0x3f00; state.r[1] = 3; state.r[2] = 0x500;
    assert(ef_asm_dos_file_service(&state, 0x2f0, 0x21, &drive) == 0);
    assert(state.r[0] == 3 && std::memcmp(ram.data() + 0x500, "DOS", 3) == 0);
    state.r[0] = 0x4400;
    assert(ef_asm_dos_file_service(&state, 0x2f0, 0x21, &drive) == 0);
    assert(state.r[0] == 2);
    for (const unsigned request : {0x252bu, 0x3508u, 0x4401u, 0x6900u}) {
        state.r[0] = request; before = state;
        const auto memory_before = ram;
        assert(ef_asm_dos_file_service(&state, 0x2f0, 0x21, &drive) == 0x122);
        assert(std::memcmp(&state, &before, sizeof state) == 0);
        assert(ram == memory_before);
        assert(drive.seek(gateway_handle, 0, 1) == 3);
    }
    // Flush publishes the overlay without closing or moving the handle.
    // Check console handles, invalid handles, high BX bits and both CF inputs.
    for (const uint16_t h : {uint16_t(0), uint16_t(1), uint16_t(2),
                             uint16_t(gateway_handle), uint16_t(0xffff)}) {
        for (const uint32_t flags : {0xad6u, 0xad7u}) {
            state.r[0] = 0xabcd68ff; state.r[3] = 0xbeef0000u | h;
            state.flags = flags; before = state;
            const auto memory_before = ram;
            assert(ef_asm_dos_file_service(&state, 0x2f0, 0x21, &drive) == 0);
            before.flags = (flags & ~1u) | (h == 0xffff);
            if (h == 0xffff) before.r[0] = 6;
            assert(std::memcmp(&state, &before, sizeof state) == 0);
            assert(ram == memory_before);
            assert(drive.is_open(gateway_handle));
            assert(drive.seek(gateway_handle, 0, 1) == 3);
        }
    }
    {
        std::ifstream flushed(root / "overlay/EF2000/GATEWAY.DAT", std::ios::binary);
        assert(std::string(std::istreambuf_iterator<char>(flushed), {}) == "DOS");
    }
    // AH=68 is accepted only at the generic gateway.
    state.r[0] = 0x6800; before = state;
    assert(ef_asm_dos_file_service(&state, 0xfa9a8, 0x21, &drive) == 0x122);
    assert(std::memcmp(&state, &before, sizeof state) == 0);
    state.r[3] = gateway_handle;
    state.r[0] = 0x3e00;
    assert(ef_asm_dos_file_service(&state, 0x2f0, 0x21, &drive) == 0);
    assert(!drive.is_open(gateway_handle));
    std::memcpy(ram.data() + 0x300, "GATEWAY.DAT", 12);
    state.r[0] = 0x3d00; state.r[2] = 0x300;
    assert(ef_asm_dos_file_service(&state, 0x2f0, 0x21, &drive) == 0);
    assert(drive.read(state.r[0], 3) == std::vector<uint8_t>({'D', 'O', 'S'}));
    drive.close(state.r[0]);
    ram.fill('X'); state.r[0] = 0x3d00; state.r[2] = 0x300; before = state;
    assert(ef_asm_dos_file_service(&state, 0xfa4db, 0x21, &drive) == 0);
    before.r[0] = 3; before.flags |= 1u;
    assert(std::memcmp(&state, &before, sizeof state) == 0);
    { std::ofstream file(root / "game/EF2000.CFG"); file << "CONTROL=0\nMUSIC=0,127\n"; }
    std::memcpy(ram.data() + 0x300, "EF2000.CFG", 11);
    EfAsmDosFileContext configured{&drive, true, true, true};
    state.r[0] = 0x3d00; state.r[2] = 0x300;
    assert(ef_asm_dos_file_service_configured(&state, 0xfa4db, 0x21, &configured) == 0);
    const auto config = state.r[0]; const auto bytes = drive.read(config, 1024);
    const std::string text(bytes.begin(), bytes.end());
    assert(text.find("CONTROL=1") != std::string::npos);
    assert(text.find("MUSIC=2,127") != std::string::npos);
    drive.close(config);
}
