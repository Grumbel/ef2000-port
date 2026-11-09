/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/* Common entry point for the first native Linux and DOS decomp builds.
 * This is a bootstrap, not the still-unrecovered EF2000 game entry point.
 * These two globals are fixture data, not the original game's initializers. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ef_abi.h"
#include "ef_globals.h"
#if !defined(__WATCOMC__)
#include "decomp_image.h"
#include "decomp_startup.h"
#endif

int32_t g_hiscore_current;
ef_hiscore_entry g_hiscore_table[10];

#if defined(__WATCOMC__)
#define EF_BUILD_PLATFORM "DOS / OpenWatcom"
#else
#define EF_BUILD_PLATFORM "Linux / GCC"
#endif

static int self_test(void)
{
    g_hiscore_table[9].score = -42;
    g_hiscore_current = -41;
    if (hiscore_qualifies() != 1)
        return 1;

    g_hiscore_current = -42;
    if (hiscore_qualifies() != 0)
        return 1;

    g_hiscore_current = -43;
    if (hiscore_qualifies() != 0)
        return 1;

    puts("Recovered C high-score comparison: passed");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
#if defined(__WATCOMC__)
        puts("Usage: ef2000-decomp [--help | --self-test]");
#else
        puts("Usage: ef2000-decomp [--help | --self-test | --image _EF2000.EXE [--data-dir DIR] [--probe-arena | --probe-main | --probe-main-exit | --probe-main-channel | --probe-x32 | --probe-x32-vectors | --probe-x32-traps | --probe-x32-input | --probe-x32-timer | --probe-x32-tactcom | --probe-x32-tactcom-timer | --probe-x32-tactcom-ail | --probe-x32-tactcom-ail-irq | --probe-x32-tactcom-ail-vectors | --probe-x32-tactcom-ail-callback | --probe-x32-tactcom-ail-module | --probe-x32-tactcom-ail-slot | --probe-x32-tactcom-ail-rate | --probe-x32-tactcom-ail-start | --probe-x32-tactcom-ail-finish]]");
#endif
        puts("Standalone recovered-source bootstrap; gameplay is not implemented.");
        return 0;
    }
#if !defined(__WATCOMC__)
    if (argc >= 3 && strcmp(argv[1], "--image") == 0) {
        ef_decomp_image_info info;
        char reason[128];
        char *image_dir;
        const char *data_dir = NULL;
        const char *probe_type = NULL;
        int i;
        for (i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--data-dir") == 0 && i + 1 < argc && !data_dir) {
                data_dir = argv[++i];
            } else if ((strcmp(argv[i], "--probe-arena") == 0 ||
                        strcmp(argv[i], "--probe-main") == 0 ||
                        strcmp(argv[i], "--probe-main-exit") == 0 ||
                        strcmp(argv[i], "--probe-main-channel") == 0 ||
                        strcmp(argv[i], "--probe-x32") == 0 ||
                        strcmp(argv[i], "--probe-x32-vectors") == 0 ||
                        strcmp(argv[i], "--probe-x32-traps") == 0 ||
                        strcmp(argv[i], "--probe-x32-input") == 0 ||
                        strcmp(argv[i], "--probe-x32-timer") == 0 ||
                        strcmp(argv[i], "--probe-x32-tactcom") == 0 ||
                        strcmp(argv[i], "--probe-x32-tactcom-timer") == 0 ||
                        strcmp(argv[i], "--probe-x32-tactcom-ail") == 0 ||
                        strcmp(argv[i], "--probe-x32-tactcom-ail-irq") == 0 ||
                        strcmp(argv[i], "--probe-x32-tactcom-ail-vectors") == 0 ||
                        strcmp(argv[i], "--probe-x32-tactcom-ail-callback") == 0 ||
                        strcmp(argv[i], "--probe-x32-tactcom-ail-module") == 0 ||
                        strcmp(argv[i], "--probe-x32-tactcom-ail-slot") == 0 ||
                        strcmp(argv[i], "--probe-x32-tactcom-ail-rate") == 0 ||
                        strcmp(argv[i], "--probe-x32-tactcom-ail-start") == 0 ||
                        strcmp(argv[i], "--probe-x32-tactcom-ail-finish") == 0) && !probe_type) {
                probe_type = argv[i];
            } else {
                fputs("Unsupported argument (try --help).\n", stderr);
                return 2;
            }
        }
        if (ef_decomp_load_executable(argv[2], &info, reason, sizeof(reason)) != 0) {
            fprintf(stderr, "ef2000-decomp: %s: %s\n", argv[2], reason);
            return 1;
        }
        image_dir = (char *)malloc(strlen(argv[2]) + 1);
        if (!image_dir) {
            ef_decomp_memory_release();
            return 1;
        }
        strcpy(image_dir, argv[2]);
        {
            char *separator = strrchr(image_dir, '/');
            if (separator)
                separator[separator == image_dir ? 1 : 0] = 0;
            else
                strcpy(image_dir, ".");
        }
        printf("EF2000 decomp bootstrap (%s)\n", EF_BUILD_PLATFORM);
        printf("Original image mapped: %u initialized bytes, BSS through 0x%08x\n",
               info.initialized_size, info.bss_end);
        if (probe_type) {
            if (strcmp(probe_type, "--probe-x32") == 0 ||
                strcmp(probe_type, "--probe-x32-vectors") == 0 ||
                strcmp(probe_type, "--probe-x32-traps") == 0 ||
                strcmp(probe_type, "--probe-x32-input") == 0 ||
                strcmp(probe_type, "--probe-x32-timer") == 0 ||
                strcmp(probe_type, "--probe-x32-tactcom") == 0 ||
                strcmp(probe_type, "--probe-x32-tactcom-timer") == 0 ||
                strcmp(probe_type, "--probe-x32-tactcom-ail") == 0 ||
                strcmp(probe_type, "--probe-x32-tactcom-ail-irq") == 0 ||
                strcmp(probe_type, "--probe-x32-tactcom-ail-vectors") == 0 ||
                strcmp(probe_type, "--probe-x32-tactcom-ail-callback") == 0 ||
                strcmp(probe_type, "--probe-x32-tactcom-ail-module") == 0 ||
                strcmp(probe_type, "--probe-x32-tactcom-ail-slot") == 0 ||
                strcmp(probe_type, "--probe-x32-tactcom-ail-rate") == 0 ||
                strcmp(probe_type, "--probe-x32-tactcom-ail-start") == 0 ||
                strcmp(probe_type, "--probe-x32-tactcom-ail-finish") == 0) {
                ef_decomp_x32_probe x32;
                int virtual_ail_finish =
                    strcmp(probe_type, "--probe-x32-tactcom-ail-finish") == 0;
                int virtual_ail_start = virtual_ail_finish ||
                    strcmp(probe_type, "--probe-x32-tactcom-ail-start") == 0;
                int virtual_ail_rate = virtual_ail_start ||
                    strcmp(probe_type, "--probe-x32-tactcom-ail-rate") == 0;
                int virtual_ail_slot = virtual_ail_rate ||
                    strcmp(probe_type, "--probe-x32-tactcom-ail-slot") == 0;
                int virtual_ail_module = virtual_ail_slot ||
                    strcmp(probe_type, "--probe-x32-tactcom-ail-module") == 0;
                int virtual_ail_callback = virtual_ail_module ||
                    strcmp(probe_type, "--probe-x32-tactcom-ail-callback") == 0;
                int virtual_ail_vectors = virtual_ail_callback ||
                    strcmp(probe_type, "--probe-x32-tactcom-ail-vectors") == 0;
                int virtual_ail_irq = virtual_ail_vectors ||
                    strcmp(probe_type, "--probe-x32-tactcom-ail-irq") == 0;
                int virtual_ail_init = virtual_ail_irq ||
                    strcmp(probe_type, "--probe-x32-tactcom-ail") == 0;
                int virtual_sound_timer = virtual_ail_init ||
                    strcmp(probe_type, "--probe-x32-tactcom-timer") == 0;
                int virtual_tactcom = virtual_sound_timer ||
                    strcmp(probe_type, "--probe-x32-tactcom") == 0;
                int virtual_timer = virtual_tactcom ||
                                    strcmp(probe_type, "--probe-x32-timer") == 0;
                int virtual_input = virtual_timer ||
                                    strcmp(probe_type, "--probe-x32-input") == 0;
                int virtual_traps = virtual_input ||
                                    strcmp(probe_type, "--probe-x32-traps") == 0;
                int virtual_vectors = virtual_traps ||
                                      strcmp(probe_type, "--probe-x32-vectors") == 0;
                int x32_status = virtual_ail_finish
                    ? ef_decomp_probe_x32_tactcom_ail_finish(info.bss_end,
                                                              &x32, reason,
                                                              sizeof(reason))
                    : virtual_ail_start
                    ? ef_decomp_probe_x32_tactcom_ail_start(info.bss_end,
                                                             &x32, reason,
                                                             sizeof(reason))
                    : virtual_ail_rate
                    ? ef_decomp_probe_x32_tactcom_ail_rate(info.bss_end,
                                                            &x32, reason,
                                                            sizeof(reason))
                    : virtual_ail_slot
                    ? ef_decomp_probe_x32_tactcom_ail_slot(info.bss_end,
                                                            &x32, reason,
                                                            sizeof(reason))
                    : virtual_ail_module
                    ? ef_decomp_probe_x32_tactcom_ail_module(info.bss_end,
                                                              &x32, reason,
                                                              sizeof(reason))
                    : virtual_ail_callback
                    ? ef_decomp_probe_x32_tactcom_ail_callback(info.bss_end,
                                                                &x32, reason,
                                                                sizeof(reason))
                    : virtual_ail_vectors
                    ? ef_decomp_probe_x32_tactcom_ail_vectors(info.bss_end,
                                                               &x32, reason,
                                                               sizeof(reason))
                    : virtual_ail_irq
                    ? ef_decomp_probe_x32_tactcom_ail_irq(info.bss_end, &x32,
                                                           reason, sizeof(reason))
                    : virtual_ail_init
                    ? ef_decomp_probe_x32_tactcom_ail(info.bss_end, &x32,
                                                       reason, sizeof(reason))
                    : virtual_sound_timer
                        ? ef_decomp_probe_x32_tactcom_timer(info.bss_end, &x32,
                                                         reason, sizeof(reason))
                    : virtual_tactcom
                        ? ef_decomp_probe_x32_tactcom(info.bss_end, &x32,
                                                      reason, sizeof(reason))
                    : virtual_timer
                        ? ef_decomp_probe_x32_timer(info.bss_end, &x32,
                                                reason, sizeof(reason))
                    : virtual_input
                        ? ef_decomp_probe_x32_input(info.bss_end, &x32,
                                                reason, sizeof(reason))
                    : virtual_traps
                        ? ef_decomp_probe_x32_traps(info.bss_end, &x32,
                                                    reason, sizeof(reason))
                    : virtual_vectors
                        ? ef_decomp_probe_x32_vectors(info.bss_end, &x32,
                                                      reason, sizeof(reason))
                        : ef_decomp_probe_x32(info.bss_end, &x32,
                                              reason, sizeof(reason));
                if (x32_status != 0) {
                    fprintf(stderr, "ef2000-decomp: X-32 probe: %s\n", reason);
                    free(image_dir);
                    ef_decomp_memory_release();
                    return 1;
                }
                printf("Recovered pre-main 0xe20: far stub 0x%08x, cleanup 0x%08x\n",
                       x32.far_stub_address, x32.cleanup_hook);
                printf("X-32 request free %u, timer free %u; first request "
                       "0x%08x selector 0x%04x length 0x%x\n",
                       x32.request_free, x32.timer_free,
                       x32.first_address, x32.first_selector, x32.first_length);
                if (virtual_vectors) {
                    printf("Virtual-only X-32 requests: %u accepted; final request "
                           "0x%08x selector 0x%04x length 0x%x\n",
                           x32.registered_requests, x32.last_address,
                           x32.last_selector, x32.last_length);
                    if (virtual_traps) {
                        printf("Virtual-only X-32 traps: %u descriptions; "
                               "first vector %u handler 0x%08x, "
                               "last vector %u handler 0x%08x\n",
                               x32.registered_traps, x32.first_trap_vector,
                               x32.first_trap_handler, x32.last_trap_vector,
                               x32.last_trap_handler);
                        printf("Synthetic saved trap context: callback 0x%08x "
                               "ESP 0x%08x SS 0x%04x DS 0x%04x\n",
                               x32.saved_trap_callback, x32.saved_trap_esp,
                               x32.saved_trap_ss, x32.saved_trap_ds);
                        if (virtual_input) {
                            printf("Recovered keyboard init: %u LED command bytes, "
                                   "request slots %u/%u, data selector 0x%04x\n",
                                   x32.keyboard_led_bytes,
                                   x32.keyboard_code_slot, x32.keyboard_data_slot,
                                   x32.stored_data_selector);
                            if (virtual_timer) {
                                printf("Virtual-only X-32 timer slot 7: source %u; "
                                       "synthetic AX=2503h 0x%x, AX=2502h 0x%04x:%08x; "
                                       "%u inert AX=2506h registration\n",
                                       x32.pending_timer_source,
                                       x32.timer_2503_result,
                                       x32.timer_2502_result_high,
                                       x32.timer_2502_result_low,
                                       x32.timer_registrations);
                                if (virtual_tactcom) {
                                    printf("Recovered cold input initializer returned to "
                                           "TACTCOM 0x8b47c; state %u, "
                                           "cleanup callback 0x%08x\n",
                                           x32.input_initialized,
                                           x32.input_cleanup_hook);
                                    if (virtual_sound_timer) {
                                        printf("Recovered first-time timer initializer "
                                               "0xe28e8 set guard %u; "
                                               "sound module initialized %u\n",
                                               x32.timer_service_initialized,
                                               x32.sound_module_initialized);
                                        if (virtual_ail_init) {
                                            printf("Recovered cold no-debug AIL setup: "
                                                   "%u inert memory-lock requests, "
                                                   "%u default settings; "
                                                   "trace/settings range flags %u/%u\n",
                                                   x32.ail_locked_ranges,
                                                   x32.ail_settings_written,
                                                   x32.ail_trace_ranges_initialized,
                                                   x32.ail_settings_ranges_initialized);
                                            if (virtual_ail_irq) {
                                                printf("Recovered AIL IRQ table prefix: "
                                                       "%u inert memory-lock requests, "
                                                       "saved DS selector 0x%04x; "
                                                       "virtual IRQ region entered %u\n",
                                                       x32.ail_locked_ranges,
                                                       x32.ail_saved_ds_selector,
                                                       x32.ail_irq_critical_entered);
                                                if (virtual_ail_vectors) {
                                                    printf("Virtual-only DOS IRQ8 vectors: "
                                                           "%u reads, %u inert installation; "
                                                           "saved protected %04x:%08x, "
                                                           "real-mode 0x%08x\n",
                                                           x32.ail_irq_vector_queries,
                                                           x32.ail_irq_vector_registrations,
                                                           x32.ail_irq_original_pm_selector,
                                                           x32.ail_irq_original_pm_offset,
                                                           x32.ail_irq_original_rm_offset);
                                                    if (virtual_ail_callback) {
                                                        printf("Recovered AIL timer callback: "
                                                               "slot 15 value 0x%08x, "
                                                               "PIT reload 0x%08x, %u inert "
                                                               "port writes; virtual IRQ "
                                                               "region exited %u\n",
                                                               x32.ail_timer_slot_value,
                                                               x32.ail_timer_reload,
                                                               x32.ail_pit_port_writes,
                                                               x32.ail_irq_critical_exited);
                                                        if (virtual_ail_module) {
                                                            printf("Recovered AIL sound module: "
                                                                   "%u inert code/data lock "
                                                                   "requests; cleanup 0x%08x "
                                                                   "registered (%u entry)\n",
                                                                   x32.ail_module_locked_ranges,
                                                                   x32.ail_module_cleanup_handler,
                                                                   x32.ail_module_cleanup_count);
                                                            if (virtual_ail_slot) {
                                                                printf("Recovered AIL timer slot: "
                                                                       "offset %u, callback 0x%08x, "
                                                                       "state %u\n",
                                                                       x32.ail_module_timer_slot,
                                                                       x32.ail_module_timer_handler,
                                                                       x32.ail_module_timer_slot_state);
                                                                if (virtual_ail_rate) {
                                                                    printf("Recovered AIL timer rate: "
                                                                           "period %u, PIT reload 0x%08x, "
                                                                           "%u inert cumulative port writes; "
                                                                           "timer handler 0x%08x\n",
                                                                           x32.ail_module_timer_period,
                                                                           x32.ail_module_timer_reload,
                                                                           x32.ail_module_timer_port_writes,
                                                                           x32.ail_module_timer_callback_ptr);
                                                                    if (virtual_ail_finish)
                                                                        printf("Recovered cold TACTCOM tail: "
                                                                               "timer slot 0 running (state 2); "
                                                                               "mode-zero probe and table/word "
                                                                               "selection returned at 0x%08x "
                                                                               "(no DOS IRQ, PIT or audio "
                                                                               "driver installed)\n",
                                                                               x32.stop_address);
                                                                    else if (virtual_ail_start)
                                                                        printf("Recovered AIL timer start: "
                                                                               "slot 0 running (state 2); "
                                                                               "returned to TACTCOM 0x%08x "
                                                                               "(no DOS IRQ, PIT or audio "
                                                                               "driver installed)\n",
                                                                               x32.stop_address);
                                                                    else
                                                                        printf("Stopped before AIL timer start "
                                                                               "0x%08x (no DOS IRQ, PIT or "
                                                                               "audio driver installed)\n",
                                                                               x32.stop_address);
                                                                } else {
                                                                    printf("Stopped before AIL timer interval "
                                                                           "programming 0x%08x (no DOS IRQ, "
                                                                           "PIT or audio driver installed)\n",
                                                                           x32.stop_address);
                                                                }
                                                            } else {
                                                                printf("Stopped before AIL timer-slot "
                                                                       "allocation 0x%08x (no DOS "
                                                                       "IRQ, PIT or audio driver "
                                                                       "installed)\n", x32.stop_address);
                                                            }
                                                        } else {
                                                            printf("Stopped at sound module lock "
                                                                   "0x%08x (no DOS IRQ, PIT "
                                                                   "or audio driver installed)\n",
                                                                   x32.stop_address);
                                                        }
                                                    } else {
                                                        printf("Stopped before AIL timer setup "
                                                               "0x%08x (no DOS IRQ or PIT "
                                                               "installed)\n", x32.stop_address);
                                                    }
                                                } else {
                                                    printf("Stopped at DOS/X-32 IRQ8 vector query "
                                                           "0x%08x (no DOS IRQ or audio driver "
                                                           "installed)\n", x32.stop_address);
                                                }
                                            } else {
                                                printf("Stopped at interrupt-sensitive AIL "
                                                       "runtime initialization 0x%08x "
                                                       "(no audio driver installed)\n",
                                                       x32.stop_address);
                                            }
                                        } else {
                                            printf("Stopped at unimplemented sound service "
                                                   "0x%08x (no sound timer installed)\n",
                                                   x32.stop_address);
                                        }
                                    } else {
                                        printf("TACTCOM post-input C reached first-time "
                                               "timer/sound service 0x%08x "
                                               "(no DOS timer installed)\n",
                                               x32.stop_address);
                                    }
                                } else {
                                    printf("Recovered cold input initializer returned at 0x%08x; "
                                           "state %u, cleanup callback 0x%08x "
                                           "(no DOS timer installed)\n",
                                           x32.stop_address, x32.input_initialized,
                                           x32.input_cleanup_hook);
                                }
                            } else {
                                printf("Timer slot 7 staged source %u; stopped at "
                                       "DOS/X-32 timer service 0x%08x "
                                       "(INT 21h AX=2503h; no timer installed)\n",
                                       x32.pending_timer_source, x32.stop_address);
                            }
                        } else {
                            printf("Recovered X-32 initializer returned at 0x%08x "
                                   "(no DOS interrupts or host traps installed)\n",
                                   x32.stop_address);
                        }
                    } else {
                        printf("Stopped before DOS/X-32 trap registration 0x%08x "
                               "(INT 21h AX=2533h; no vectors installed)\n",
                               x32.stop_address);
                    }
                } else {
                    printf("Stopped at unimplemented DOS/X-32 call 0x%08x "
                           "(INT 21h AX=252Bh; no vector installed)\n",
                           x32.stop_address);
                }
            } else {
                ef_decomp_arena_probe probe;
                uint32_t stopped = 0;
                int main_probe = strcmp(probe_type, "--probe-main") == 0;
                int main_exit_probe = strcmp(probe_type, "--probe-main-exit") == 0;
                int main_channel_probe = strcmp(probe_type, "--probe-main-channel") == 0;
                int status = main_channel_probe
                    ? ef_decomp_probe_main_channel(info.bss_end, 1u, 0u,
                                                   data_dir ? data_dir : image_dir,
                                                   &probe, &stopped,
                                                   reason, sizeof(reason))
                    : main_exit_probe
                    ? ef_decomp_probe_main_exit_handler(info.bss_end, 1u, 0u,
                                                        data_dir ? data_dir : image_dir,
                                                        &probe, &stopped,
                                                        reason, sizeof(reason))
                    : main_probe
                    ? ef_decomp_probe_main(info.bss_end, 1u, 0u,
                                           data_dir ? data_dir : image_dir, &probe, &stopped,
                                           reason, sizeof(reason))
                    : ef_decomp_probe_arena(info.bss_end, &probe, reason, sizeof(reason));
                if (status != 0) {
                    fprintf(stderr, "ef2000-decomp: startup probe: %s\n", reason);
                    free(image_dir);
                    ef_decomp_memory_release();
                    return 1;
                }
                printf("Recovered 0xe29a0 + 0xe2a1c: arena 0x%08x (%u bytes), release hook 0x%08x\n",
                       probe.base, probe.size, probe.release_hook);
                if (main_channel_probe)
                    printf("Recovered channel-mode 0x10dc7f set its mode-change pointer; "
                           "stopped before runtime-hook tail 0x%08x "
                           "(channel 2 unchanged)\n", stopped);
                else if (main_exit_probe)
                    printf("Recovered game main passed TACTCOM and entered exit handler "
                           "0xea470; stopped before channel-mode service 0x%08x "
                           "(no handler installed)\n", stopped);
                else if (main_probe)
                    printf("Recovered game main 0x8b274 entered TACTCOM prefix; "
                           "stopped at unimplemented call 0x%08x\n", stopped);
            }
        }
        if (!probe_type || (strcmp(probe_type, "--probe-x32") != 0 &&
                            strcmp(probe_type, "--probe-x32-vectors") != 0 &&
                            strcmp(probe_type, "--probe-x32-traps") != 0 &&
                            strcmp(probe_type, "--probe-x32-input") != 0 &&
                            strcmp(probe_type, "--probe-x32-timer") != 0 &&
                            strcmp(probe_type, "--probe-x32-tactcom") != 0 &&
                            strcmp(probe_type, "--probe-x32-tactcom-timer") != 0 &&
                            strcmp(probe_type, "--probe-x32-tactcom-ail") != 0 &&
                            strcmp(probe_type, "--probe-x32-tactcom-ail-irq") != 0 &&
                            strcmp(probe_type, "--probe-x32-tactcom-ail-vectors") != 0 &&
                            strcmp(probe_type, "--probe-x32-tactcom-ail-callback") != 0 &&
                            strcmp(probe_type, "--probe-x32-tactcom-ail-module") != 0 &&
                            strcmp(probe_type, "--probe-x32-tactcom-ail-slot") != 0 &&
                            strcmp(probe_type, "--probe-x32-tactcom-ail-rate") != 0 &&
                            strcmp(probe_type, "--probe-x32-tactcom-ail-start") != 0 &&
                            strcmp(probe_type, "--probe-x32-tactcom-ail-finish") != 0))
            puts("TACTCOM settings and Views sequence prefix are bounded; plain resource files parse multiple ordinary '#' records and END validation, but mode-two geometry, EOF cleanup, archives and the first screen remain unimplemented.");
        free(image_dir);
        ef_decomp_memory_release();
        return 0;
    }
#endif
    if (argc > 1 && (argc != 2 || strcmp(argv[1], "--self-test") != 0)) {
        fputs("Unsupported argument (try --help).\n", stderr);
        return 2;
    }

    printf("EF2000 decomp bootstrap (%s)\n", EF_BUILD_PLATFORM);
    if (self_test() != 0) {
        fputs("Recovered C high-score comparison: FAILED\n", stderr);
        return 1;
    }
    if (argc == 1)
        puts("Game entry and TACTCOM memory prefix are recovered; filesystem and platform layer remain incomplete.");
    return 0;
}
