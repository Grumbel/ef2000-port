/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_mem.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t ram[0x500000];
struct Event { uint32_t entry, eax, edx, ebx; };
static struct Event calls[64];
static unsigned call_count;
static int scanf_count;
static int32_t scanf_number;
static uint8_t port_result;
static uint32_t widget_result;

void *ef_guest_memory_ptr(ef_addr32 addr)
{
    assert(addr < sizeof(ram));
    return &ram[addr];
}

static void record(uint32_t entry, uint32_t eax, uint32_t edx, uint32_t ebx)
{
    assert(call_count < sizeof(calls) / sizeof(calls[0]));
    calls[call_count++] = (struct Event){entry, eax, edx, ebx};
}

uint32_t net_ui_guest_call_086(uint32_t entry, uint32_t eax,
                              uint32_t edx, uint32_t ebx)
{
    record(entry, eax, edx, ebx);
    if (entry == 0xe1540u)
        return port_result;
    if (entry == 0xa78bcu)
        return widget_result;
    return eax;
}

int net_ui_parse_decimal_086(ef_addr32 text, uint32_t format, int32_t *result)
{
    record(0xdf26fu, text, format, 0u);
    *result = scanf_number;
    return scanf_count;
}

ef_addr32 net_ui_format_option_086(uint32_t format, uint32_t value)
{
    record(0xa4342u, format, value, 0u);
    return 0x3000u;
}

static void clear_calls(void)
{
    memset(calls, 0, sizeof(calls));
    call_count = 0;
}

static void reset(void)
{
    memset(ram, 0, sizeof(ram));
    clear_calls();
    scanf_count = 1;
    scanf_number = 1;
    port_result = 2;
    widget_result = 0x1000u;
}

static void expect(unsigned i, uint32_t entry, uint32_t eax,
                   uint32_t edx, uint32_t ebx)
{
    assert(i < call_count);
    assert(calls[i].entry == entry);
    assert(calls[i].eax == eax);
    assert(calls[i].edx == edx);
    assert(calls[i].ebx == ebx);
}

static void test_baud_and_parity(void)
{
    reset();
    EF_U32(0x200u + 0x27u) = 0x290u;
    scanf_number = 4;
    net_new_baud_335c0(0x200u);
    assert(EF_U8(0x15f07fu) == 3u);
    assert(call_count == 5);
    expect(0, 0x34ea4u, 0x200u, 0x200u, 0u);
    expect(1, 0xa3da0u, 0u, 0u, 0u);
    expect(2, 0xdf26fu, 0x290u, 0x182bbcu, 0u);
    expect(3, 0xa2bdcu, 0u, 0u, 0u);
    expect(4, 0x33630u, 0u, 0u, 0u);

    scanf_number = 1;
    net_new_baud_335c0(0x200u);
    assert(EF_U8(0x15f07fu) == 0u);
    scanf_number = 5;
    net_new_baud_335c0(0x200u);
    assert(EF_U8(0x15f07fu) == 0xffu);
    scanf_number = -2;
    net_new_baud_335c0(0x200u);
    assert(EF_U8(0x15f07fu) == 0xffu);
    scanf_number = 2;
    scanf_count = 0;
    net_new_baud_335c0(0x200u);
    assert(EF_U8(0x15f07fu) == 0xffu);

    clear_calls();
    net_new_parity_3361c(0x200u);
    assert(call_count == 5);
    expect(0, 0x34ea4u, 0x200u, 0u, 0u);
    expect(2, 0xdf2e4u, 0u, 0u, 0u);
    expect(3, 0xa2bdcu, 0u, 0u, 0u);
    expect(4, 0x33630u, 0u, 0u, 0u);
}

