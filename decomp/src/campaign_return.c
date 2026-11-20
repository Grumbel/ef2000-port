/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define NAME_MISSION_SELECT   0x00180550u /* "missionsel" */
#define NAME_MISSION_BRIEF    0x0018060cu /* "missionbrief" */
#define NAME_MISSION_OVERVIEW 0x00180584u /* "missionoverview" */
#define NAME_MAP              0x00180850u /* "map" */
#define NAME_GENERATE_CAMP    0x00180854u /* "generatecamp" */
#define NAME_WAR_WON          0x00180864u /* "warwon" */
#define NAME_WAR_LOST         0x00180878u /* "warlost" */
#define NAME_CAMPAIGN_FILE    0x0018086cu /* "campaign.cg" */
#define NAME_ONE              0x00180580u /* "1" */
#define NAME_TIME_ADJUST      0x00180880u /* "timeadjust" */
#define NAME_VIEW_EIGHT       0x0018088cu /* "vieweight" */

#define WIDGET_INDEX(widget)  ((uint32_t)EF_U8((widget) + 0x10u))

/* 0xba5c, 445 bytes. Return from a mission to the campaign screens.
 * Close the campaign generator, refresh the campaign state and either
 * show the war won or lost screen, or rebuild the node pools, reopen the
 * mission select screen and its overview pages. When no target window or
 * input is pending, a difference above the lookup count (0x50770) shows
 * the 0x507dc widget; otherwise "vieweight" of "timeadjust" is checked to
 * exist and the pilots window closed. Called by 0x42ef0. */
void return_to_campaign_ba5c(void)
{
    ef_addr32 mission_select = find_named_or_abort_a46cc(NAME_MISSION_SELECT);
    ef_addr32 overview;
    ef_addr32 map;
    ef_addr32 generator;

    find_named_or_abort_a46cc(NAME_MISSION_BRIEF);
    overview = find_named_or_abort_a46cc(NAME_MISSION_OVERVIEW);
    map = find_named_or_abort_a46cc(NAME_MAP);
    generator = find_named_or_abort_a46cc(NAME_GENERATE_CAMP);
    activate_widget_a4cb4((uint16_t)WIDGET_INDEX(generator));
    guest_fn_c9fc4();
    release_parser_buffers();
    rebuild_tables_ca7e0();
    guest_fn_b0f5c();

    if (EF_U16(0x00137998u) != 0u) {
        set_pending_name_ac7a8(NAME_WAR_WON);
        tail_jump_cbc9b(NAME_CAMPAIGN_FILE);
        return;
    }
    if (EF_U16(0x0013799au) != 0u) {
        set_pending_name_ac7a8(NAME_WAR_LOST);
        tail_jump_cbc9b(NAME_CAMPAIGN_FILE);
        return;
    }

    guest_fn_94fc();
    g_target_group_index = g_campaign_target_choices[0].group;
    g_target_member_index = g_campaign_target_choices[0].member;
    g_reference_mirror[2] = (uint8_t)g_target_member_index;
    EF_U16((ef_addr32)(uintptr_t)g_reference_mirror) = (uint16_t)g_target_group_index;
    g_reference_mirror[3] = 0u;
    run_named_widget_a5bc();
    guest_fn_9820();
    guest_fn_9ea0();
    release_pointer_b1a10(0x001379bcu);
    g_active_record_pool = allocate_weighted_node_records(0x00137970u, 0x001379a8u);
    release_pointer_b1a3c(0x001379c0u);
    EF_U32(0x001379c0u) = alloc_completion_array_b00d0(0x00137960u, 0x001379acu);
    enable_widget_a72d8(find_widget_checked_a78bc(find_named_or_abort_a46cc(NAME_MISSION_SELECT), NAME_ONE));
    clear_widget_words_a56f8(mission_select);
    apply_a5ef8_a6018(NAME_MISSION_SELECT);
    close_widget_a4b80(WIDGET_INDEX(generator));
    activate_widget_a4cb4((uint16_t)WIDGET_INDEX(overview));
    activate_widget_a4cb4((uint16_t)WIDGET_INDEX(map));

    if (g_target_window_open != 0 || g_runtime_input_byte_c != 0u)
        return;
    if ((int16_t)max_difference_50770() > g_tactical_lookup_count) {
        refresh_widget_507dc();
        return;
    }
    find_widget_checked_a78bc(find_named_or_abort_a46cc(NAME_TIME_ADJUST), NAME_VIEW_EIGHT);
    gpwClosePilots();
}
