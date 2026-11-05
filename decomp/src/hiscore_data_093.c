/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_hiscore_093.h"
#include "ef_globals.h"
#include "ef_mem.h"

#include <stdint.h>

EF_STATIC_ASSERT(hiscore_data_093_entry_size, sizeof(ef_hiscore_entry) == 25);
EF_STATIC_ASSERT(hiscore_data_093_level_offset, offsetof(ef_hiscore_entry, difficulty) == 20);
EF_STATIC_ASSERT(hiscore_data_093_score_offset, offsetof(ef_hiscore_entry, score) == 21);

/* The original calls strcpy, which leaves bytes after NUL in the destination
 * unchanged. The host tests supply NUL-terminated input just like gameplay. */
static void copy_guest_name(char *destination, ef_addr32 guest_source)
{
    unsigned i = 0;
    do {
        destination[i] = (char)EF_U8(guest_source + i);
    } while (destination[i++] != '\0');
}

static void copy_record_name(char *destination, const char *source)
{
    unsigned i = 0;
    do {
        destination[i] = source[i];
    } while (destination[i++] != '\0');
}

/* 0x4050 size 138. Twenty unsigned 16-bit RNG results, two per row; the
 * original initializes its random generator before processing the name pool.
 * Each source name occupies 20 bytes; each default score has a 25-byte stride. */
void hiscore_generate_defaults_4050(void)
{
    unsigned row;
    hiscore_seed_093();
    for (row = 0; row < 10u; ++row) {
        uint16_t name_index = (uint16_t)(hiscore_random_093() % 48u);
        copy_guest_name(g_hiscore_table[row].name,
                        0x121292u + (uint32_t)name_index * 20u);
        g_hiscore_table[row].difficulty = (uint8_t)(hiscore_random_093() % 3u + 1u);
        g_hiscore_table[row].score = EF_S32(0x1211adu + row * 25u);
    }
}

/* 0x433c size 242. The DEC before signed JLE is intentional: INT_MIN in a
 * stored row wraps to INT_MAX, and equal scores normally enter before ties.
 * Shifting uses NUL-terminated strcpy, not a packed 25-byte memcpy. */
int32_t hiscore_insert_433c(ef_addr32 guest_name)
{
    uint32_t current_bits = (uint32_t)g_hiscore_current;
    unsigned position;
    unsigned row;
    for (position = 0; position < 10u; ++position) {
        uint32_t threshold = (uint32_t)g_hiscore_table[position].score - 1u;
        int32_t signed_threshold;
        /* Bit-preserving signed conversion; avoids implementation-defined
         * unsigned-to-signed conversion at INT_MIN/INT_MAX boundaries. */
        unsigned char *destination = (unsigned char *)&signed_threshold;
        unsigned char *source = (unsigned char *)&threshold;
        for (row = 0; row < 4u; ++row)
            destination[row] = source[row];
        if (g_hiscore_current > signed_threshold)
            break;
    }
    if (position == 10u)
        return -1;

    for (row = 9u; row > position; --row) {
        ef_hiscore_entry *to = &g_hiscore_table[row];
        const ef_hiscore_entry *from = &g_hiscore_table[row - 1u];
        copy_record_name(to->name, from->name);
        to->difficulty = from->difficulty;
        to->score = from->score;
    }
    copy_guest_name(g_hiscore_table[position].name, guest_name);
    g_hiscore_table[position].difficulty = EF_U8(0x121e44u);
    g_hiscore_table[position].score = (int32_t)current_bits;
    return (int32_t)position;
}
