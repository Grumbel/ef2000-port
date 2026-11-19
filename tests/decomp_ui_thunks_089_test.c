/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_ui_thunks_089.h"
#include "ef_globals.h"

#include <assert.h>
#include <stdint.h>

static unsigned calls;
static uint32_t last_target;
const char g_quit_confirmation_name[5] = "test";
const char g_quit_notice_text[1] = "";

ef_addr32 apply_a4cb4_a4d90(ef_addr32 name)
{
    assert(calls++ == 0);
    assert(name == (ef_addr32)(uintptr_t)g_quit_confirmation_name);
    return 0x400u;
}

void redraw_widget_ac9a0(ef_addr32 widget)
{
    assert(calls++ == 1);
    assert(widget == 0x400u);
}

void rewrite_settings_830b8(void)
{
    assert(calls++ == 0);
}

void report_fatal_ac7c8(uint32_t text)
{
    assert(calls++ == 1);
    assert(text == (ef_addr32)(uintptr_t)g_quit_notice_text);
}

static void test_quit_wrappers(void)
{
    calls = 0;
    QuitEF2000Yes();
    assert(calls == 2);

    calls = 0;
    QuitEF2000No();
    assert(calls == 2);
}

#define RECOVERED_VOID_JUMPS(X) \
    X(guiClosedownHelp, draw_named_widget_abc98, 0xabc98u) \
    X(UpdateMagnify, refresh_named_widget_abce8, 0xabce8u) \
    X(gnStartup, update_mission_page_34f94, 0x34f94u) \
    X(gnClosedown, start_session_34ca4, 0x34ca4u) \
    X(rxmissStartup, end_session_34e18, 0x34e18u) \
    X(gnNetAbort, forward_if_flag_1357c8, 0x33750u) \
    X(rxmissMainloop, reset_flags_35144, 0x35144u) \
    X(rxmissClosedown, step_menu_screen_35590, 0x35590u) \
    X(txmissStartup, start_screen_357a4, 0x357a4u) \
    X(txmissMainloop, prepare_briefing_file_35cfc, 0x35cfcu) \
    X(txmissClosedown, step_setup_screen_36f84, 0x36f84u) \
    X(gnReadyToFly, reset_list_view_3711c, 0x3711cu) \
    X(gpwWeaponsAccepted, rebuild_tactical_screen_69fac, 0x69facu) \
    X(gpwGenerateCampaign, copy_table_689f0, 0x689f0u) \
    X(gpwClickCampaign, reset_name_record_40e20, 0x40e20u) \
    X(InitSim2, show_main_widgets_7cc5c, 0x7cc5cu) \
    X(simShowDebrief, run_named_widget_7cc38, 0x7cc38u) \
    X(gpwContinueArcade, reset_menu_state_4738, 0x4738u) \
    X(InitArcade2, select_entry_4bf8, 0x4bf8u) \
    X(InitPickMiss, setup_dialog_41028, 0x41028u) \
    X(arcNextDifficulty, enter_hiscore_screen_4e14, 0x4e14u) \
    X(gpwTestClicked, finish_trace_c594, 0xc594u) \
    X(gpwClickTimeAdjust, apply_two_a4b80_b544, 0xb544u) \
    X(gpwCloseDebrief, refresh_status_widgets_d550, 0xd550u) \
    X(gpwNextFLight, emit_and_advance_d628, 0xd628u) \
    X(QuitEF2000, update_panel_pages_42e8c, 0x42e8cu) \
    X(sumaircraftgraph, draw_box_rows_99ee4, 0x99ee4u) \
    X(sumgroundgraph, draw_box_rows_99fe0, 0x99fe0u) \
    X(sumstaticgraph, draw_box_rows_9a0dc, 0x9a0dcu) \
    X(sumgenbar, draw_box_rows_9a1d8, 0x9a1d8u) \
    X(sumgencamp, draw_hud_frame_9bba4, 0x9bba4u) \
    X(openservicesell, update_item_dialog_34298, 0x34298u) \
    X(entergamename, show_notice_34390, 0x34390u) \
    X(simrecallmiss, toggle_named_widget_7bc00, 0x7bc00u) \
    X(gpwUpdateTime, bind_page_widgets_42f7c, 0x42f7cu) \
    X(InitCustom, update_flight_options_widgets_7e2a8, 0x7e2a8u) \
    X(ChooseEnemy, finish_enemy_selection_7ccdc, 0x7ccdcu) \
    X(gpwExitSimulator, close_simulator_7d2f8, 0x7d2f8u) \
    X(onopenParams, open_simulator_parameters_7c770, 0x7c770u) \
    X(gpwSelectLevel, select_arcade_level_494c, 0x494cu) \
    X(kotsClickTeam, select_kots_team_4f18, 0x4f18u) \
    X(InitCampaign, initialize_campaign_5630, 0x5630u) \
    X(gpwLoadSaveCampaign, load_saved_campaign_bfcc, 0xbfccu) \
    X(gpwExitCampaign, exit_campaign_b1e0, 0xb1e0u) \
    X(onopenDirectPlaySettings, open_directplay_settings_98bec, 0x98becu)

#define STUB_TARGET(name, target, address) \
    void target(void) { ++calls; last_target = address; }
RECOVERED_VOID_JUMPS(STUB_TARGET)
#undef STUB_TARGET

uint32_t call_b1a10_1379bc(void)
{
    ++calls;
    return 0x12345678u;
}

static void test_recovered_jumps(void)
{
#define CHECK_TARGET(name, target, address) \
    calls = 0; \
    name(); \
    assert(calls == 1); \
    assert(last_target == address);
    RECOVERED_VOID_JUMPS(CHECK_TARGET)
#undef CHECK_TARGET

    calls = 0;
    assert(gpwSimTypeSelected() == 0x12345678u);
    assert(calls == 1);
}

int main(void)
{
    test_recovered_jumps();
    test_quit_wrappers();

    calls = 0;
    gpwRedrawMap();
    gcClickMap();
    assert(calls == 0);
    return 0;
}
