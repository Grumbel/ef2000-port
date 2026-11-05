/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_mem.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t ram[0x500000];
struct Call { uint32_t entry, eax, edx; };
static struct Call calls[32];
static unsigned call_count;
static uint32_t select_result, fuel_result, type_result;

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(ram));
    return &ram[address];
}

uint32_t gui_guest_call_089(uint32_t entry, uint32_t eax, uint32_t edx)
{
    assert(call_count < sizeof(calls) / sizeof(calls[0]));
    calls[call_count++] = (struct Call){ entry, eax, edx };
    if (entry == 0x432cu) return select_result;
    if (entry == 0x50770u) return fuel_result;
    if (entry == 0x50a1cu) return type_result;
    if (entry == 0x50b68u) return 0xabcdefffu;
    return 0u;
}

static void reset(void)
{
    memset(ram, 0, sizeof(ram));
    memset(calls, 0, sizeof(calls));
    call_count = 0u;
    select_result = fuel_result = type_result = 0u;
}

static void check(unsigned index, uint32_t entry, uint32_t eax, uint32_t edx)
{
    assert(index < call_count);
    assert(calls[index].entry == entry);
    assert(calls[index].eax == eax);
    assert(calls[index].edx == edx);
}

static void test_kots(void)
{
    reset();
    EF_U32(0x200u + 0x51u) = 0x400u;
    EF_U8(0x410u) = 0x71u;
    gui_kots_init_4b54(0x200u);
    assert(call_count == 3u);
    check(0, 0xa4b80u, 0x71u, 0u);
    check(1, 0x432cu, 0u, 0u);
    check(2, 0xac7a8u, 0x17f79cu, 0u);

    reset();
    EF_U32(0x251u) = 0x400u;
    EF_U8(0x410u) = 0x22u;
    select_result = 0x12340001u;
    gui_kots_init_4b54(0x200u);
    check(2, 0x487cu, select_result, 0u);
}

static void test_warning(void)
{
    reset();
    EF_U8(0x126576u) = 1u;
    gui_campaign_warning_ok_4b1a4();
    assert(call_count == 1u);
    check(0, 0x4f388u, 0x132202u, 0x132223u);

    reset();
    EF_U16(0x1357b0u) = 10u;
    fuel_result = 9u;
    gui_campaign_warning_ok_4b1a4();
    check(0, 0x50770u, 0u, 0u);
    check(1, 0x4f388u, 0x1321b7u, 0x1321d8u);
    assert(call_count == 2u);

    reset();
    EF_U16(0x1357b0u) = 10u;
    fuel_result = 10u;
    gui_campaign_warning_ok_4b1a4();
    assert(call_count == 3u);
    check(1, 0xa4c84u, 0u, 0u);
    check(2, 0xac7a8u, 0x1849bcu, 0u);
}

static void test_fuel_and_types(void)
{
    reset();
    gui_draw_fuel_bar_50638();
    check(0, 0xac9e8u, 0u, 0u);
    check(1, 0xa4db0u, 0x184ec4u, 0u);

    reset();
    EF_U8(0x242u) = '1';
    EF_U8(0x243u) = '7';
    gui_type2_click_5089c(0x200u);
    assert(call_count == 3u);
    check(0, 0x50a1cu, 3u, 17u); /* ASCII '7' - 0x26 */
    check(1, 0x50b68u, 3u, 17u);
    check(2, 0x5086cu, 0xabcdefffu, 17u);

    reset();
    EF_U8(0x242u) = '2';
    gui_type3_click_508fc(0x200u);
    check(0, 0x50a1cu, 4u, 2u);
    check(1, 0x50b68u, 4u, 2u);

    reset();
    EF_U8(0x242u) = '1';
    EF_U8(0x243u) = 'x';
    type_result = 1u;
    gui_type4_click_5095c(0x200u);
    assert(call_count == 2u);
    check(0, 0x50a1cu, 1u, 1u);
    check(1, 0x4f314u, 0x1326e7u, 0x132704u);
}

static void test_assignments(void)
{
    reset();
    gui_previous_assignment_4d9cc();
    check(0, 0x4f314u, 0x132351u, 0x132373u);
    gui_make_assignment_4da2c();
    check(1, 0x4f314u, 0x132351u, 0x132373u);
    gui_view_assignment_4da90();
    check(2, 0x4f314u, 0x132351u, 0x132373u);

    reset();
    EF_U8(0x1a5532u) = 1u;
    EF_U8(0x1a5531u) = 3u;
    EF_U8(0x1a5510u) = 3u;
    EF_U8(0x1a5511u) = 2u;
    EF_U8(0x1a5512u) = 4u;
    EF_U8(0x1a5598u) = 0u;
    gui_previous_assignment_4d9cc();
    assert(EF_U8(0x1a5598u) == 2u);
    check(0, 0x4db60u, 0u, 0u);
    gui_make_assignment_4da2c();
    assert(EF_U8(0x1a5598u) == 0u);
    check(1, 0x4db60u, 0u, 0u);
    gui_make_assignment_4da2c();
    assert(EF_U8(0x1a5598u) == 2u); /* wraps from zero to last */

    EF_U8(0x1a5540u) = 0x29u;
    EF_U32(0x1a5554u) = 0x12345678u;
    gui_view_assignment_4da90();
    assert(EF_U8(0x1a5546u) == 0x29u);
    assert(EF_U32(0x1a5560u) == 0x12345678u);
    assert(EF_U8(0x1a5598u) == 0u);
    assert(call_count == 5u);
    check(3, 0x4db60u, 0u, 0u);
    check(4, 0x4db60u, 0u, 0u);
}

static void test_misc(void)
{
    reset();
    EF_U8(0x410u) = 0x84u;
    gui_show_summary_info_84ddc(0x400u);
    check(0, 0xa4b80u, 0x84u, 0u);
    env_clouds_da2b0();
    assert(EF_U8(0x425c88u) == 1u);

    reset();
    gui_open_credits_37260();
    check(0, 0x3718eu, 0u, 0u);
    gui_new_socket_37254();
    assert(EF_U8(0x125846u) == 1u);
    check(1, 0x3718eu, 1u, 0u);

    reset();
    EF_U8(0x125846u) = 3u;
    EF_U8(0x126576u) = 1u;
    gui_open_credits_37260();
    check(0, 0x371adu, 3u, 0u);
    EF_U8(0x126576u) = 0u;
    EF_U32(0x19769eu) = 0xffff0000u;
    gui_open_credits_37260();
    check(1, 0x371adu, 0xffffffffu, 0u);
    EF_U32(0x19769eu) = 0x00000001u;
    gui_open_credits_37260();
    check(2, 0xb1e0u, 0u, 0u);
}

int main(void)
{
    test_kots();
    test_warning();
    test_fuel_and_types();
    test_assignments();
    test_misc();
    return 0;
}
