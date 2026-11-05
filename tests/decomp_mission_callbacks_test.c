/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_mem.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t ram[0x200000];
struct Call { uint32_t target, eax, edx, ebx; };
static struct Call calls[64];
static unsigned count;
static uint8_t cascade, change_index;

void *ef_guest_memory_ptr(ef_addr32 addr)
{
    assert(addr < sizeof(ram));
    return &ram[addr];
}

uint32_t mp_ui_guest_call_084(uint32_t target, uint32_t eax, uint32_t edx)
{
    assert(count < sizeof(calls) / sizeof(calls[0]));
    calls[count].target = target;
    calls[count].eax = eax;
    calls[count].edx = edx;
    calls[count++].ebx = 0u;
    if (cascade && target == 0x4bc80u)
        EF_U8(0x1a53dcu) = 2;
    if (change_index && target == 0xa7434u)
        EF_U8(0x1a5598u) = 3;
    if (target == 0xa46ccu) {
        switch (eax) {
        case 0x184b80u: return 0x400u;
        case 0x184e18u: return 0x500u;
        case 0x184ec4u: return 0x600u;
        case 0x184eccu: return 0x700u;
        case 0x184af0u: return 0x800u;
        case 0x184a8cu: return 0x900u;
        case 0x184b1cu: return 0xa00u;
        default: assert(0 && "unknown name");
        }
    }
    return eax;
}

void mp_ui_guest_map_select_084(uint32_t target, uint32_t eax,
                                uint32_t edx, uint32_t ebx)
{
    assert(count < sizeof(calls) / sizeof(calls[0]));
    calls[count].target = target;
    calls[count].eax = eax;
    calls[count].edx = edx;
    calls[count++].ebx = ebx;
}

static void clear(void)
{
    memset(ram, 0, sizeof(ram));
    memset(calls, 0, sizeof(calls));
    count = 0;
    cascade = change_index = 0;
}

static void calls_only(void)
{
    memset(calls, 0, sizeof(calls));
    count = 0;
}

static void expected(unsigned index, uint32_t target, uint32_t eax, uint32_t edx)
{
    assert(index < count);
    assert(calls[index].target == target);
    assert(calls[index].eax == eax);
    assert(calls[index].edx == edx);
}

static void test_map_modes(void)
{
    unsigned i;
    clear();
    for (i = 0; i != 5u; ++i) {
        EF_U8(0x1a53dcu) = (uint8_t)i;
        calls_only();
        mp_zoom_in_map_4bc40(0x2468u);
        assert(count == (i == 1u || i == 2u || i == 4u ? 1u : 0u));
        if (count)
            expected(0, i == 1u ? 0x4bc80u : (i == 2u ? 0x4be88u : 0x4dcd8u), 0x2468u, 0);
    }
    EF_U8(0x1a53dcu) = 1;
    cascade = 1;
    calls_only();
    mp_zoom_in_map_4bc40(0x55);
    assert(count == 2);
    expected(1, 0x4be88u, 0x55u, 0u);

    clear();
    EF_U8(0x1a53dcu) = 4u;
    mp_map_select_4bef0();
    assert(count == 2);
    expected(0, 0x4bf1cu, 0x137980u, 0x137988u);
    assert(calls[0].ebx == 0x137960u);
    expected(1, 0x4e360u, 0, 0);

    calls_only();
    EF_U8(0x1a54f3u) = 1;
    mp_redraw_map_4bec0(0x1234u);
    assert(count == 3);
    expected(0, 0xe5088u, 0, 0);
    expected(1, 0x4bf1cu, 0x137980u, 0x137988u);
    expected(2, 0x4e360u, 0, 0);
    assert(EF_U8(0x1a54f3u) == 0);
    calls_only();
    EF_U8(0x1a53dcu) = 1;
    mp_redraw_map_4bec0(0u);
    assert(count == 0);
    EF_U8(0x1a559eu) = 1;
    mp_redraw_map_4bec0(0u);
    assert(count == 2);

    clear();
    EF_U16(0x12656au) = 1u;
    mp_cycle_map_4c610();
    assert(EF_U16(0x12656au) == 0u && EF_U8(0x12654cu) == 1u);
    assert(EF_U8(0x1a54f3u) == 1u);
    EF_U16(0x12656au) = 3u;
    mp_cycle_map_4c610();
    assert(EF_U16(0x12656au) == (uint16_t)-2);

    clear();
    for (i = 0; i < 4; ++i) {
        mp_ewr_coverage_4c638();
        mp_toggle_flags_4c66c();
        assert(EF_U8(0x1a559du) == (uint8_t)((i + 1) % 3));
        assert(EF_U8(0x1a559eu) == (uint8_t)((i + 1) % 3));
    }
    EF_U8(0x1a559du) = 0xfe;
    mp_ewr_coverage_4c638();
    assert(EF_U8(0x1a559du) == 0xffu);
    mp_ewr_coverage_4c638();
    assert(EF_U8(0x1a559du) == 0u);
}

