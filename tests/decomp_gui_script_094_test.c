/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_gui_script_094.h"
#include "ef_mem.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { TOKEN_ADDR = 0x485e08, NODE = 0x30000, STATUS = 0x6000,
       PATH = 0x7000 };

static uint8_t ram[0x500000];
static const char *tokens[8];
static unsigned next_token;
static struct { uint32_t target, eax, edx; } calls[64];
static unsigned call_count;
static struct { uint32_t message, arguments, token, line; } errors[16];
static unsigned error_count;

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(ram));
    return ram + address;
}

uint32_t gui_script_guest_call_094(uint32_t target, uint32_t eax, uint32_t edx)
{
    char *end;
    long value;
    assert(call_count < sizeof(calls) / sizeof(calls[0]));
    calls[call_count].target = target;
    calls[call_count].eax = eax;
    calls[call_count].edx = edx;
    call_count++;
    if (target == 0xf46ccu) {
        assert(next_token < 8u && tokens[next_token] != NULL);
        strcpy((char *)ef_guest_memory_ptr(TOKEN_ADDR), tokens[next_token++]);
    } else if (target == 0xebd30u) {
        return STATUS;
    } else if (target == 0x9d4cdu) {
        value = strtol((char *)ef_guest_memory_ptr(eax), &end, 10);
        if (end == (char *)ef_guest_memory_ptr(eax) || *end != '\0')
            EF_U32(STATUS) = 1u;
        return (uint32_t)value;
    } else if (target == 0x9c980u) {
        assert(eax == PATH);
        strcpy((char *)ef_guest_memory_ptr(eax), "base/");
    } else if (target == 0x9ca57u) {
        char *dst;
        char *src;
        assert(eax == PATH);
        dst = (char *)ef_guest_memory_ptr(eax);
        src = (char *)ef_guest_memory_ptr(edx);
        memmove(dst + strlen(dst), src, strlen(src) + 1u);
    }
    return 0u;
}

void gui_script_report_094(ef_addr32 message, unsigned arguments,
                           ef_addr32 token, ef_addr32 line)
{
    assert(error_count < sizeof(errors) / sizeof(errors[0]));
    errors[error_count].message = message;
    errors[error_count].arguments = arguments;
    errors[error_count].token = token;
    errors[error_count].line = line;
    error_count++;
}

ef_addr32 gui_script_path_buffer_094(void)
{
    return PATH;
}

static void reset(void)
{
    memset(ram, 0, sizeof(ram));
    memset(tokens, 0, sizeof(tokens));
    memset(calls, 0, sizeof(calls));
    memset(errors, 0, sizeof(errors));
    next_token = call_count = error_count = 0u;
    EF_U32(0x485de0u) = NODE;
    EF_U32(0x161ea8u) = 0x1234u;
    EF_U16(0x485dfeu) = 7u;
    strcpy((char *)ef_guest_memory_ptr(0x192448u), "obj/");
    strcpy((char *)ef_guest_memory_ptr(0x192454u), ".bmp");
}

static void input_pair(uint16_t mode)
{
    EF_U16(0x485dfcu) = mode;
    tokens[0] = "-3";
    tokens[1] = "65539";
}

static void assert_pair_calls(void)
{
    static const uint32_t expected[] = {
        0xf46ccu, 0xebd30u, 0x9d4cdu, 0xebd30u,
        0xf46ccu, 0x9d4cdu, 0xebd30u
    };
    unsigned i;
    assert(call_count == 7u && next_token == 2u);
    for (i = 0u; i < 7u; ++i)
        assert(calls[i].target == expected[i]);
    assert(calls[2].eax == TOKEN_ADDR && calls[5].eax == TOKEN_ADDR);
}

static void test_numeric_properties(void)
{
    static const struct {
        void (*function)(void);
        uint16_t mode;
        uint32_t first, second, flag;
    } cases[] = {
        {gui_script_size_f4db8, 1, 0x57, 0x59, 0x485f6f},
        {gui_script_size_f4db8, 2, 0x63, 0x65, 0x485f6c},
        {gui_script_size_f4db8, 3, 0x4b, 0x4d, 0x485f70},
        {gui_script_from_f4ed4, 1, 0x5b, 0x5d, 0x485f73},
        {gui_script_from_f4ed4, 2, 0x67, 0x69, 0x485f6e},
        {gui_script_from_f4ed4, 3, 0x4f, 0x51, 0x485f71},
        {gui_script_backdrop_f4ff0, 1, 0x55, 0x53, 0x485f72},
        {gui_script_backdrop_f4ff0, 2, 0x61, 0x5f, 0x485f6d},
    };
    unsigned i;
    for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        reset();
        input_pair(cases[i].mode);
        cases[i].function();
        assert_pair_calls();
        assert(EF_U16(NODE + cases[i].first) == 0xfffdu);
        assert(EF_U16(NODE + cases[i].second) == 3u);
        assert(EF_U8(cases[i].flag) == 1u);
        assert(error_count == 0u);
    }

    reset();
    input_pair(3u);
    gui_script_backdrop_f4ff0();
    assert_pair_calls();
    assert(error_count == 1u);
    assert(errors[0].message == 0x1922b0u && errors[0].arguments == 2u);
    assert(EF_U8(0x485f72u) == 0u && EF_U8(0x485f6du) == 0u);

    reset();
    input_pair(0u);
    gui_script_size_f4db8();
    assert(error_count == 1u && errors[0].message == 0x192148u);

    reset();
    EF_U16(0x485dfcu) = 1u;
    tokens[0] = "invalid";
    tokens[1] = "7";
    gui_script_size_f4db8();
    assert(error_count == 2u);
    assert(errors[0].message == 0x1920f0u);
    assert(errors[1].message == 0x19211cu);
    assert(EF_U16(NODE + 0x57u) == 0u && EF_U16(NODE + 0x59u) == 7u);
}

