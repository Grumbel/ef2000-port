/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_mem.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t ram[0x500000];
struct Call { uint32_t address, eax, edx; };
static struct Call events[64];
static unsigned event_count;
static uint32_t availability;

void *ef_guest_memory_ptr(ef_addr32 addr)
{
    assert(addr < sizeof(ram));
    return &ram[addr];
}

static void record(uint32_t address, uint32_t eax, uint32_t edx)
{
    assert(event_count < sizeof(events) / sizeof(events[0]));
    events[event_count].address = address;
    events[event_count].eax = eax;
    events[event_count++].edx = edx;
}

uint32_t gc_ui_guest_call_085(uint32_t entry, uint32_t eax, uint32_t edx)
{
    record(entry, eax, edx);
    if (entry == 0xabcdfu)
        return availability;
    if (entry == 0x9d4cdu)
        return 0x42u;
    if (entry == 0xa46ccu)
        return 0x1000u;
    if (entry == 0xa78bcu)
        return edx == 0x184388u ? 0x2030u : 0x2010u;
    return eax;
}

uint32_t gpw_ui_guest_call_085(uint32_t entry, uint32_t eax, uint32_t edx)
{
    record(entry, eax, edx);
    if (entry == 0xa78bcu)
        return 0x1000u;
    return eax;
}

static void reset(void)
{
    memset(ram, 0, sizeof(ram));
    memset(events, 0, sizeof(events));
    event_count = 0;
    availability = 0;
}

static void clear_calls(void)
{
    event_count = 0;
    memset(events, 0, sizeof(events));
}

static void expect(unsigned i, uint32_t addr, uint32_t eax, uint32_t edx)
{
    assert(i < event_count);
    assert(events[i].address == addr);
    assert(events[i].eax == eax);
    assert(events[i].edx == edx);
}

static void test_campaign_mode(void)
{
    unsigned i;
    reset();
    gc_start_new_campaign_40ebc();
    assert(EF_U8(0x1a364cu) == 1u);
    gc_performance_yes_40ec4();
    assert(EF_U8(0x1a364cu) == 0u);
    gc_performance_no_40ed0(0x300u);
    expect(0, 0x9d4cdu, 0x33fu, 0u);
    assert(EF_U8(0x1264e2u) == 0x42u);
    assert(EF_U8(0x12584au) == 0x42u);
    clear_calls();

    gc_set_difficulty_40e50();
    assert(EF_U8(0x1a364bu) == 0x32u);
    assert(EF_U8(0x1a364cu) == 1u);
    assert(EF_U8(0x12584au) == 6u);
    assert(event_count == 9);
    expect(0, 0xa4c84u, 0, 0);
    expect(1, 0xa4d90u, 0x18436cu, 0);
    expect(2, 0xa4d90u, 0x184374u, 0);
    expect(3, 0xa46ccu, 0x184374u, 0x184384u);
    expect(4, 0xa78bcu, 0x1000u, 0x184384u);
    expect(5, 0xa72d8u, 0x2010u, 0x184384u);
    expect(6, 0xa78bcu, 0x1000u, 0x184388u);
    expect(7, 0xa7380u, 0x2030u, 0x2030u);
    expect(8, 0xa72d8u, 0x2030u, 0x0630u);

    clear_calls();
    EF_U32(0x132ab4u) = 0xabcdfu;
    availability = 1;
    gc_set_campaign_difficulty_40f9c();
    assert(event_count == 4);
    expect(0, 0xa4c84u, 0, 0);
    expect(1, 0xabcdfu, 0x18439cu, 0);
    expect(2, 0xa4d90u, 0x18436cu, 0);
    expect(3, 0xa4d90u, 0x1843a8u, 0);
    EF_U8(0x12584au) = 3;
    clear_calls();
    availability = 0;
    gc_set_campaign_difficulty_40f9c();
    assert(event_count == 10);
    expect(2, 0xa4d90u, 0x184374u, 0);
    expect(9, 0xc9fc4u, 0, 0);
    assert(EF_U8(0x12584au) == 6u);

    reset();
    for (i = 0; i < 12; ++i) {
        EF_U8(0x184358u + i) = (uint8_t)(i + 0xa0u);
        EF_U8(0x18439cu + i) = (uint8_t)(i + 0x50u);
    }
    gc_load_old_campaign_40ee4();
    assert(event_count == 3);
    expect(0, 0xa4c84u, 0, 0);
    expect(1, 0xa4d90u, 0x18438cu, 1u);
    expect(2, 0xc9fc4u, 0, 1u);
    assert(EF_U16(0x137992u) == 1u);
    assert(memcmp(&ram[0x41f30c], &ram[0x184358], 12) == 0);
    clear_calls();
    gc_init_main_menu_40f64();
    assert(event_count == 1);
    expect(0, 0xac7a8u, 0x184364u, 0);
    assert(EF_U16(0x137992u) == 0u);
    assert(EF_U16(0x137994u) == 1u);
    assert(memcmp(&ram[0x41f30c], &ram[0x18439c], 12) == 0);
    clear_calls();
    gc_loadfile_accepted_40f18();
    assert(event_count == 4);
    expect(0, 0xa4db0u, 0x18436cu, 0);
    expect(1, 0xac7a8u, 0x184364u, 0);
    expect(2, 0xaca88u, 0, 0);
    expect(3, 0xc9fc4u, 0, 0);
    assert(EF_U16(0x137992u) == 0u && EF_U16(0x137994u) == 1u);
    assert(memcmp(&ram[0x41f30c], &ram[0x18439c], 12) == 0);
}

