/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/* Arcade-mode GUI callbacks: high-score name entry and the base
 * selection of the "kots" dialog. Plain Watcom register ABI. */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"
#include "ef_hiscore_093.h"

/* 0x4c50, 162 bytes. Accept the high-score name: copy the text of
 * "nametext" in the "entername" dialog into the name of the new entry
 * (rank in word 0x121e48), substitute "Jemand" for an empty name, save
 * and redisplay the table and switch to the "arcade" screen. The rank is
 * read again for each use, as in the original. */
void InitArcade(void)
{
    copy_string_9d749((ef_addr32)(uintptr_t)g_hiscore_table[g_hiscore_new_rank].name,
                      first_link_a75fc(find_widget_checked_a78bc(
                          find_named_or_abort_a46cc(0x0017f714u),  /* "entername" */
                          0x0017f7dcu)));                          /* "nametext" */
    if (strlen_ds((ef_addr32)(uintptr_t)g_hiscore_table[g_hiscore_new_rank].name) == 0u)
        copy_string_9d749((ef_addr32)(uintptr_t)g_hiscore_table[g_hiscore_new_rank].name,
                          0x00130be8u);                            /* "Jemand" */
    hiscore_save_42a4();
    hiscore_refresh_display_40dc(0u);
    set_pending_name_ac7a8(0x0017f79cu);                           /* "arcade" */
}

/* 0x4d4c, 199 bytes, click callback of the arcade difficulty buttons
 * (WIDGET name at +0x3f): "rookie", "pro" and anything else select
 * levels 1, 2 and 3 with the score files "arcroo.txt", "arcpro.txt" and
 * "arctop.txt". The level is stored as a word over the difficulty byte.
 * Then reload the difficulty rows (0x7bee8), hand the selected 76-byte
 * row's text to the "summary" dialog and redraw it. */
void gpwAcceptHiscore(ef_addr32 widget)
{
    ef_addr32 name = widget + 0x3fu;
    ef_addr32 file;
    ef_addr32 dialog;
    uint32_t i;

    if (guest_strcmp(name, 0x0017f6b8u) == 0) {          /* "rookie" */
        g_difficulty_word = 1u;
        file = 0x0017f7b8u;                              /* "arcroo.txt" */
    } else if (guest_strcmp(name, 0x0017f6c0u) == 0) {   /* "pro" */
        g_difficulty_word = 2u;
        file = 0x0017f7c4u;                              /* "arcpro.txt" */
    } else {
        g_difficulty_word = 3u;
        file = 0x0017f7d0u;                              /* "arctop.txt" */
    }
    for (i = 0; i < 11u; ++i)                           /* "arcXXX.txt" and NUL */
        g_score_file_name[i] = EF_U8(file + i);
    guest_fn_7bee8_x(0x00121e40u);
    dialog = find_named_or_abort_a46cc(0x0017f7a4u);    /* "summary" */
    set_session_text_7c4fc(g_difficulty_row_pool + 0x28u +
                               (uint32_t)(((int32_t)g_difficulty_selection - 1) * 76),
                           dialog);
    redraw_item_a5ef8(EF_U8(dialog + 0x10u));
}

/* 0x5110, 194 bytes, click callback of the base buttons of the "kots"
 * dialog; the WIDGET name (+0x43) holds the base number from 1. Unless
 * the dialog is locked (mode byte 1), advance the base's side and its
 * mirror byte, wrapping both to 0 at 2, label the button with the side
 * name ("GRUN"/"ROT"), relabel the matching "abase<n>" widget with that
 * side's home base, and redraw the dialog. */
void kotsClickBase(ef_addr32 widget)
{
    char name[20];
    int16_t base;

    if (g_kots_lock == 1u)
        return;
    base = (int16_t)(guest_convert_parser_integer(widget + 0x43u) - 1);
    ++g_kots_base_side[base];
    ++g_kots_base_side_mirror[base];
    if (g_kots_base_side[base] >= 2u) {
        g_kots_base_side[base] = 0u;
        g_kots_base_side_mirror[base] = 0u;
    }
    show_widget_a7588(widget, g_kots_side_labels[g_kots_base_side[base]]);
    guest_sprintf_a4342((ef_addr32)(uintptr_t)name, 0x0017f84cu, (int32_t)base + 1); /* "abase%d" */
    show_widget_a7588(find_widget_checked_a78bc(find_named_or_abort_a46cc(0x0017f7f8u), /* "kots" */
                                                (ef_addr32)(uintptr_t)name),
                      g_kots_side_bases[g_kots_base_side[base]].name);
    apply_a5ef8_a6018(0x0017f7f8u);
}

/* 0x522c, 601 bytes, accept callback of the "kots" setup dialog. For
 * each of the count-word (0x1357b0) rows, walk the team1/ateam1/base1/
 * abase1 widget chains (+0x21) together while team1 and ateam1 rows
 * exist: "soloplay" shows team1 and abase1 with team "Keine Angaben";
 * "teamplay" shows ateam1 and abase1 with the row's side label; any
 * other name shows ateam1 and base1 with the side label and the side's
 * home base. The chosen mode 1/2/3 is stored in the kots mode byte. */
void kotsAccept(ef_addr32 widget)
{
    ef_addr32 team = find_widget_checked_a78bc(find_named_or_abort_a46cc(0x0017f7f8u), 0x0017f824u);  /* "kots", "team1" */
    ef_addr32 ateam = find_widget_checked_a78bc(find_named_or_abort_a46cc(0x0017f7f8u), 0x0017f83cu); /* "ateam1" */
    ef_addr32 base = find_widget_checked_a78bc(find_named_or_abort_a46cc(0x0017f7f8u), 0x0017f82cu);  /* "base1" */
    ef_addr32 abase = find_widget_checked_a78bc(find_named_or_abort_a46cc(0x0017f7f8u), 0x0017f844u); /* "abase1" */
    ef_addr32 name = widget + 0x3fu;
    uint8_t mode;
    int16_t i;

    if (guest_strcmp(name, 0x0017f800u) == 0)              /* "soloplay" */
        mode = 1u;
    else if (guest_strcmp(name, 0x0017f854u) == 0)         /* "teamplay" */
        mode = 2u;
    else
        mode = 3u;
    for (i = 0; i < g_tactical_lookup_count; ++i) {
        uint8_t side;

        if (team == 0u || ateam == 0u)
            continue;
        side = g_kots_base_side[i];
        set_widget_bit6(team, mode == 1u);
        set_widget_bit6(ateam, mode != 1u);
        set_widget_bit6(base, mode == 3u);
        set_widget_bit6(abase, mode != 3u);
        show_widget_a7588(team, mode == 1u ? 0x0012f69du      /* "Keine Angaben" */
                                           : g_kots_side_labels[side]);
        if (mode == 3u)
            show_widget_a7588(abase, g_kots_side_bases[side].name);
        team = EF_U32(team + 0x21u);
        ateam = EF_U32(ateam + 0x21u);
        base = EF_U32(base + 0x21u);
        abase = EF_U32(abase + 0x21u);
    }
    g_kots_lock = mode;
}
