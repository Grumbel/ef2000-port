/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_ui_thunks_089.h"

/* A five-byte JMP inherits its target's ABI. Use the named target and
 * the same input and return types, then check that OpenWatcom emits just
 * one tail jump to it. Keep ABI and tracker entries in step with the C. */

/* 0x84ac4, 15 bytes. Activate the confirmation widget and redraw it. */
void QuitEF2000Yes(void)
{
    ef_addr32 widget = apply_a4cb4_a4d90((ef_addr32)(uintptr_t)g_quit_confirmation_name);
    redraw_widget_ac9a0(widget);
}

/* 0x84aec, 15 bytes. Save the settings before reporting the quit message. */
void QuitEF2000No(void)
{
    rewrite_settings_830b8();
    report_fatal_ac7c8((ef_addr32)(uintptr_t)g_quit_notice_text);
}

/* 0x84afc, 5 bytes. */
void guiClosedownHelp(void)
{
    draw_named_widget_abc98();
}

/* 0x84b04, 5 bytes. */
void UpdateMagnify(void)
{
    refresh_named_widget_abce8();
}

/* 0x84b0c, 5 bytes. */
void gnStartup(void)
{
    update_mission_page_34f94();
}

/* 0x84b14, 5 bytes. */
void gnClosedown(void)
{
    start_session_34ca4();
}

/* 0x84b1c, 5 bytes. */
void rxmissStartup(void)
{
    end_session_34e18();
}

/* 0x84b24, 5 bytes. */
void gnNetAbort(void)
{
    forward_if_flag_1357c8();
}

/* 0x84b2c, 5 bytes. */
void rxmissMainloop(void)
{
    reset_flags_35144();
}

/* 0x84b34, 5 bytes. */
void rxmissClosedown(void)
{
    step_menu_screen_35590();
}

/* 0x84b3c, 5 bytes. */
void txmissStartup(void)
{
    start_screen_357a4();
}

/* 0x84b44, 5 bytes. */
void txmissMainloop(void)
{
    prepare_briefing_file_35cfc();
}

/* 0x84b4c, 5 bytes. */
void txmissClosedown(void)
{
    step_setup_screen_36f84();
}

/* 0x84b54, 5 bytes. */
void gnReadyToFly(void)
{
    reset_list_view_3711c();
}

/* 0x84b5c, 5 bytes. */
void gpwWeaponsAccepted(void)
{
    rebuild_tactical_screen_69fac();
}

/* 0x84b64, 5 bytes. */
void gpwGenerateCampaign(void)
{
    copy_table_689f0();
}

/* 0x84b6c, 5 bytes. */
void gpwClickCampaign(void)
{
    reset_name_record_40e20();
}

/* 0x84b74, 5 bytes. */
void InitPickMiss(void)
{
    setup_dialog_41028();
}

/* 0x84b7c, 5 bytes. */
void InitSim2(void)
{
    show_main_widgets_7cc5c();
}

/* 0x84b84, 5 bytes. */
void simShowDebrief(void)
{
    run_named_widget_7cc38();
}

/* 0x84b8c, 5 bytes. */
void ChooseEnemy(void)
{
    finish_enemy_selection_7ccdc();
}

/* 0x84b94, 5 bytes. */
void gpwExitSimulator(void)
{
    close_simulator_7d2f8();
}

/* 0x84b9c, 5 bytes. */
uint32_t gpwSimTypeSelected(void)
{
    return call_b1a10_1379bc();
}

/* 0x84ba4, 5 bytes. */
void onopenParams(void)
{
    open_simulator_parameters_7c770();
}

/* 0x84bac, 5 bytes. */
void gpwContinueArcade(void)
{
    reset_menu_state_4738();
}

/* 0x84bb4, 5 bytes. */
void InitArcade2(void)
{
    select_entry_4bf8();
}

/* 0x84bbc, 5 bytes. */
void gpwSelectLevel(void)
{
    select_arcade_level_494c();
}

/* 0x84bc4, 5 bytes. */
void arcNextDifficulty(void)
{
    enter_hiscore_screen_4e14();
}

/* 0x84bcc, 5 bytes. */
void kotsClickTeam(void)
{
    select_kots_team_4f18();
}

/* 0x84bd4, 5 bytes. */
void InitCampaign(void)
{
    initialize_campaign_5630();
}

/* 0x84bdc, 5 bytes. */
void gpwLoadSaveCampaign(void)
{
    load_saved_campaign_bfcc();
}

/* 0x84be4, 5 bytes. The original target 0xa9a4 immediately returns. */
void gpwRedrawMap(void)
{
}

/* 0x84bec, 5 bytes. */
void gpwExitCampaign(void)
{
    exit_campaign_b1e0();
}

/* 0x84bf4, 5 bytes. */
void gpwTestClicked(void)
{
    finish_trace_c594();
}

/* 0x84bfc, 5 bytes. */
void gpwClickTimeAdjust(void)
{
    apply_two_a4b80_b544();
}

/* 0x84c04, 5 bytes. */
void gpwCloseDebrief(void)
{
    refresh_status_widgets_d550();
}

/* 0x84c0c, 5 bytes. */
void gpwNextFLight(void)
{
    emit_and_advance_d628();
}

/* 0x84c14, 5 bytes. The original target 0xe48c immediately returns. */
void gcClickMap(void)
{
}

/* 0x84c1c, 5 bytes. */
void QuitEF2000(void)
{
    update_panel_pages_42e8c();
}

/* 0x84c24, 5 bytes. */
void onopenDirectPlaySettings(void)
{
    open_directplay_settings_98bec();
}

/* 0x84c2c, 5 bytes. */
void sumaircraftgraph(void)
{
    draw_box_rows_99ee4();
}

/* 0x84c34, 5 bytes. */
void sumgroundgraph(void)
{
    draw_box_rows_99fe0();
}

/* 0x84c3c, 5 bytes. */
void sumstaticgraph(void)
{
    draw_box_rows_9a0dc();
}

/* 0x84c44, 5 bytes. */
void sumgenbar(void)
{
    draw_box_rows_9a1d8();
}

/* 0x84c54, 5 bytes. */
void sumgencamp(void)
{
    draw_hud_frame_9bba4();
}

/* 0x84c5c, 5 bytes. */
void openservicesell(void)
{
    update_item_dialog_34298();
}

/* 0x84c64, 5 bytes. */
void entergamename(void)
{
    show_notice_34390();
}

/* 0x84c6c, 5 bytes. */
void simrecallmiss(void)
{
    toggle_named_widget_7bc00();
}

/* 0x84c74, 5 bytes. */
void gpwUpdateTime(void)
{
    bind_page_widgets_42f7c();
}

/* 0x84c7c, 5 bytes. */
void InitCustom(void)
{
    update_flight_options_widgets_7e2a8();
}

/* 0x84c4c jumps to 0x9bcf8. Its callback entry remains outside the
 * canonical inventory pending caller and boundary evidence. */
