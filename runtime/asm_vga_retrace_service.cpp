// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
#include "asm_vga_retrace_service.hpp"
#include "asm_port_request.hpp"

extern "C" unsigned ef_asm_vga_retrace_service(ef_lift_state* guest, uint32_t ip,
                                               unsigned request, void* context)
{
    EfAsmPortRequest decoded{};
    if (!context || !ef_asm_decode_port_request(guest, ip, request, decoded) ||
        decoded.output || decoded.port != 0x3dau) return 0x122u;
    auto& configured = *static_cast<EfAsmVgaRetraceContext*>(context);
    if (!configured.sample_interval) return 0x122u;
    const uint32_t value = configured.retrace.read(configured.sample_interval, [&] {
        return configured.clock ? configured.clock(configured.clock_context) :
                                  configured.retrace.next_guest_time_ns();
    });
    const uint32_t mask = decoded.width == 32 ? 0xffffffffu : (1u << decoded.width) - 1u;
    guest->r[0] = (guest->r[0] & ~mask) | (value & mask);
    return 0;
}