static void test_waypoints_and_speed(void)
{
    unsigned i;
    clear();
    EF_U8(0x1a5672u) = 0x44u;
    EF_U32(0x1a560au) = 0x12345678u;
    EF_U8(0x1a5672u + 25u) = 0x55u;
    mp_redraw_wp_heights_4e768();
    assert(count == 1);
    expected(0, 0x4e7b8u, 0, 0);
    assert(EF_U8(0x1a5672u) == 0x44u);
    assert(EF_U32(0x1a560au) == 0x12345678u);
    assert(EF_U8(0x1a5672u + 25u) == 0x55u);
    for (i = 1; i < 25; ++i) {
        assert(EF_U8(0x1a5672u + i) == 0x11u);
        assert(EF_U32(0x1a560au + i * 4u) == 1000u);
    }
    assert(EF_U8(0x1a54f3u) == 1);
    assert(EF_U8(0x1a559fu) == 1);
    assert(EF_U8(0x1a5708u) == 0 && EF_U8(0x1a5709u) == 0);

    clear();
    mp_clear_wp_route_4dae8();
    assert(count == 1);
    expected(0, 0x4f314u, 0x132351u, 0x132373u);
    calls_only();
    EF_U8(0x1a5532u) = 1;
    mp_clear_wp_route_4dae8();
    assert(count == 0);
    EF_U32(0x1a54f6u) = 1;
    EF_U8(0x1a5598u) = 2;
    EF_U8(0x1a5544u + 2u) = 0xffu;
    mp_clear_wp_route_4dae8();
    assert(count == 0);
    EF_U8(0x1a5544u + 2u) = 9;
    EF_U32(0x1a5558u + 2u * 4u) = 0x111u;
    EF_U32(0x1a5558u + 3u * 4u) = 0x222u;
    change_index = 1;
    mp_clear_wp_route_4dae8();
    assert(count == 3 && EF_U8(0x1a5540u) == 9u);
    expected(0, 0xa46ccu, 0x184b1cu, 0u);
    expected(1, 0xa7434u, 0xa00u, 0u);
    expected(2, 0xa72d8u, 0x222u, 0u);

    clear();
    mp_inc_speed_4eb78();
    assert(EF_U8(0x1a55a1u) == 1u);
    mp_inc_speed_4eb78();
    assert(EF_U8(0x1a55a1u) == 0u);
    EF_U8(0x1a55a1u) = 2u;
    mp_inc_speed_4eb78();
    assert(EF_U8(0x1a55a1u) == 1u);
    EF_U16(0x1a56f4u) = 895u;
    mp_dec_speed_4ec14();
    assert(EF_U16(0x1a56f4u) == 900u);
    EF_U16(0x1a56f4u) = 0xfff0u;
    mp_dec_speed_4ec14();
    assert(EF_U16(0x1a56f4u) == 44u); /* wrapped, not clamped */
    EF_U16(0x1a56f4u) = 100u;
    mp_mission_1_click_4ec44();
    assert(EF_U16(0x1a56f4u) == 120u);
    EF_U16(0x1a56f4u) = 0u;
    mp_mission_1_click_4ec44();
    assert(EF_U16(0x1a56f4u) == (uint16_t)-60);
    assert(count == 4u);
    for (i = 0; i < 4; ++i)
        expected(i, 0x4ec70u, 0, 0);
}

