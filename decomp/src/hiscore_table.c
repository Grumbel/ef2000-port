/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/* The arcade high-score table g_hiscore_table: defaults, display in the
 * "hiscores" window, hiscores.dat load and save, and insertion. The file
 * helpers take their arguments on the stack; their first four
 * parameters are unused register slots. */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define HISCORE_ROWS 10
#define HISCORE_NAME_POOL 48
#define HISCORE_FILE_NAME 0x0017f694u   /* "hiscores.dat" */
#define HISCORE_WINDOW 0x0017f648u      /* "hiscores" */

/* 0x4050, 138 bytes. Seed the random generator with the time and fill
 * each row with a random name from the pool, a random difficulty 1-3
 * and the default score of that row. */
void hiscore_generate_defaults_4050(void)
{
    int16_t row;

    set_patch_dword_a45e4(0u, 0u, 0u, 0u, get_time_seconds_a4574(0u));  /* srand(time(0)) */
    for (row = 0; row < HISCORE_ROWS; ++row) {
        int16_t name = (int16_t)((uint16_t)half_word_9c960() % HISCORE_NAME_POOL);

        copy_string_9d749((ef_addr32)(uintptr_t)g_hiscore_table[row].name,
                          (ef_addr32)(uintptr_t)g_hiscore_name_pool[name]);
        g_hiscore_table[row].difficulty = (uint8_t)((uint16_t)half_word_9c960() % 3u + 1u);
        g_hiscore_table[row].score = g_default_hiscores[row].score;
    }
}

/* 0x40dc, 288 bytes. Write the table into the name, level and score
 * columns of the "hiscores" window, or of WINDOW when it is given.
 * Each column is a chain of widgets linked at +0x21. */
void hiscore_refresh_display_40dc(ef_addr32 window)
{
    char score[12];
    ef_addr32 widget;
    int16_t row;

    if (window == 0u) {
        window = find_named_or_abort_a46cc(HISCORE_WINDOW);
        if (window == 0u)
            return;
    }
    widget = find_widget_checked_a78bc(window, 0x0017f654u);    /* "name1" */
    for (row = 0; row < HISCORE_ROWS && widget != 0u; ++row) {
        show_widget_a7588(widget, (ef_addr32)(uintptr_t)g_hiscore_table[row].name);
        widget = EF_U32(widget + 0x21u);
    }

    widget = find_widget_checked_a78bc(find_named_or_abort_a46cc(HISCORE_WINDOW),
                                       0x0017f65cu);          /* "level1" */
    for (row = 0; row < HISCORE_ROWS && widget != 0u; ++row) {
        switch (g_hiscore_table[row].difficulty) {
        case 1:
            show_widget_a7588(widget, 0x0012f631u);           /* "Rookie" */
            break;
        case 2:
            show_widget_a7588(widget, 0x0012f638u);           /* "Profi" */
            break;
        case 3:
            show_widget_a7588(widget, 0x0012f63eu);           /* "Topgun" */
            break;
        default:
            guest_callptr1_x(g_gui_error_hook, 0x0017f664u);  /* "Invalid difficulty level in arcade!" */
            break;
        }
        widget = EF_U32(widget + 0x21u);
    }

    widget = find_widget_checked_a78bc(find_named_or_abort_a46cc(HISCORE_WINDOW),
                                       0x0017f688u);          /* "score1" */
    for (row = 0; row < HISCORE_ROWS && widget != 0u; ++row) {
        guest_sprintf_a4342((ef_addr32)(uintptr_t)score, 0x0017f690u,  /* "%d" */
                            g_hiscore_table[row].score);
        show_widget_a7588(widget, (ef_addr32)(uintptr_t)score);
        widget = EF_U32(widget + 0x21u);
    }
}

/* ORIGINAL BUG: the checksum loops never advance their pointer, so the
 * checksum is the first name byte added 250 times. */
