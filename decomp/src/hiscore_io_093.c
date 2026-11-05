/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_hiscore_093.h"
#include "ef_globals.h"

#include <stdint.h>

enum { HISCORE_TABLE_BYTES = 250 };
enum { HISCORE_FILENAME = 0x17f694 };

EF_STATIC_ASSERT(hiscore_io_093_size, sizeof(g_hiscore_table) == HISCORE_TABLE_BYTES);

/* ORIGINAL BUG shared by 0x426d and 0x42d4: EBX does not advance in the
 * 250-iteration checksum loops. Consequently only the first name byte is
 * summed 250 times; every other record byte is ignored. */
static uint32_t original_checksum(void)
{
    return (uint32_t)(uint8_t)g_hiscore_table[0].name[0] * HISCORE_TABLE_BYTES;
}

/* 0x41fc size 165. The preflight/open failures regenerate defaults without
 * refreshing the GUI and return 0. After opening, the unchecked original
 * reads 4+250 bytes, closes, then regenerates on checksum failure. In both
 * cases it refreshes the GUI and returns 1. Host read boundary must emulate
 * the original DOS short-read behavior if exercising truncated files. */
uint8_t hiscore_load_41fc(void)
{
    int32_t handle;
    uint32_t checksum = 0u;
    if (!hiscore_file_exists_093(HISCORE_FILENAME)) {
        hiscore_generate_defaults_4050();
        return 0u;
    }
    handle = hiscore_file_open_093(HISCORE_FILENAME, 0);
    if (handle == -1) {
        hiscore_generate_defaults_4050();
        return 0u;
    }
    hiscore_file_read_093(handle, &checksum, sizeof(checksum));
    hiscore_file_read_093(handle, g_hiscore_table, HISCORE_TABLE_BYTES);
    hiscore_file_close_093(handle);
    if (checksum != original_checksum())
        hiscore_generate_defaults_4050();
    hiscore_refresh_display_40dc(0u);
    return 1u;
}

/* 0x42a4 size 114. DOS open failure is the only tested failure branch;
 * original file-write return counts are intentionally not checked. */
uint8_t hiscore_save_42a4(void)
{
    int32_t handle = hiscore_file_open_093(HISCORE_FILENAME, 1);
    uint32_t checksum;
    if (handle == -1)
        return 0u;
    checksum = original_checksum();
    hiscore_file_write_093(handle, &checksum, sizeof(checksum));
    hiscore_file_write_093(handle, g_hiscore_table, HISCORE_TABLE_BYTES);
    hiscore_file_close_093(handle);
    return 1u;
}
