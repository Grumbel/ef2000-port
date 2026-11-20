/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/* Custom-mission ("custom" dialog) slot editing. The five flight slots
 * are kept in parallel arrays: aircraft count, aircraft type, guns-only
 * flag and skill. */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* 0x7de5c, 446 bytes, change callback of a custom-mission slot widget.
 * Find which "slot1"-chained entry of the "custom" dialog is current
 * (the first with flag bit 0 set; with a null WIDGET the scan stays at
 * slot 0). A name of up to two characters is a number: in the "custom"
 * dialog itself it is the aircraft count, elsewhere (for slots after
 * the first) the 1-based aircraft type. The names gunsyes/gunsno and
 * high/medium/low set the guns-only flag and the skill. Finally the
 * slot line "<count> <type> <armament> <skill>" is shown. */
void simSyncSlots(ef_addr32 widget)
{
    char text[0x64];
    ef_addr32 name;
    ef_addr32 entry;
    uint8_t slot;

    g_custom_slot = 0;
    entry = find_widget_checked_a78bc(find_named_or_abort_a46cc(0x00187f94u), /* "custom" */
                                      0x00187fc8u);                          /* "slot1" */
    if (widget != 0u) {
        while ((EF_U8(entry + 0x1du) & 1u) == 0u) {
            g_custom_slot++;
            entry = EF_U32(entry + 0x21u);
        }
    }
    name = widget + 0x3fu;
    if (strlen_ds(name) <= 2u) {
        if (guest_strcmp(EF_U32(widget + 0x51u), 0x00187f94u) == 0)     /* "custom" */
            g_custom_slot_count[g_custom_slot] = (uint8_t)parse_int(name);
        else if (g_custom_slot != 0)
            g_custom_slot_type[g_custom_slot] = (int16_t)(parse_int(name) - 1);
    }
    slot = g_custom_slot;
    if (guest_strcmp(name, 0x001880b8u) == 0)                  /* "gunsyes" */
        g_custom_slot_guns_only[slot] = 1u;
    else if (guest_strcmp(name, 0x00188604u) == 0)             /* "gunsno" */
        g_custom_slot_guns_only[slot] = 0u;
    else if (guest_strcmp(name, 0x00187eecu) == 0)             /* "high" */
        g_custom_slot_skill[slot] = 0u;
    else if (guest_strcmp(name, 0x00187fc0u) == 0)             /* "medium" */
        g_custom_slot_skill[slot] = 1u;
    else if (guest_strcmp(name, 0x00187ee8u) == 0)             /* "low" */
        g_custom_slot_skill[slot] = 2u;

    slot = g_custom_slot;
    guest_sprintf_a4342((ef_addr32)(uintptr_t)text, 0x0018860cu,    /* "%d %s %s %s" */
                        (uint32_t)g_custom_slot_count[slot],
                        g_text_strings[g_aircraft_type_text[g_custom_slot_type[slot]]],
                        g_custom_slot_guns_only[slot] != 0u ? 0x0013291au   /* guns only */
                                                            : 0x00131eebu,  /* full armament */
                        g_skill_names[g_custom_slot_skill[slot]]);
    show_widget_a7588(entry, (ef_addr32)(uintptr_t)text);
}

/* Training-mission list entries of 0x7c328: widget name and file. */
struct training_entry {
    ef_addr32 name;
    ef_addr32 file;
};

static const struct training_entry training_entries[] = {
    { 0x00187e10u, 0x00187e14u },   /* "2": "basic.txt" */
    { 0x00187e20u, 0x00187e24u },   /* "3": "weapon.txt" */
    { 0x00187e30u, 0x00187e34u },   /* "4": "air2air.txt" */
    { 0x00187e40u, 0x00187e44u },   /* "5": "air2grnd.txt" */
    { 0x00187e54u, 0x00187d3cu },   /* "6": "advanced.txt" */
    { 0x00187e58u, 0x00187e5cu },   /* "7": "king.txt" */
    { 0x00187e68u, 0x00187e6cu },   /* "8": "testa2g.txt" */
};

static void set_score_file(ef_addr32 file)
{
    uint32_t i = 0;

    do
        g_score_file_name[i] = EF_U8(file + i);
    while (EF_U8(file + i++) != 0u);
}