static uint32_t hiscore_checksum(void)
{
    uint32_t sum = 0u;
    int16_t i;

    for (i = 0; (uint16_t)i < sizeof(g_hiscore_table); ++i)
        sum += (uint8_t)g_hiscore_table[0].name[0];
    return sum;
}

/* 0x41fc, 165 bytes. Load hiscores.dat (checksum, then the table).
 * Without the file the defaults are generated and 0 returned; a wrong
 * checksum also falls back to the defaults. Short reads are not
 * checked. Returns 1 after refreshing the window. */
uint8_t hiscore_load_41fc(void)
{
    uint32_t checksum = 0u;
    int32_t handle;

    if (guest_callptr_eaxedx_ret_x(g_resource_size_hook, HISCORE_FILE_NAME, 0u) == 0u) {
        hiscore_generate_defaults_4050();
        return 0u;
    }
    handle = open_slot_a7900(0u, 0u, 0u, 0u, HISCORE_FILE_NAME);
    if (handle == -1) {
        hiscore_generate_defaults_4050();
        return 0u;
    }
    read_handle_a79d4(0u, 0u, 0u, 0u, (uint32_t)handle, sizeof(checksum),
                      (ef_addr32)(uintptr_t)&checksum);
    read_handle_a79d4(0u, 0u, 0u, 0u, (uint32_t)handle, sizeof(g_hiscore_table),
                      (ef_addr32)(uintptr_t)g_hiscore_table);
    close_handle_a7990(0u, 0u, 0u, 0u, (uint32_t)handle);
    if (checksum != hiscore_checksum())
        hiscore_generate_defaults_4050();
    hiscore_refresh_display_40dc(0u);
    return 1u;
}

/* 0x42a4, 114 bytes. Write the checksum and the table to hiscores.dat;
 * returns 0 if the file cannot be created. Write errors are ignored. */
uint8_t hiscore_save_42a4(void)
{
    uint32_t checksum = 0u;
    int32_t handle = open_slot_a7948(0u, 0u, 0u, 0u, HISCORE_FILE_NAME);

    if (handle == -1)
        return 0u;
    checksum = hiscore_checksum();
    write_handle_a7a1c(0u, 0u, 0u, 0u, (uint32_t)handle, sizeof(checksum),
                       (ef_addr32)(uintptr_t)&checksum);
    write_handle_a7a1c(0u, 0u, 0u, 0u, (uint32_t)handle, sizeof(g_hiscore_table),
                       (ef_addr32)(uintptr_t)g_hiscore_table);
    close_handle_a7990(0u, 0u, 0u, 0u, (uint32_t)handle);
    return 1u;
}

/* 0x433c, 242 bytes. Insert g_hiscore_current under NAME at the first
 * row whose score it reaches (ties go above, as the comparison is with
 * score - 1, which wraps for INT_MIN). Lower rows move down and the
 * last falls off. Returns the row, or -1 if the score is too low. */
int16_t hiscore_insert_433c(ef_addr32 name)
{
    int16_t position;
    int16_t row;

    for (position = 0; position < HISCORE_ROWS; ++position)
        if (g_hiscore_current > (int32_t)((uint32_t)g_hiscore_table[position].score - 1u))
            break;
    if (position == HISCORE_ROWS)
        return -1;

    for (row = HISCORE_ROWS - 1; row > position; --row) {
        copy_string_9d749((ef_addr32)(uintptr_t)g_hiscore_table[row].name,
                          (ef_addr32)(uintptr_t)g_hiscore_table[row - 1].name);
        g_hiscore_table[row].difficulty = g_hiscore_table[row - 1].difficulty;
        g_hiscore_table[row].score = g_hiscore_table[row - 1].score;
    }
    copy_string_9d749((ef_addr32)(uintptr_t)g_hiscore_table[position].name, name);
    g_hiscore_table[position].difficulty = g_difficulty;
    g_hiscore_table[position].score = g_hiscore_current;
    return position;
}