static void test_fifo_and_ipx(void)
{
    reset();
    net_new_fifo_len_336c0(0x200u);
    assert(EF_U8(0x15f099u) == 1u);
    assert(call_count == 4);
    expect(3, 0x33630u, 0u, 0u, 0u);
    EF_U8(0x13578cu) = 1u;
    EF_U8(0x15f098u) = 1u;
    net_new_fifo_len_336c0(0x200u);
    assert(EF_U8(0x15f099u) == 1u);
    EF_U8(0x15f098u) = 0u;
    net_new_fifo_len_336c0(0x200u);
    assert(EF_U8(0x15f099u) == 0u);
    clear_calls();
    net_open_ipx_settings_336f0(0x200u);
    assert(call_count == 0);
    EF_U8(0x15f099u) = 1u;
    EF_U8(0x15f09au) = 3u;
    net_open_ipx_settings_336f0(0x200u);
    assert(EF_U8(0x15f09au) == 0u);
    assert(call_count == 4);
    expect(3, 0x33630u, 0u, 0u, 0u);
    net_open_ipx_settings_336f0(0x200u);
    assert(EF_U8(0x15f09au) == 1u);
}

static void test_modem(void)
{
    reset();
    EF_U32(0x200u + 0x27u) = 0x2a0u;
    net_new_number_33720(0x200u);
    expect(0, 0x9d749u, 0x15f57du, 0x2a0u, 0u);
    expect(1, 0xa2bdcu, 0u, 0u, 0u);
    clear_calls();
    net_new_pt_33738(0x200u);
    expect(0, 0x9d749u, 0x15f53du, 0x2a0u, 0u);
    clear_calls();
    net_open_modem_status_337ac(0x200u);
    expect(0, 0x9d749u, 0x15f4fdu, 0x2a0u, 0u);

    clear_calls();
    EF_U32(0x125958u) = 0x2200u;
    EF_U32(0x12595cu) = 0x2300u;
    net_new_init_string_33760(0x200u);
    assert(EF_U8(0x15f688u) == 1u);
    assert(call_count == 3);
    expect(0, 0xa4342u, 0x182bc0u, 0x2300u, 0u);
    expect(1, 0xa7588u, 0x200u, 0x3000u, 0u);
    expect(2, 0xa2bdcu, 0u, 0u, 0u);
    clear_calls();
    net_new_init_string_33760(0x200u);
    assert(EF_U8(0x15f688u) == 0u);
    expect(0, 0xa4342u, 0x182bc0u, 0x2200u, 0u);

    clear_calls();
    EF_U8(0x13579au) = 7u;
    port_result = 0x23u;
    net_new_port_337c4(0x200u);
    assert(EF_U8(0x13579au) == 0x23u);
    assert(call_count == 5);
    expect(1, 0xe1540u, 7u, 0u, 0u);
    expect(2, 0x34ea4u, 0x23u, 0u, 0u);
    expect(4, 0x33630u, 0u, 0u, 0u);
}

static void test_directplay_rows(void)
{
    reset();
    EF_U32(0x429210u) = 3u;
    EF_U32(0x1000u + 0x21u) = 0x1100u;
    EF_U32(0x1100u + 0x21u) = 0x1200u;
    net_directplay_game_selected_341d8(0x200u);
    assert(call_count == 4);
    expect(0, 0xa78bcu, 0x200u, 0x182d94u, 0x4288f0u);
    expect(1, 0xa7588u, 0x1000u, 0x4288f0u, 0u);
    expect(2, 0xa7588u, 0x1100u, 0x428930u, 0u);
    expect(3, 0xa7588u, 0x1200u, 0x428970u, 0u);
    clear_calls();
    EF_U32(0x429210u) = 0xffffffffu;
    net_directplay_game_selected_341d8(0x200u);
    assert(call_count == 1);
}

