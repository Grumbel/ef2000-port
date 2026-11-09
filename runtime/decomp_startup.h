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

/* Execute the original pre-main initializer 0xe20 with synthetic selectors
 * and a bounded six-byte far-stub allocation. Stop at the first real X-32
 * DOS INT 21h vector-registration call 0x9c220. No interrupts are installed.
 * The fields capture the recovered guest-RAM writes *before* that call. */
typedef struct ef_decomp_x32_probe {
    uint32_t stop_address;
    uint32_t request_free;
    uint32_t timer_free;
    uint32_t first_address;
    uint32_t first_selector;
    uint32_t first_length;
    uint32_t far_stub_address;
    uint32_t cleanup_hook;
} ef_decomp_x32_probe;

int ef_decomp_probe_x32(uint32_t bss_end, ef_decomp_x32_probe *probe,
                        char *reason, size_t reason_capacity);

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
 * counts, computes mode-two angles and distance, then advances through
 * subsequent '#' records.
 * Unknown options stop at 0x310d8, invalid coordinate counts at 0x31116,
 * and successful EOF releases the stream and closes the file, then the
 * original caller 0x82cec continues into the 0x2ff44 sequence-list parser.
 * With supported plain resources both readers return, then Aid.c's
 * ReadAidData (0xf80) initializes 27/8 mapping bytes, 50 default objects,
 * eight 0x33-byte records and two 0x34-byte records. Its plain-file reader
 * opens \views\aid2.dat, parses the first 27+8 numeric mapping lines and
 * stops at nested association parsing 0x1202. An absent aid2.dat stops at
 * 0xea50c after initialization. TACTCOM has not yet resumed its outer caller.
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
