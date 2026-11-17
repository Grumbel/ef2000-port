// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
#pragma once
#include "ef_lift_state.h"
#include "vga_dac.hpp"

struct EfAsmVgaPortContext {
    ef2000::VgaDac dac;
    // Must address the same mapped palette storage used by the presentation
    // backend. The hybrid uses DOS_BASE + 0xf1000, outside its payload RAM.
    uint32_t palette_address;
};
// Handle scalar OUT requests to 3c8/3c9 only. Unknown/malformed requests or
// invalid palette spans return 0x122 without modifying state, RAM or the DAC.
extern "C" unsigned ef_asm_vga_port_service(ef_lift_state*, uint32_t ip,
                                            unsigned request, void* context);
