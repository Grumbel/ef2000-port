/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_ui_thunks_089.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static unsigned calls;
static uint32_t last_target;
static ef_ui_regs_089 last_regs;

ef_ui_regs_089 ef_ui_jump_guest_089(uint32_t target, ef_ui_regs_089 registers)
{
    ++calls;
    last_target = target;
    last_regs = registers;
    registers.eax ^= 0x3a5ac003u;
    registers.edx += 17u;
    return registers;
}

static void test_pending_jump_models(void)
{
    const ef_ui_regs_089 incoming = {0x12345678u, 0xfedcba98u,
                                      0x13579bdfu, 0x2468ace0u};
    unsigned i;
    assert(ef_ui_jump_table_size_089 == 28u);
    for (i = 0; i < ef_ui_jump_table_size_089; ++i) {
        ef_ui_regs_089 output;
        const ef_ui_jump_entry_089 *entry = &ef_ui_jump_table_089[i];
        assert(strlen(entry->original_name) != 0u);
        calls = 0u;
        output = entry->model(incoming);
        assert(calls == 1u);
        assert(last_target == entry->jump_target);
        assert(memcmp(&last_regs, &incoming, sizeof(incoming)) == 0);
        assert(output.eax == (incoming.eax ^ 0x3a5ac003u));
        assert(output.edx == incoming.edx + 17u);
        assert(output.ebx == incoming.ebx);
        assert(output.ecx == incoming.ecx);
    }
}

static void test_quit_wrappers(void)
{
    ef_ui_regs_089 registers = {0x55555u, 42u, 13u, 27u};
    calls = 0u;
    registers = ef_ui_quit_yes_84ac4(registers);
    assert(calls == 2u);
    assert(last_target == 0xac9a0u);
    assert(last_regs.eax == (0x189b78u ^ 0x3a5ac003u));
    assert(registers.edx == 42u + 34u);

    registers = (ef_ui_regs_089){0x55555u, 42u, 13u, 27u};
    calls = 0u;
    registers = ef_ui_quit_no_84aec(registers);
    assert(calls == 2u);
    assert(last_target == 0xac7c8u);
    assert(last_regs.eax == 0x189b5cu);
    assert(registers.edx == 42u + 34u);
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
    X(InitArcade2, select_entry_4bf8, 0x4bf8u)

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
    test_pending_jump_models();
    test_recovered_jumps();
    test_quit_wrappers();
    return 0;
}
