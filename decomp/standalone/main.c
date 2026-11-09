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
        puts("Usage: ef2000-decomp [--help | --self-test | --image _EF2000.EXE [--data-dir DIR] [--probe-arena | --probe-main | --probe-x32 | --probe-x32-vectors | --probe-x32-traps | --probe-x32-input]]");
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
                        strcmp(argv[i], "--probe-x32") == 0 ||
                        strcmp(argv[i], "--probe-x32-vectors") == 0 ||
                        strcmp(argv[i], "--probe-x32-traps") == 0 ||
                        strcmp(argv[i], "--probe-x32-input") == 0) && !probe_type) {
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
                strcmp(probe_type, "--probe-x32-input") == 0) {
                ef_decomp_x32_probe x32;
                int virtual_input = strcmp(probe_type, "--probe-x32-input") == 0;
                int virtual_traps = virtual_input ||
                                    strcmp(probe_type, "--probe-x32-traps") == 0;
                int virtual_vectors = virtual_traps ||
                                      strcmp(probe_type, "--probe-x32-vectors") == 0;
                int x32_status = virtual_input
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
                            printf("Timer slot 7 staged source %u; stopped at "
                                   "DOS/X-32 timer service 0x%08x "
                                   "(INT 21h AX=2503h; no timer installed)\n",
                                   x32.pending_timer_source, x32.stop_address);
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
                int status = main_probe
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
                if (main_probe)
                    printf("Recovered game main 0x8b274 entered TACTCOM prefix; "
                           "stopped at unimplemented call 0x%08x\n", stopped);
            }
        }
        if (!probe_type || (strcmp(probe_type, "--probe-x32") != 0 &&
                            strcmp(probe_type, "--probe-x32-vectors") != 0 &&
                            strcmp(probe_type, "--probe-x32-traps") != 0 &&
                            strcmp(probe_type, "--probe-x32-input") != 0))
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
