// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
#pragma once

#include "ef_lift_state.h"

// Context is an ef2000::DosDrive*. Uses the hybrid runtime's flat-address DOS
// file policy and copy-on-write overlay. Unknown sites/requests return 0x122
// without changing guest state. Exceptions other than numeric DOS errors escape
// to the host caller, as they do in the hybrid interpreter.
extern "C" unsigned ef_asm_dos_file_service(ef_lift_state *guest, uint32_t ip,
                                           unsigned vector, void *context);
