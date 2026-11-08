/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/* Common entry point for the first native Linux and DOS decomp builds.
 * This is a bootstrap, not the still-unrecovered EF2000 game entry point.
 * These two globals are fixture data, not the original game's initializers. */

#include <stdio.h>
#include <string.h>

#include "ef_abi.h"
#include "ef_globals.h"

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
        puts("Usage: ef2000-decomp [--help | --self-test]");
        puts("Standalone recovered-source bootstrap; gameplay is not implemented.");
        return 0;
    }
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
