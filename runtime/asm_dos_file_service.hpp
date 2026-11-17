// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
#pragma once

#include "ef_lift_state.h"

// Context is an ef2000::DosDrive*. Uses the hybrid runtime's flat-address DOS
// file policy and copy-on-write overlay. Unknown sites/requests return 0x122
// without changing guest state. The generic intdos site (0x2f0) accepts only
// these implemented file services; other DOS/extender requests still reject.
// Exceptions other than numeric DOS errors escape
// to the host caller, as they do in the hybrid interpreter.
extern "C" unsigned ef_asm_dos_file_service(ef_lift_state *guest, uint32_t ip,
                                           unsigned vector, void *context);

namespace ef2000 { class DosDrive; }
struct EfAsmDosFileContext {
    ef2000::DosDrive *drive;
    bool joystick = false, digital_sound = false, cd_music = false;
};
// Configured variant accepts EfAsmDosFileContext*, including the options used
// when DosDrive virtualizes configuration files. The original entry defaults
// all three options to false and retains its DosDrive* context ABI.
extern "C" unsigned ef_asm_dos_file_service_configured(ef_lift_state *guest,
    uint32_t ip, unsigned vector, void *context);
