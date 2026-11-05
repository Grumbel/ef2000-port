/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_mem.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint8_t ram[0x500000];
struct Call { uint32_t target, eax, edx, ebx; };
static struct Call calls[40];
static unsigned call_count, report_count, compare_count;
static uint32_t compare_results[2];
static ef_addr32 report_source;
static uint32_t report_format, report_sink;

void *ef_guest_memory_ptr(ef_addr32 addr)
{
    assert(addr < sizeof(ram));
    return &ram[addr];
}

void environment_report_normal_line_088(ef_addr32 source, uint32_t format,
                                        uint32_t sink)
{
    ++report_count;
    report_source = source;
    report_format = format;
    report_sink = sink;
}

uint32_t gui_guest_call_088(uint32_t entry, uint32_t eax, uint32_t edx,
                           uint32_t ebx)
{
    assert(call_count < sizeof(calls) / sizeof(calls[0]));
    calls[call_count].target = entry;
    calls[call_count].eax = eax;
    calls[call_count].edx = edx;
    calls[call_count++].ebx = ebx;
    if (entry == 0xa4380u) {
        assert(compare_count < 2u);
        return compare_results[compare_count++];
    }
    if (entry == 0xa46ccu) return 0x350u;
    if (entry == 0x9d4cdu) return 0x7f121234u;
    return 0xdeadbeefu;
}

static void reset(void)
{
    memset(ram, 0, sizeof(ram));
    memset(calls, 0, sizeof(calls));
    call_count = report_count = compare_count = 0u;
    compare_results[0] = compare_results[1] = 0u;
    EF_U8(0x360u) = 0xabu;
}

static void check_call(unsigned index, uint32_t entry, uint32_t eax,
                       uint32_t edx)
{
    assert(index < call_count);
    if (calls[index].target != entry || calls[index].eax != eax ||
        calls[index].edx != edx)
        fprintf(stderr, "call[%u] actual=%x(%x,%x) expected=%x(%x,%x)\n",
                index, calls[index].target, calls[index].eax, calls[index].edx,
                entry, eax, edx);
    assert(calls[index].target == entry);
    assert(calls[index].eax == eax);
    assert(calls[index].edx == edx);
}

static void test_normal(void)
{
    reset();
    EF_U32(0x132a74u) = 0x777777u;
    memcpy(&ram[0x400u], "   ;comment", 12u);
    EF_U32(0x425af0u) = 0x400u;
    env_normal_dbdd0();
    assert(EF_U32(0x425af0u) == 0x403u && report_count == 0u);

    memcpy(&ram[0x500u], "  DATA\n", 7u);
    EF_U32(0x425af0u) = 0x500u;
    env_normal_dbdd0();
    assert(EF_U32(0x425af0u) == 0x502u && report_count == 1u);
    assert(report_source == 0x502u && report_format == 0x190448u);
    assert(report_sink == 0x777777u);

    memcpy(&ram[0x600u], " \n", 3u);
    EF_U32(0x425af0u) = 0x600u;
    env_normal_dbdd0();
    assert(EF_U32(0x425af0u) == 0x601u && report_count == 1u);
    EF_U32(0x425af0u) = 0x602u;
    env_normal_dbdd0();
    assert(report_count == 1u);
}

static void test_environment_modes(void)
{
    reset();
    EF_U8(0x425bc4u) = 0x40u;
    EF_U8(0x159130u) = 0x01u;
    assert(env_spacecorr_dbe5c(0xa1b2c3d4u) == 0xa1b248d4u);
    assert(EF_U8(0x159130u) == 0x85u);
    assert(EF_U8(0x425bc4u) == 0x48u);
    assert(env_rings_dbe80(0xff00ff01u) == 0xff004801u);
    assert(EF_U8(0x425c89u) == 1u);

    EF_U16(0x425bc4u) = 0xa550u;
    EF_U8(0x159130u) = 0x02u;
    assert(env_moon_dbeac(0x12345678u) == 0x12348278u);
    assert(EF_U16(0x425bc4u) == 0xa558u);
    assert(EF_U8(0x159130u) == 0x82u);
    env_inferno_dbedc();
    env_shield_dbee4();
    env_city_dbeec();
    assert(EF_U8(0x425bc4u) == 0x7bu);
    EF_U8(0x15bb5fu) = 3u;
    assert(env_day_dbef8(0xface1234u) == 0xface8234u);
    assert(EF_U8(0x15bb5fu) == 0u);
    assert(EF_U8(0x425bc5u) == 0xadu);
    assert(EF_U16(0x425bc4u) == 0xad7bu);

    reset();
    assert(env_rings_dbe80(0x11223344u) == 0x11220844u);
    assert(EF_U8(0x159130u) == 0x84u);
    assert(env_day_dbef8(0x12345678u) == 0x12348478u);
}

