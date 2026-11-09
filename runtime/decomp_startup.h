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
 * The fields capture the recovered guest-RAM writes *before* that call.
 * In virtual mode, they instead include all nine request records and the
 * stop address of the next unsupported trap-installation service. */
typedef struct ef_decomp_x32_probe {
    uint32_t stop_address;
    uint32_t request_free;
    uint32_t timer_free;
    uint32_t first_address;
    uint32_t first_selector;
    uint32_t first_length;
    uint32_t far_stub_address;
    uint32_t cleanup_hook;
    uint32_t registered_requests;
    uint32_t last_address;
    uint32_t last_selector;
    uint32_t last_length;
    uint32_t registered_traps;
    uint32_t saved_trap_callback;
    uint32_t saved_trap_esp;
    uint32_t saved_trap_ss;
    uint32_t saved_trap_ds;
    uint32_t first_trap_vector;
    uint32_t first_trap_handler;
    uint32_t last_trap_vector;
    uint32_t last_trap_handler;
    uint32_t keyboard_led_bytes;
    uint32_t keyboard_code_slot;
    uint32_t keyboard_data_slot;
    uint32_t pending_timer_source;
    uint32_t stored_data_selector;
    uint32_t timer_2503_result;
    uint32_t timer_2502_result_low;
    uint32_t timer_2502_result_high;
    uint32_t timer_registrations;
    uint32_t input_cleanup_hook;
    uint32_t input_initialized;
    uint32_t timer_service_initialized;
    uint32_t sound_module_initialized;
    uint32_t ail_locked_ranges;
    uint32_t ail_settings_written;
    uint32_t ail_trace_ranges_initialized;
    uint32_t ail_settings_ranges_initialized;
    uint32_t ail_irq_critical_entered;
    uint32_t ail_saved_ds_selector;
    uint32_t ail_irq_vector_queries;
    uint32_t ail_irq_vector_registrations;
    uint32_t ail_irq_original_pm_offset;
    uint32_t ail_irq_original_pm_selector;
    uint32_t ail_irq_original_rm_offset;
    uint32_t ail_timer_slot_value;
    uint32_t ail_timer_reload;
    uint32_t ail_pit_port_writes;
    uint32_t ail_irq_critical_exited;
    uint32_t ail_module_locked_ranges;
    uint32_t ail_module_cleanup_handler;
    uint32_t ail_module_cleanup_count;
    uint32_t ail_module_timer_slot;
    uint32_t ail_module_timer_handler;
    uint32_t ail_module_timer_slot_state;
    uint32_t ail_module_timer_period;
    uint32_t ail_module_timer_reload;
    uint32_t ail_module_timer_callback_ptr;
    uint32_t ail_module_timer_port_writes;
} ef_decomp_x32_probe;

int ef_decomp_probe_x32(uint32_t bss_end, ef_decomp_x32_probe *probe,
                        char *reason, size_t reason_capacity);

/* Opt-in registry-only continuation: store nine X-32 request descriptions
 * in an inert host table and return success to the game-owned allocator.
 * Stop before the next trap-installation service. No vector is activated. */
int ef_decomp_probe_x32_vectors(uint32_t bss_end, ef_decomp_x32_probe *probe,
                                char *reason, size_t reason_capacity);

/* Additional opt-in test-only model for the eight AX=2533h trap requests.
 * Captures synthetic ESP/SS/DS and stops at return 0xf43, without invoking
 * a DOS interrupt or installing anything on the host. */
int ef_decomp_probe_x32_traps(uint32_t bss_end, ef_decomp_x32_probe *probe,
                              char *reason, size_t reason_capacity);

/* Continue after the recovered pre-main initializer into keyboard setup,
 * two additional X-32 request records and the original 0x734 timer pool.
 * Stop at the first unsupported INT 21h AX=2503h call at 0x324, with only
 * the timer source field staged. Keyboard port and vector services are
 * strictly bounded virtual hardware doubles. */
int ef_decomp_probe_x32_input(uint32_t bss_end, ef_decomp_x32_probe *probe,
                              char *reason, size_t reason_capacity);

