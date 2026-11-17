// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
#pragma once
#include "ef_lift_state.h"
#include "vga_retrace.hpp"

struct EfAsmVgaRetraceContext {
    ef2000::VgaRetrace retrace;
    unsigned sample_interval = 1;
    // Null selects the hybrid's deterministic guest clock. Otherwise return
    // host nanoseconds with the same epoch on every call. Called on samples only.
    int64_t (*clock)(void*) = nullptr;
    void* clock_context = nullptr;
};
// Scalar IN from 3da only. Unknown requests or a zero sampling interval reject
// with 0x122 unchanged. Caller controls slice boundaries via retrace.start_slice().
extern "C" unsigned ef_asm_vga_retrace_service(ef_lift_state*, uint32_t ip,
                                               unsigned request, void* context);