/* 0x7c328, 465 bytes, selection callback of the training-mission list.
 * "first" picks "free.txt" with the mission flag clear; "2".."8" pick
 * their files with the flag set; anything else picks "test.txt". The
 * option word is set, except for "3" (weapon training) which clears it. */
void gpwMissionSelected(ef_addr32 widget)
{
    ef_addr32 name = widget + 0x3fu;
    ef_addr32 file = 0x00187e78u;                   /* "test.txt" */
    size_t i;

    g_mission_option_word = 1u;
    if (guest_strcmp(name, 0x00187dfcu) == 0) {     /* "first" */
        set_score_file(0x00187e04u);                /* "free.txt" */
        g_training_mission_flag = 0;
        return;
    }
    for (i = 0; i < sizeof(training_entries) / sizeof(training_entries[0]); ++i) {
        if (guest_strcmp(name, training_entries[i].name) == 0) {
            file = training_entries[i].file;
            if (training_entries[i].name == 0x00187e20u)
                g_mission_option_word = 0u;
            break;
        }
    }
    g_training_mission_flag = 1;
    set_score_file(file);
}

/* Guest address of the current mission config name. */
#define CONFIG_NAME ((ef_addr32)(uintptr_t)g_config_file_name)

/* Address of row INDEX of the training-mission row pool. */
static ef_addr32 training_row(int16_t index)
{
    return g_training_row_pool + (uint32_t)((int32_t)index * (int32_t)sizeof(ef_session_row));
}

/* Check the "params" widget NAME. */
static void check_params_widget(ef_addr32 name)
{
    guest_fn_a72d8(find_widget_checked_a78bc(find_named_or_abort_a46cc(0x00187e90u), name)); /* "params" */
}

/* 0x7c574, 506 bytes, accept callback of the training-mission list.
 * Select the row ("first" = 0, otherwise the 1-based number in the
 * name), take its config file name, show its text in "miss_info", and
 * read the config's four parameter words. The addon configs are read
 * by bare name, others from "<default path>\cfg\". The hour (second
 * word) checks twilight (6-7, 20), day (8-19) or night; the weather
 * (fourth word) poor (2), fair (1) or good; and "pro" is always
 * checked. */
void gpwMissionAccepted(ef_addr32 widget)
{
    char path[0x50];
    int16_t first = 0;
    int16_t hour = 0;
    int16_t third = 0;
    int16_t weather = 0;
    ef_addr32 name = widget + 0x3fu;
    ef_addr32 dialog;

    if (guest_strcmp(name, 0x00187dfcu) == 0)              /* "first" */
        g_training_selection = 0;
    else
        g_training_selection = (int16_t)(parse_int(name) - 1);
    copy_string_9d749(CONFIG_NAME, training_row(g_training_selection) +
                                          (uint32_t)offsetof(ef_session_row, config_name));
    dialog = find_named_or_abort_a46cc(0x00187e84u);       /* "miss_info" */
    set_session_text_7c4fc(training_row(g_training_selection) +
                               (uint32_t)offsetof(ef_session_row, session_text),
                           dialog);
    guest_fn_a5ef8(EF_U8(dialog + 0x10u));
    if (guest_strcmp(CONFIG_NAME, 0x00187d4cu) == 0 ||   /* "addon1.cfg" */
        guest_strcmp(CONFIG_NAME, 0x00187d78u) == 0 ||   /* "addon2.cfg" */
        guest_strcmp(CONFIG_NAME, 0x00187da4u) == 0 ||   /* "addon3.cfg" */
        guest_strcmp(CONFIG_NAME, 0x00187dd0u) == 0) {   /* "addon4.cfg" */
        copy_string_9d749((ef_addr32)(uintptr_t)path, CONFIG_NAME);
    } else {
        build_default_path_9c980((ef_addr32)(uintptr_t)path);
        guest_fn_9ca57((ef_addr32)(uintptr_t)path, 0x00187ce4u);  /* "\cfg\" */
        guest_fn_9ca57((ef_addr32)(uintptr_t)path, CONFIG_NAME);
    }
    guest_fn_105a4((ef_addr32)(uintptr_t)&first, (ef_addr32)(uintptr_t)&hour,
                   (ef_addr32)(uintptr_t)&third, (ef_addr32)(uintptr_t)&weather,
                   (ef_addr32)(uintptr_t)path);
    if ((hour > 5 && hour < 8) || hour == 20)
        check_params_widget(0x00187e98u);                  /* "ttwilight" */
    else if (hour >= 8 && hour <= 19)
        check_params_widget(0x00187ea4u);                  /* "tday" */
    else
        check_params_widget(0x00187eacu);                  /* "tnight" */
    if (weather == 2)
        check_params_widget(0x00187eb4u);                  /* "wpoor" */
    else if (weather == 1)
        check_params_widget(0x00187ebcu);                  /* "wfair" */
    else
        check_params_widget(0x00187ec4u);                  /* "wgood" */
    check_params_widget(0x00187eccu);                      /* "pro" */
}

