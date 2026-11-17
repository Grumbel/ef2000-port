// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
#include "asm_vga_port_service.hpp"

extern "C" unsigned ef_asm_vga_port_service(ef_lift_state* guest, uint32_t ip,
                                            unsigned request, void* context)
{
    if (!guest || !context || !guest->memory || (request & ~0xfffu) != 0x1000u)
        return 0x122u;
    const unsigned length = (request >> 8) & 15u;
    const unsigned opcode = request & 255u;
    if (opcode != 0xe6u && opcode != 0xe7u && opcode != 0xeeu && opcode != 0xefu)
        return 0x122u;
    if (!length || ip >= guest->memory_size || length > guest->memory_size - ip)
        return 0x122u;
    const uint8_t* bytes = guest->memory + ip;
    const bool word = bytes[0] == 0x66u;
    const bool immediate = opcode < 0xecu;
    if (length != unsigned(word) + (immediate ? 2u : 1u) ||
        (word && !(opcode & 1u)) || bytes[unsigned(word)] != opcode ||
        guest->pc != ip + length) return 0x122u;
    const uint16_t port = immediate ? bytes[length - 1] : guest->r[2] & 0xffffu;
    if (port != 0x3c8u && port != 0x3c9u) return 0x122u;
    auto& configured = *static_cast<EfAsmVgaPortContext*>(context);
    const uint32_t palette = configured.palette_address;
    if (port == 0x3c9u && (palette >= guest->memory_size ||
        configured.dac.cursor() >= guest->memory_size - palette)) return 0x122u;
    configured.dac.write(port, guest->r[0], [&](uint32_t index, uint8_t value) {
        guest->memory[palette + index] = value;
    });
    return 0;
}
