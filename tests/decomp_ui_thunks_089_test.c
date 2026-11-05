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

static void test_all_named_jumps(void)
{
    const ef_ui_regs_089 incoming = {0x12345678u, 0xfedcba98u,
                                      0x13579bdfu, 0x2468ace0u};
    unsigned i;
    assert(ef_ui_jump_table_size_089 == 48u);
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

int main(void)
{
    test_all_named_jumps();
    test_quit_wrappers();
    return 0;
}
