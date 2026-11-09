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
 * path enters the 0x9cb20 archive/disk lookup callback and (when data_dir
 * is supplied) its recovered 0xb3b90 file reader.  A missing file stops
 * at diagnostic 0xea50c; a found file stops at options 0x82cec. Without
 * a directory the legacy boundary remains at 0xb3b90. Low memory stops
 * at 0x8b417, and playvideo at 0x8b2d8.
 * Never executes any original x86 or the full game main loop. */
int ef_decomp_probe_main(uint32_t bss_end, uint32_t argc, uint32_t argv,
                         const char *data_dir,
                         ef_decomp_arena_probe *probe, uint32_t *stop_address,
                         char *reason, size_t reason_capacity);

#endif
