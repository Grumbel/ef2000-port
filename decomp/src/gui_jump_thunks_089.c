/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_ui_thunks_089.h"

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