static void test_mainloop_and_mode(void)
{
    reset();
    gui_mainloop_4318(0x1000u);
    assert(call_count == 2u);
    check_call(0, 0x41fcu, 0x1000u, 0x1000u);
    check_call(1, 0x40dcu, 0x1000u, 0x1000u);

    reset();
    gui_change_mode_a960(0x345u);
    check_call(0, 0x4595cu, 0x345u, 0u);
    EF_U16(0x121e8eu) = 2u;
    gui_change_mode_a960(0x345u);
    check_call(1, 0xa870u, 0x345u, 0u);

    EF_U16(0x121e8eu) = 2u;
    gui_zoom_map_a974(0x987u);
    assert(EF_U16(0x121e8eu) == 0xffffu);
    check_call(2, 0xa7588u, 0x987u, 0x132119u);
    EF_U16(0x121e8eu) = 1u;
    gui_zoom_map_a974(0x988u);
    assert(EF_U16(0x121e8eu) == 0u);
    check_call(3, 0xa7588u, 0x988u, 0x13211eu);
}

static void test_difficulty_and_target(void)
{
    reset();
    EF_U32(0x121e44u) = 0xfffe1234u;
    EF_U32(0x121e40u) = 0x700u;
    gui_change_difficulty_4cf4(0x100u);
    assert(EF_U16(0x121e46u) == 0x1234u);
    assert(call_count == 4u);
    check_call(0, 0x9d4cdu, 0x13fu, 0u);
    check_call(1, 0xa46ccu, 0x17f7a4u, 0u);
    /* Store at 0x121e46 *overlaps* the high word of 0x121e44, so the
     * original row uses the freshly parsed 0x1234, not old 0xfffe. */
    check_call(2, 0x7c4fcu, 0x700u + 0x28u + 4659u * 76u, 0x350u);
    check_call(3, 0xa5ef8u, 0xabu, 0x350u);

    reset();
    compare_results[0] = 1u;
    gui_close_target_view_b574(0x100u);
    assert(compare_count == 1u && EF_U8(0x121e95u) == 2u);
    check_call(0, 0xa4380u, 0x13fu, 0x180834u);
    check_call(1, 0xa46ccu, 0x1805f0u, 0u);
    check_call(2, 0xa5ef8u, 0xabu, 0u);

    reset();
    compare_results[1] = 1u;
    gui_close_target_view_b574(0x100u);
    assert(compare_count == 2u && EF_U8(0x121e95u) == 1u);
    check_call(1, 0xa4380u, 0x13fu, 0x180834u);

    reset();
    gui_close_target_view_b574(0x100u);
    assert(compare_count == 2u && EF_U8(0x121e95u) == 0u);
}

static void test_pilots(void)
{
    static const uint32_t names[] = { 0x1808f4u, 0x1808fcu,
                                       0x180904u, 0x18090cu };
    unsigned i;
    reset();
    gui_click_pilot1_bde8();
    assert(call_count == 8u);
    for (i = 0; i < 4u; ++i) {
        check_call(i * 2u, 0xa46ccu, names[i], 0u);
        check_call(i * 2u + 1u, 0xa4b80u, 0xabu, 0u);
    }

    reset();
    gui_click_pilot2_be48(0x200u);
    assert(call_count == 3u);
    check_call(0, 0xa4380u, 0x23fu, 0x180914u);
    check_call(1, 0x779f4u, 0u, 0u);
    check_call(2, 0xbc38u, 0x1808f4u, 0u);

    reset();
    EF_U32(0x197668u) = 0x200u;
    gui_click_pilot3_be8c(0x200u);
    check_call(1, 0x779f4u, 0x243u, 1u);
    check_call(2, 0xbc38u, 0x1808fcu, 1u);

    reset();
    gui_click_pilot4_bed8(0x200u);
    check_call(1, 0x779f4u, 0x86u, 2u);
    check_call(2, 0xbc38u, 0x180904u, 2u);

    reset();
    gui_save_conflict_bf24(0x200u);
    check_call(1, 0x779f4u, 0xc9u, 3u);
    check_call(2, 0xbc38u, 0x18090cu, 3u);

    reset();
    compare_results[0] = 1u;
    gui_click_pilot2_be48(0x200u);
    assert(call_count == 3u);
    check_call(1, 0xa46ccu, 0x180918u, 0u);
    check_call(2, 0xa4b80u, 0xabu, 0u);
}

static void test_debrief_and_size(void)
{
    reset();
    EF_U16(0x121e98u) = 1u;
    EF_U16(0x1a53c0u) = 3u;
    gui_show_debrief_c838();
    assert(EF_U16(0x121e98u) == 2u);
    gui_show_debrief_c838();
    assert(EF_U16(0x121e98u) == 0u);
    assert(call_count == 2u);
    check_call(0, 0xc868u, 0u, 0u);
    EF_U16(0x121e98u) = 0x7fffu;
    EF_U16(0x1a53c0u) = 0x8000u;
    gui_show_debrief_c838();
    assert(EF_U16(0x121e98u) == 0u); /* signed >= */

    EF_U16(0x1a365eu) = 0x9876u;
    EF_U16(0x1a3660u) = 0x1234u;
    gui_alter_size_ca40();
    assert(EF_U16(0x1a4ba2u) == 0x9876u);
    assert(EF_U16(0x1a4ba0u) == 0x1234u);
}

int main(void)
{
    test_normal();
    test_environment_modes();
    test_mainloop_and_mode();
    test_difficulty_and_target();
    test_pilots();
    test_debrief_and_size();
    return 0;
}