static void test_string_properties(void)
{
    reset();
    tokens[0] = "DO_THIS";
    tokens[1] = "DO_AGAIN";
    tokens[2] = "ARROW";
    tokens[3] = "AFTER_POINTER";
    gui_script_action_f50ec();
    gui_script_action_f50ec();
    assert(error_count == 0u); /* ACTION does not test its own flag. */
    assert(strcmp((char *)ef_guest_memory_ptr(NODE + 8u), "DO_AGAIN") == 0);
    assert(EF_U8(0x485f76u) == 1u);
    gui_script_pointer_f5180();
    assert(strcmp((char *)ef_guest_memory_ptr(NODE + 0x29u), "ARROW") == 0);
    assert(EF_U8(0x485f78u) == 1u);
    gui_script_action_f50ec();
    assert(error_count == 1u && errors[0].message == 0x192320u);
    assert(strcmp((char *)ef_guest_memory_ptr(NODE + 8u), "AFTER_POINTER") == 0);
    assert(errors[0].token == TOKEN_ADDR && errors[0].line == 0x1234u);

    reset();
    tokens[0] = "123456789012345678901234567890123"; /* 33 > 32 */
    gui_script_pointer_f5180();
    assert(error_count == 1u && errors[0].message == 0x192378u);
    assert(strlen((char *)ef_guest_memory_ptr(NODE + 0x29u)) == 33u);
}

static void check_asset_calls(uint32_t selector, uint32_t name)
{
    static const uint32_t expected[] = {
        0xf46ccu, 0x9c980u, 0x9ca57u, 0x9ca57u,
        0x9ca57u, 0u, 0xc34ccu, 0xc39acu
    };
    unsigned i;
    assert(call_count == 8u);
    for (i = 0u; i < 8u; ++i)
        assert(calls[i].target == (i == 5u ? selector : expected[i]));
    assert(calls[2].edx == 0x192448u);
    assert(calls[3].edx == name);
    assert(calls[4].edx == 0x192454u);
    assert(calls[6].eax == PATH);
}

static void test_asset_handlers(void)
{
    unsigned i;
    reset();
    EF_U16(0x485dfcu) = 12u;
    tokens[0] = "GFX";
    for (i = 0u; i < 256u; ++i) {
        EF_U8(0x425c8cu + i * 3u) = (uint8_t)i;
        EF_U8(0x425c8cu + i * 3u + 1u) = (uint8_t)i;
        EF_U8(0x425c8cu + i * 3u + 2u) = (uint8_t)i;
    }
    gui_script_gadgetry_f5214();
    check_asset_calls(0xc3974u, 0x485f5au);
    assert(strcmp((char *)ef_guest_memory_ptr(0x485f5au), "GFX") == 0);
    assert(strcmp((char *)ef_guest_memory_ptr(PATH), "base/obj/GFX.bmp") == 0);
    assert(EF_U16(0x485dfeu) == 255u);
    assert(EF_U16(0x485e00u) == 255u);
    assert(EF_U16(0x485e02u) == 0u);
    assert(error_count == 0u);

    reset();
    EF_U16(0x485dfcu) = 12u;
    tokens[0] = "FONT";
    EF_U16(0x485dfeu) = 7u;
    memset(ef_guest_memory_ptr(0x425c8cu), 20, 256 * 3);
    gui_script_gadgetry_f5214();
    assert(EF_U16(0x485dfeu) == 7u && EF_U16(0x485e02u) == 7u);

    reset();
    EF_U16(0x485dfcu) = 12u;
    tokens[0] = "BIRDS";
    gui_script_name_f53bc();
    check_asset_calls(0xc3980u, 0x485f63u);
    assert(strcmp((char *)ef_guest_memory_ptr(0x485f63u), "BIRDS") == 0);
    assert(strcmp((char *)ef_guest_memory_ptr(PATH), "base/obj/BIRDS.bmp") == 0);
    assert(EF_U16(0x485dfeu) == 7u && EF_U16(0x485e02u) == 0u);

    reset();
    EF_U16(0x485dfcu) = 4u;
    strcpy((char *)ef_guest_memory_ptr(0x485f63u), "OLD");
    tokens[0] = "";
    gui_script_name_f53bc();
    assert(error_count == 2u);
    assert(errors[0].message == 0x19245cu);
    assert(errors[1].message == 0x1924b4u);
    assert(strcmp((char *)ef_guest_memory_ptr(PATH), "base/obj/OLD.bmp") == 0);
}

int main(void)
{
    test_numeric_properties();
    test_string_properties();
    test_asset_handlers();
    return 0;
}
