/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"

#include <assert.h>
#include <string.h>

static uint8_t mem[0x230000];
static unsigned refreshed, optional_calls, required_calls;
static unsigned opens, selections, binds, flags, destroys;
static ef_addr32 bind_destinations[2], bind_sources[2];

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(mem));
    return mem + address;
}

void guest_refresh_event_slot(int16_t index)
{
    assert(index == (int16_t)refreshed);
    ++refreshed;
}

void guest_dispatch_object_event_optional(ef_addr32 obj, int16_t index)
{
    assert(obj == 0x1d0000 && index == -7);
    ++optional_calls;
}

void guest_dispatch_object_event_required(ef_addr32 obj, int16_t index)
{
    assert(obj == 0x1d0000 && index == -7);
    ++required_calls;
}

ef_addr32 guest_open_named_dialog(ef_addr32 name, ef_addr32 root)
{
    assert(name == 0x184b80 && root == 0x184e00);
    ++opens;
    return 0x1d1000;
}

ef_addr32 guest_select_dialog_widget(ef_addr32 root, ef_addr32 name)
{
    assert(root == 0x1d1000);
    ++selections;
    assert(name == (selections % 2 ? 0x184e00u : 0x184e0cu));
    return selections % 2 ? 0x1d2000 : 0x1d3000;
}

void guest_bind_dialog_widget(ef_addr32 widget, ef_addr32 source)
{
    unsigned index = binds % 2;
    bind_destinations[index] = widget;
    bind_sources[index] = source;
    ++binds;
}

void guest_emit_dialog_flag(uint8_t flag)
{
    assert(flag == 0x7c);
    ++flags;
}

void guest_destroy_named_dialog(ef_addr32 dialog)
{
    assert(dialog == 0x1d1000);
    ++destroys;
}

int main(void)
{
    memset(mem, 0, sizeof(mem));
    refresh_all_event_slots();
    assert(refreshed == 34);

    dispatch_two_object_events(0x1d0000, -7);
    assert(optional_calls == 0 && required_calls == 1);
    mem[0x127da9] = 1;
    dispatch_two_object_events(0x1d0000, -7);
    assert(optional_calls == 1 && required_calls == 2);

    mem[0x1d1010] = 0x7c;
    update_dialog_bindings(0x1e0000, 0x1e1000);
    assert(opens == 1 && selections == 2 && binds == 2);
    assert(flags == 1 && destroys == 1);
    assert(bind_destinations[0] == 0x1d2000 && bind_destinations[1] == 0x1d3000);
    assert(bind_sources[0] == 0x1e0000 && bind_sources[1] == 0x1e1000);

    mem[0x1357b0] = 3;
    mem[0x1a5510] = 5;
    mem[0x1a5511] = 7;
    mem[0x1a5512] = 9;
    mem[0x1a553b + 5] = 1;
    mem[0x1a553b + 7] = 0;
    mem[0x1a553b + 9] = 2;
    flush_active_dialog_slots();
    assert(mem[0x1a553b + 5] == 0 && mem[0x1a553b + 9] == 0);
    assert(opens == 3 && selections == 6 && binds == 6);
    assert(flags == 3 && destroys == 3);
    assert(bind_sources[0] == 0x1323da && bind_sources[1] == 0x1323f6);
    return 0;
}