static void test_open_loops_form(void)
{
    reset();
    EF_U8(0x15f688u) = 1u;
    EF_U32(0x12595cu) = 0x2300u;
    net_open_loops_form_33ad8(0x200u);
    assert(call_count == 9);
    expect(0, 0xa78bcu, 0x200u, 0x182c70u, 0u);
    expect(1, 0xa7588u, 0x1000u, 0x15f4fdu, 0u);
    expect(2, 0xa4342u, 0x182bc0u, 0x2300u, 0u);
    expect(3, 0xa78bcu, 0x200u, 0x182c78u, 0u);
    expect(4, 0xa7588u, 0x1000u, 0x3000u, 0u);
    expect(5, 0xa78bcu, 0x200u, 0x182c80u, 0u);
    expect(6, 0xa7588u, 0x1000u, 0x15f53du, 0u);
    expect(7, 0xa78bcu, 0x200u, 0x182c8cu, 0u);
    expect(8, 0xa7588u, 0x1000u, 0x15f57du, 0u);
}

static void test_channel_and_windows(void)
{
    reset();
    EF_U16(0x1357b8u) = 0x1234u;
    net_start_arcade_34480();
    assert(EF_U16(0x1357b8u) == 0u);
    assert(call_count == 3);
    expect(0, 0x34bccu, 0u, 0u, 0u);
    expect(2, 0x33630u, 0u, 0u, 0u);

    clear_calls();
    EF_U8(0x1357c8u) = 0xfeu;
    net_new_channel_344a0();
    assert(EF_U8(0x1357c8u) == 0xffu);
    assert(call_count == 4);
    expect(0, 0xa3da0u, 0u, 0u, 0u);
    expect(1, 0x34ea4u, 0u, 0u, 0u);
    expect(3, 0x33630u, 0u, 0u, 0u);

    clear_calls();
    net_new_end_string_345d4();
    assert(call_count == 0);
    EF_U8(0x13578cu) = 1u;
    net_new_end_string_345d4();
    expect(0, 0xa4d90u, 0x182e10u, 0u, 0u);

    clear_calls();
    EF_U8(0x200u + 0x10u) = 0xadu;
    net_open_loops_347ac(0x200u);
    expect(0, 0xa4b80u, 0xadu, 0u, 0u);

    clear_calls();
    net_main_loop_349ec(0x200u);
    assert(EF_U32(0x19f320u) == 0x1000u);
    expect(0, 0xa78bcu, 0x200u, 0x182e60u, 0u);
    expect(1, 0x348e4u, 0u, 0u, 0u);

    clear_calls();
    net_open_initialise_35128();
    assert(EF_U8(0x19f59bu) == 0xffu);
    assert(call_count == 3);
    expect(0, 0xa3e04u, 0u, 0u, 0u);
    expect(2, 0xac7a8u, 0x182f24u, 0u, 0u);
}

static void test_receive_status_and_type(void)
{
    reset();
    net_open_rx_status_35e68(0x200u);
    assert(call_count == 3);
    expect(0, 0xa75b4u, 0x200u, 0x182fa8u, 0u);
    expect(1, 0xa7434u, 0x200u, 0u, 0u);
    expect(2, 0x35de0u, 0u, 0u, 0u);
    clear_calls();
    net_set_nonentry_flag_35e90(0x200u);
    assert(call_count == 3);
    expect(0, 0xa75b4u, 0x200u, 0x182fa8u, 0u);
    expect(2, 0x35de0u, 0u, 0u, 0u);

    clear_calls();
    net_new_type_35eb8(0x200u);
    assert(call_count == 2);
    expect(0, 0xa73ccu, 0x200u, 0x183114u, 0u);
    expect(1, 0xa73ccu, 0x200u, 0x18311cu, 1u);
    clear_calls();
    EF_U8(0x13578du) = 2u;
    net_new_type_35eb8(0x200u);
    expect(0, 0xa73ccu, 0x200u, 0x183114u, 1u);
    expect(1, 0xa73ccu, 0x200u, 0x18311cu, 0u);
}

int main(void)
{
    test_baud_and_parity();
    test_fifo_and_ipx();
    test_modem();
    test_directplay_rows();
    test_open_loops_form();
    test_channel_and_windows();
    test_receive_status_and_type();
    return 0;
}
