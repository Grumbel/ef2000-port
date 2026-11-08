/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/* Common entry point for the first native Linux and DOS decomp builds.
 * This is a bootstrap, not the still-unrecovered EF2000 game entry point.
 * These two globals are fixture data, not the original game's initializers. */

#include <stdio.h>
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
        puts("Usage: ef2000-decomp [--help | --self-test | --image _EF2000.EXE [--probe-arena]]");
#endif
        puts("Standalone recovered-source bootstrap; gameplay is not implemented.");
        return 0;
    }
#if !defined(__WATCOMC__)
    if ((argc == 3 || (argc == 4 && strcmp(argv[3], "--probe-arena") == 0)) &&
        strcmp(argv[1], "--image") == 0) {
        ef_decomp_image_info info;
        char reason[128];
        if (ef_decomp_load_executable(argv[2], &info, reason, sizeof(reason)) != 0) {
            fprintf(stderr, "ef2000-decomp: %s: %s\n", argv[2], reason);
            return 1;
        }
        printf("EF2000 decomp bootstrap (%s)\n", EF_BUILD_PLATFORM);
        printf("Original image mapped: %u initialized bytes, BSS through 0x%08x\n",
               info.initialized_size, info.bss_end);
        if (argc == 4) {
            ef_decomp_arena_probe probe;
            if (ef_decomp_probe_arena(info.bss_end, &probe, reason, sizeof(reason)) != 0) {
                fprintf(stderr, "ef2000-decomp: arena probe: %s\n", reason);
                ef_decomp_memory_release();
                return 1;
            }
            printf("Recovered 0xe29a0 + 0xe2a1c: arena 0x%08x (%u bytes), release hook 0x%08x\n",
                   probe.base, probe.size, probe.release_hook);
        }
        puts("Recovered game entry and first-screen initialization are not yet linked.");
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
        puts("Game entry point, global data and platform layer are not recovered yet.");
    return 0;
}