static void test_menu_callbacks(void)
{
    clear();
    EF_U8(0x410u) = 7;
    mp_click_v8_okay_4f2f8();
    assert(count == 3);
    expected(0, 0xa46ccu, 0x184b80u, 0);
    expected(1, 0xa4b80u, 7u, 0);
    expected(2, 0xac9e8u, 0, 0);
    calls_only();
    EF_U8(0x510u) = 11;
    mp_warning_ok_click_4f36c();
    assert(count == 3);
    expected(0, 0xa46ccu, 0x184e18u, 0);
    expected(1, 0xa4b80u, 11u, 0);

    calls_only();
    mp_return_to_campaign_505e4();
    assert(count == 3);
    expected(0, 0xa46ccu, 0x184ec4u, 0);
    expected(1, 0xa4d90u, 0x184ec4u, 0);
    expected(2, 0xac9a0u, 0x600u, 0);

    clear();
    EF_U8(0x1a53d8u) = 0xf0;
    EF_U8(0x1a53d9u) = 0xe1;
    EF_U8(0x1a53dau) = 0xd2;
    mp_no_quit_50604();
    assert(EF_U16(0x126554u) == 0xf0u);
    assert(EF_U16(0x126556u) == 0xe1u);
    assert(EF_U16(0x126558u) == 0xd2u);
    assert(EF_U8(0x1a53dcu) == 6u);
    assert(count == 1);
    expected(0, 0xac9e8u, 0, 0);

    clear();
    EF_U8(0x1a553au) = 8;
    mp_miss_plan_task_50750();
    assert(EF_U8(0x1a553au) == 0u);
    assert(EF_U8(0x12654cu) == 1 && EF_U8(0x1a54f3u) == 1);
    mp_miss_plan_task_50750();
    assert(EF_U8(0x1a553au) == 1u);

    clear();
    EF_U8(0x710u) = 13;
    mp_set_role_507f4();
    assert(count == 2);
    expected(1, 0xa4b80u, 13u, 0);
    calls_only();
    EF_U8(0x810u) = 4;
    EF_U8(0x910u) = 6;
    mp_set_ac_type_5080c();
    assert(count == 4);
    expected(0, 0xa46ccu, 0x184af0u, 0);
    expected(1, 0xa4b80u, 4u, 0);
    expected(2, 0xa46ccu, 0x184a8cu, 0);
    expected(3, 0xa4cb4u, 6u, 0);

    clear();
    EF_U8(0x1a53dcu) = 1;
    mp_change_mode_50c2c(0xabcu);
    assert(count == 1);
    expected(0, 0x4bc80u, 0xabcu, 0);
    EF_U8(0x1a53deu) = 1;
    mp_change_mode_50c2c(0xabcu);
    assert(count == 2);
    expected(1, 0xa870u, 0xabcu, 0);

    calls_only();
    EF_U8(0x1a53deu) = 0;
    mp_redraw_target_view_50c40(0xcdeu);
    assert(EF_U8(0x1a53deu) == 1u);
    expected(0, 0xa7588u, 0xcdeu, 0x132119u);
    mp_redraw_target_view_50c40(0xcdeu);
    assert(EF_U8(0x1a53deu) == 0u);
    expected(1, 0xa7588u, 0xcdeu, 0x13211eu);
    EF_U8(0x1a53deu) = 2;
    mp_redraw_target_view_50c40(0xcdeu);
    assert(EF_U8(0x1a53deu) == 0xffu);
    expected(2, 0xa7588u, 0xcdeu, 0x132119u);
}

int main(void)
{
    test_map_modes();
    test_waypoints_and_speed();
    test_menu_callbacks();
    return 0;
}