/* Complete the same recovered cold input C path after opt-in synthetic
 * AX=2503h/2502h results and an inert AX=2506h registration receipt.
 * Stop at the original 0xcd550 return. No DOS call or timer is executed. */
int ef_decomp_probe_x32_timer(uint32_t bss_end, ef_decomp_x32_probe *probe,
                              char *reason, size_t reason_capacity);

/* Resume the recovered TACTCOM 0x8b47c post-input source after the complete
 * virtual-only cold X-32 keyboard/timer initializer. Stop at first-time
 * timer/sound service 0xe28e8, before installing a real OS timer. This
 * deliberately does not simulate the preceding TACTCOM file/options phase. */
int ef_decomp_probe_x32_tactcom(uint32_t bss_end, ef_decomp_x32_probe *probe,
                                char *reason, size_t reason_capacity);

/* Enter original source-C 0xe28e8/0xd6dc4 on first-time cold TACTCOM
 * startup. Observe the 0x15f751 guard write and stop before sound-system
 * service 0x101c54. No actual sound driver or timer is installed. */
int ef_decomp_probe_x32_tactcom_timer(uint32_t bss_end,
                                      ef_decomp_x32_probe *probe,
                                      char *reason, size_t reason_capacity);

/* Follow the no-AIL_DEBUG environment branch through 19 recovered default
 * AIL settings. Inertly record exact memory-lock requests and stop before
 * interrupt-sensitive runtime init 0x110fd4 (CLI), not a sound driver. */
int ef_decomp_probe_x32_tactcom_ail(uint32_t bss_end,
                                    ef_decomp_x32_probe *probe,
                                    char *reason, size_t reason_capacity);

/* Additional opt-in table initialization under a virtual-only IRQ-disabled
 * region. Record two more code/data locks, clear AIL timer tables, then stop
 * before the first live INT 21h AX=2502h vector query for IRQ 8. */
int ef_decomp_probe_x32_tactcom_ail_irq(uint32_t bss_end,
                                        ef_decomp_x32_probe *probe,
                                        char *reason, size_t reason_capacity);

/* Advance the opt-in IRQ table prefix using synthetic DOS AX=2502h/2503h
 * responses and an inert AX=2506h request. Stop before the timer wrapper
 * call at 0x1110ab. No interrupt vectors or PIT timers are actually set. */
int ef_decomp_probe_x32_tactcom_ail_vectors(uint32_t bss_end,
                                            ef_decomp_x32_probe *probe,
                                            char *reason, size_t reason_capacity);

/* Continue the original AIL timer registration, including its recovered
 * source-C nested lock/reprogram routines and three *inert* PIT write
 * requests. Restore virtual IRQ flags and stop at the next sound module
 * lock setup 0xd6df0. No host port IO or IRQ changes are performed. */
int ef_decomp_probe_x32_tactcom_ail_callback(uint32_t bss_end,
                                             ef_decomp_x32_probe *probe,
                                             char *reason, size_t reason_capacity);

/* In a separate opt-in continuation execute recovered 0xd6df0 and its
 * caller's two additional timer-service memory locks. Confirm the
 * original module cleanup registration; stop before 0x111365 allocates a
 * callback slot. All locks are inert and no audio/timer is activated. */
int ef_decomp_probe_x32_tactcom_ail_module(uint32_t bss_end,
                                           ef_decomp_x32_probe *probe,
                                           char *reason, size_t reason_capacity);

/* Continue past the original 0x111365 timer slot allocator using its
 * recovered source C. The free slot becomes state 1 with callback 0xe28c4;
 * stop before 0x1114f7 programs the interval. No hardware is touched. */
int ef_decomp_probe_x32_tactcom_ail_slot(uint32_t bss_end,
                                         ef_decomp_x32_probe *probe,
                                         char *reason, size_t reason_capacity);

/* Run recovered source C 0x1114f7 through the nested slot/period selector,
 * record the second three inert PIT requests, and stop before 0x11141f
 * promotes the newly configured timer slot to running state. */
int ef_decomp_probe_x32_tactcom_ail_rate(uint32_t bss_end,
                                         ef_decomp_x32_probe *probe,
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