static void test_view_presets(void)
{
    reset();
    gpw_plane_side_on_68f40();
    assert(EF_U16(0x1f6f48u) == 0x730u);
    assert(EF_U16(0x1f6f3cu) == 0x810u);
    assert(EF_U16(0x1f6f3eu) == 0x8001u);
    gpw_plane_underneath_68f6c();
    assert(EF_U16(0x1f6f48u) == 0xbe0u);
    assert(EF_U16(0x1f6f3cu) == 0x820u);
    assert(EF_U16(0x1f6f3eu) == 0x4000u);
    gpw_plane_angled_68f98();
    assert(EF_U16(0x1f6f48u) == 0xe38u);
    assert(EF_U16(0x1f6f3cu) == 0x4000u);
    gpw_use_defaults_68fbc();
    assert(EF_U16(0x1f6f48u) == 0xe38u);
    assert(EF_U16(0x1f6f3eu) == 0x4000u);
    gpw_place_missile_68fe0();
    assert(EF_U16(0x1f6f48u) == 0xaf0u);
    assert(EF_U16(0x1f6f3cu) == 0x1920u);
    assert(EF_U16(0x1f6f3eu) == 0x66e0u);

    gpw_set_clear_missile_6931c();
    assert(EF_U16(0x1282d8u) == 1u && EF_U16(0x1282dau) == 0u);
    gpw_set_clear_all_missiles_69338();
    assert(EF_U16(0x1282d8u) == 0u && EF_U16(0x1282dau) == 1u);
    gpw_show_current_69914();
    assert(EF_U16(0x12830eu) == 1u);
}

static void test_view_rotation_and_weapon(void)
{
    unsigned i;
    reset();
    gpw_check_hit_pylon_69354();
    assert(event_count == 13);
    for (i = 0; i < event_count; ++i)
        expect(i, 0x68c18u, 0u, i);
    clear_calls();

    EF_U16(0x200u + 0xcu) = 0x0010u;
    EF_U16(0x1f6f3cu) = 0x1920u;
    gpw_turn_y_plane_69920(0x200u);
    assert(EF_U16(0x1f6f3cu) == 0x1920u); /* +0x600 then -0x600 */
    EF_U16(0x200u + 0xcu) = 0x0400u;
    gpw_turn_y_plane_69920(0x200u);
    assert(EF_U16(0x1f6f3cu) == 0x1f20u);
    EF_U16(0x200u + 0xcu) = 0x4000u;
    gpw_turn_y_plane_69920(0x200u);
    assert(EF_U16(0x1f6f3cu) == 0x1920u);
    EF_U16(0x200u + 0xcu) = 0;
    gpw_turn_y_plane_69920(0x200u);
    assert(EF_U16(0x1f6f3cu) == 0x1920u);

    EF_U16(0x1f6f3eu) = 0x0100u;
    EF_U16(0x200u + 0xcu) = 0x0400u;
    gpw_arcade_accepted_69978(0x200u);
    assert(EF_U16(0x1f6f3eu) == 0x0700u);
    EF_U16(0x200u + 0xcu) = 0x4000u;
    gpw_arcade_accepted_69978(0x200u);
    assert(EF_U16(0x1f6f3eu) == 0x0100u);

    EF_U32(0x1000u + 0x27u) = 0x2100u;
    EF_U8(0x2100u) = 0x7fu;
    EF_U16(0x1f6f4au) = 0x5555u;
    gpw_open_big_current_weapon_699d4(0x300u);
    assert(event_count == 1);
    expect(0, 0xa78bcu, 0x300u, 0x18630cu);
    assert(EF_U8(0x2100u) == 0u && EF_U16(0x1f6f4au) == 0u);
}

int main(void)
{
    test_campaign_mode();
    test_view_presets();
    test_view_rotation_and_weapon();
    return 0;
}