/* Widget of DIALOG named by the decimal VALUE. */
static ef_addr32 numbered_widget(ef_addr32 dialog, int32_t value)
{
    char digits[4];

    signed_to_digits_cbd0f(value, (ef_addr32)(uintptr_t)digits, 10u);
    return find_widget_checked_a78bc(dialog, (ef_addr32)(uintptr_t)digits);
}

/* Move the numbered widgets "1".."8" of DIALOG vertically by DY unless
 * widget "1" already sits at row STOP. */
static void shift_slot_rows(ef_addr32 dialog, int16_t stop, int16_t dy)
{
    int16_t i;

    if ((int16_t)EF_U16(find_widget_checked_a78bc(dialog, 0x00187c98u)) == stop) /* "1" */
        return;
    for (i = 1; i < 9; ++i) {
        ef_addr32 row = numbered_widget(dialog, i);

        EF_U16(row) = (uint16_t)(EF_U16(row) + dy);
    }
}

/* 0x7e0a4, 516 bytes, open callback of a custom-mission slot page; the
 * slot number is the widget name at +0x43. Slots 1-2 move the rows of
 * "Custom" up to 100 and slot 3+ down to 120. The "enemysel" list is
 * rebuilt for the slot: the first slot (numbered from the count word
 * 0x1357b0 when the mode byte is 1, else from 1) closes it, the others open it. Then the
 * slot's count, armament and skill widgets and its aircraft type in
 * "enemysel" are checked, and the enemy view is redrawn. */
void simNextFlight(ef_addr32 widget)
{
    ef_addr32 dialog = find_named_or_abort_a46cc(0x00188034u);      /* "Custom" */
    int16_t slot = (int16_t)(parse_int(widget + 0x43u) - 1);
    ef_addr32 skill_name;

    if (slot <= 1) {
        shift_slot_rows(dialog, 100, -20);
        if (slot == 0) {
            place_numbered_widgets_7e01c(g_custom_player_mode == 1u ? g_tactical_lookup_count : 1, 8);
            close_widget_a4b80(EF_U8(find_named_or_abort_a46cc(0x00187f88u) + 0x10u)); /* "enemysel" */
        } else {
            place_numbered_widgets_7e01c(1, 6);
            activate_widget_a4cb4(EF_U8(find_named_or_abort_a46cc(0x00187f88u) + 0x10u));
        }
    } else {
        shift_slot_rows(dialog, 120, 20);
        place_numbered_widgets_7e01c(0, 6);
        activate_widget_a4cb4(EF_U8(find_named_or_abort_a46cc(0x00187f88u) + 0x10u));
    }
    guest_fn_a72d8(numbered_widget(dialog, g_custom_slot_count[slot]));
    guest_fn_a72d8(find_widget_checked_a78bc(dialog, g_custom_slot_guns_only[slot] != 0u
                                                         ? 0x001880b8u      /* "gunsyes" */
                                                         : 0x00188604u));   /* "gunsno" */
    switch (g_custom_slot_skill[slot]) {
    case 0: skill_name = 0x00187eecu; break;                        /* "high" */
    case 1: skill_name = 0x00187fc0u; break;                        /* "medium" */
    case 2: skill_name = 0x00187ee8u; break;                        /* "low" */
    default: skill_name = 0u; break;
    }
    if (skill_name != 0u)
        guest_fn_a72d8(find_widget_checked_a78bc(dialog, skill_name));
    guest_fn_a5ef8(EF_U8(dialog + 0x10u));
    guest_fn_a72d8(numbered_widget(find_named_or_abort_a46cc(0x00187f88u),
                                   g_custom_slot_type[slot] + 1));
    RedrawEnemyView(numbered_widget(find_named_or_abort_a46cc(0x00187f88u),
                                    g_custom_slot_type[slot] + 1));
}
