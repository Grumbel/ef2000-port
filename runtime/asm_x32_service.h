/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */
#ifndef EF2000_ASM_X32_SERVICE_H
#define EF2000_ASM_X32_SERVICE_H
#include "ef_lift_registry.h"
#include "x32.h"

/* Native callback for the no-swap X-32 heap-growth service at IP 0x245.
 * Context is a caller-owned x32_allocation initialized for this guest.
 * 0x122 means unavailable/exhausted; it is not a fabricated DOS response.
 * This header requires the ignored generated registry include directory. */
unsigned ef_asm_x32_service(ef_lift_state *, uint32_t, unsigned, void *);
#endif
