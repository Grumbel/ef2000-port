// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
#include "asm_vga_port_service.hpp"
#include "asm_port_request.hpp"

extern "C" unsigned ef_asm_vga_port_service(ef_lift_state* guest, uint32_t ip,
                                            unsigned request, void* context)
{
    EfAsmPortRequest decoded{};
    if (!context || !ef_asm_decode_port_request(guest, ip, request, decoded) ||
        !decoded.output || (decoded.port != 0x3c8u && decoded.port != 0x3c9u)) return 0x122u;
    const uint16_t port = decoded.port;
    auto& configured = *static_cast<EfAsmVgaPortContext*>(context);
    const uint32_t palette = configured.palette_address;
    if (port == 0x3c9u && (palette >= guest->memory_size ||
        configured.dac.cursor() >= guest->memory_size - palette)) return 0x122u;
    configured.dac.write(port, guest->r[0], [&](uint32_t index, uint8_t value) {
        guest->memory[palette + index] = value;
    });
    return 0;
}
