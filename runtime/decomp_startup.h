/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#ifndef EF2000_DECOMP_STARTUP_H
#define EF2000_DECOMP_STARTUP_H

#include <stddef.h>
#include <stdint.h>

typedef struct ef_decomp_arena_probe {
    uint32_t base;
    uint32_t size;
    uint32_t release_hook;
    uint32_t hook_registrations;
} ef_decomp_arena_probe;

/* First recovered game-owned startup call (0xe29a0), in isolation. The
 * subset of X-32 services required by this call is implemented in the host.
 * This does not initialize Watcom libc, game state or any video device. */
int ef_decomp_probe_arena(uint32_t bss_end, ef_decomp_arena_probe *probe,
                          char *reason, size_t reason_capacity);

/* Run recovered game main (0x8b274), its arena call and the first TACTCOM
 * memory diagnostic, using synthetic X-32 heap accounting.  The normal
 * path stops before 0x9c980 (filesystem path setup), with a low-memory
 * boundary at 0x8b417.  The optional playvideo branch stops at 0x8b2d8.
 * Never executes any original x86 or the full game main loop. */
int ef_decomp_probe_main(uint32_t bss_end, uint32_t argc, uint32_t argv,
                         ef_decomp_arena_probe *probe, uint32_t *stop_address,
                         char *reason, size_t reason_capacity);

#endif
