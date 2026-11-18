/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_ui_thunks_089.h"

/* A five-byte JMP inherits its target's ABI. Use the named target and
 * the same input and return types, then check that OpenWatcom emits just
 * one tail jump to it. Keep ABI and tracker entries in step with the C. */

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

/* 0x84b9c, 5 bytes. */
uint32_t gpwSimTypeSelected(void)
{
    return call_b1a10_1379bc();
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

#ifdef EF_DECOMP_HOST_TEST
/* Every original five-byte body is a JMP rel32. A host-only adapter models
 * this as a guest tail call with the incoming register tuple unchanged.
 * The target is responsible for modifying registers and RAM. */
#define EF_DEFINE_UI_JUMP_089(name, addr, target) \
    ef_ui_regs_089 ef_ui_jump_##name##_##addr(ef_ui_regs_089 registers) \
    { return ef_ui_jump_guest_089(0x##target##u, registers); }
EF_UI_JUMPS_089(EF_DEFINE_UI_JUMP_089)
#undef EF_DEFINE_UI_JUMP_089

#define EF_REGISTER_UI_JUMP_089(name, addr, target) \
    { 0x##addr##u, 0x##target##u, #name, ef_ui_jump_##name##_##addr },
const ef_ui_jump_entry_089 ef_ui_jump_table_089[] = {
    EF_UI_JUMPS_089(EF_REGISTER_UI_JUMP_089)
};
#undef EF_REGISTER_UI_JUMP_089

const unsigned ef_ui_jump_table_size_089 =
    (unsigned)(sizeof(ef_ui_jump_table_089) / sizeof(ef_ui_jump_table_089[0]));

/* QuitEF2000Yes 0x84ac4 (15 bytes): guest 0xa4d90(0x189b78), then
 * terminal JMP 0xac9a0. No first-call EAX value reaches the second call. */
ef_ui_regs_089 ef_ui_quit_yes_84ac4(ef_ui_regs_089 registers)
{
    registers.eax = 0x189b78u;
    registers = ef_ui_jump_guest_089(0xa4d90u, registers);
    return ef_ui_jump_guest_089(0xac9a0u, registers);
}

/* QuitEF2000No 0x84aec (15 bytes): guest 0x830b8; then load EAX and
 * terminal JMP 0xac7c8. */
ef_ui_regs_089 ef_ui_quit_no_84aec(ef_ui_regs_089 registers)
{
    registers = ef_ui_jump_guest_089(0x830b8u, registers);
    registers.eax = 0x189b5cu;
    return ef_ui_jump_guest_089(0xac7c8u, registers);
}
#endif
