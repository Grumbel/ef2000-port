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
 * is supplied) its recovered 0xb3b90 file reader. A missing file enters
 * its read-only logger adapter and stops at the 0xeb25a page switch; a
 * found file enters recovered 0x82cec options, samples a synthetic detached
 * joystick (0xd66c2), and executes the 0xe94ec text loader and 0xe93dc
 * line parser. Unrecognized setting syntax stops at token handler 0xe882c;
 * blank/comment-only or supported config enters ReadGenSeqs at 0x30214,
 * initializes its 40 slots and opens the Views resource through the bounded
 * plain-file portion of 0x9ccb8. An absent resource stops at the diagnostic
 * callback 0xea50c; a synthetic plain resource constructs the 0x9d114
 * stream context, reads text lines and tokens with 0x9d170/0x9d1e8/0x9d278,
 * initializes successive '#' records, copies bounded record names, dispatches
 * six first-line camera mode labels, and processes Name/Key/Location text
 * options, scalar damping/minimum-altitude values and RollLock before
 * reading three dword vectors, LookAtObj, LookAt/Dist word vectors, X/Y
 * clamps, two HUD coordinates, Insts and Cockpit. END validates coordinate
 * counts, and non-geometry records advance to subsequent '#' records.
 * Unknown options stop at 0x310d8, invalid coordinate counts at 0x31116,
 * mode-two derived geometry at 0x3119b and final cleanup at 0x3120b.
 * The 0x9d55d numeric converter
 * has a bounded ordinary-decimal host adapter; malformed input stops at
 * 0x9d55d. Oversized name/key strings stop at guarded copy sites. Non-record
 * input is skipped like the original. Packed resources and archive lookups remain
 * explicit boundaries. Without a directory the
 * legacy boundary remains at 0xb3b90. Low memory stops
 * at 0x8b417, and playvideo at 0x8b2d8.
 * Never executes any original x86 or the full game main loop. */
int ef_decomp_probe_main(uint32_t bss_end, uint32_t argc, uint32_t argv,
                         const char *data_dir,
                         ef_decomp_arena_probe *probe, uint32_t *stop_address,
                         char *reason, size_t reason_capacity);

#endif
