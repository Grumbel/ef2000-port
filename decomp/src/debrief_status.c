/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define DEBRIEF_SCREEN       0x00187c90u /* "debrief" */
#define CURRENT_MISSION      0x001379c4u /* pointer; +4 word mode, +0xa record pointer */
#define DEBRIEF_GROUP        0x001f8fd6u /* word */
#define DEBRIEF_PERCENT      0x001f8fdau /* word, kept from the lead member */
#define CUSTOM_PLAYER_COUNT  0x0012583au /* byte */
#define CUSTOM_PLAYER_NAMES  0x001a62f0u /* 17-byte names */
#define MISSION_FAILED       0x00121ec0u /* byte */

#define WIDGET_NUMBER        0x3fu /* text */
#define WIDGET_Y             0x45u /* word */

#define DEBRIEF_SCORE_MAX    100

/* 0x7bc40, 679 bytes. Fill the debrief panel for the member named by
 * widget's number: the player name (custom games) and panel height, then
 * the unit tally of that member. The overall score is four points per
 * kill plus the group's destruction percentage, less 20 when the member
 * is missing and 15 when a free entry remains, clamped to 0..100; some
 * single missions use the mission record's own score instead. */
void show_debrief_status_7bc40(ef_addr32 widget)
{
    char text[40];
    int16_t home = 0;
    int16_t kills = 0;
    int16_t percent = 0;
    int16_t ready = 0;
    int16_t missing = 0;
    int16_t member, player, score;
    ef_addr32 record = 0;
    ef_addr32 screen, status;

    if (EF_U32(CURRENT_MISSION) != 0)
        record = EF_U32(EF_U32(CURRENT_MISSION) + 0xau);
    member = (int16_t)(parse_int(widget + WIDGET_NUMBER) - 1);
    player = EF_U16(DEBRIEF_GROUP) != 0 ? (int16_t)(member + 4) : member;

    screen = find_named_or_abort_a46cc(DEBRIEF_SCREEN);
    if (g_custom_player_mode == 1) {
        resize_widget_a520c(screen, (uint32_t)(int32_t)(int16_t)EF_U16(screen + WIDGET_Y), 0xc9, 0);
        if ((int32_t)player <= (int32_t)EF_U8(CUSTOM_PLAYER_COUNT) - 1)
            show_widget_a7588(find_widget_checked_a78bc(screen, 0x00187c9cu),
                              CUSTOM_PLAYER_NAMES + (uint32_t)((int32_t)player * 17));
        else
            show_widget_a7588(find_widget_checked_a78bc(screen, 0x00187c9cu), 0x0013111bu); /* "Computer" */
    } else {
        resize_widget_a520c(screen, (uint32_t)(int32_t)(int16_t)EF_U16(screen + WIDGET_Y), 0xaf, 0);
    }

    tally_unit_records_11560(0, (int16_t)EF_U16(DEBRIEF_GROUP), member, (ef_addr32)(uintptr_t)&kills,
                             (ef_addr32)(uintptr_t)&home, (ef_addr32)(uintptr_t)&percent,
                             (ef_addr32)(uintptr_t)&ready, (ef_addr32)(uintptr_t)&missing);
    if (member == 0)
        EF_U16(DEBRIEF_PERCENT) = (uint16_t)percent;
    percent = (int16_t)EF_U16(DEBRIEF_PERCENT);

    score = (int16_t)(kills * 4 + percent);
    if (missing == 1)
        score = (int16_t)(score - 20);
    if (has_free_entry_117d4())
        score = (int16_t)(score - 15);
    if (score > DEBRIEF_SCORE_MAX)
        score = DEBRIEF_SCORE_MAX;
    else if (score < 0)
        score = 0;

    if (record != 0 && (EF_U8(record) == 0 || EF_U8(record) == 1 || EF_U8(record) == 5)
        && (int16_t)EF_U16(EF_U32(CURRENT_MISSION) + 4u) == 1)
        score = EF_U8(MISSION_FAILED) != 0 ? 0 : (int16_t)EF_U8(record + 0x2bu);

    screen = find_named_or_abort_a46cc(DEBRIEF_SCREEN);
    status = find_widget_checked_a78bc(screen, 0x00187ca4u); /* "status" */
    guest_sprintf_a4342((ef_addr32)(uintptr_t)text, 0x00187cacu, /* "%s" */
                        missing == 1 ? 0x0013210au : 0x00132113u); /* "VERMISST" / "AKTIV" */
    show_widget_a7588(status, (ef_addr32)(uintptr_t)text);
    guest_sprintf_a4342((ef_addr32)(uintptr_t)text, 0x00187cb0u, (int32_t)kills); /* "%d" */
    apply_to_widget_a75b4(screen, 0x00187cb4u, (ef_addr32)(uintptr_t)text); /* "nenemy" */
    guest_sprintf_a4342((ef_addr32)(uintptr_t)text, 0x00187cbcu, (int32_t)percent); /* "%d%%" */
    apply_to_widget_a75b4(screen, 0x00187cc4u, (ef_addr32)(uintptr_t)text); /* "destpercent" */
    guest_sprintf_a4342((ef_addr32)(uintptr_t)text, 0x00187cb0u, (int32_t)ready);
    apply_to_widget_a75b4(screen, 0x00187cd0u, (ef_addr32)(uintptr_t)text); /* "additional" */
    guest_sprintf_a4342((ef_addr32)(uintptr_t)text, 0x00187cbcu, (int32_t)score);
    apply_to_widget_a75b4(screen, 0x00187cdcu, (ef_addr32)(uintptr_t)text); /* "overall" */
}
