/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_gui_options_096.h"
#include "ef_gui_script_095.h"
#include "ef_abi.h"
#include "ef_mem.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

enum { SCRIPT = 0x20000, COMMAND = 0x48592c, SOURCE = 0x485924 };
static uint8_t ram[0x500000];
static uint32_t calls[64];
static unsigned call_count;
static unsigned read_count;
static uint32_t format_args[4];

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(ram));
    return ram + address;
}

uint32_t guest_fn_b1b8b(ef_addr32 text)
{
    return (uint32_t)strlen((const char *)ef_guest_memory_ptr(text));
}

static void reset(void)
{
    memset(ram, 0, sizeof(ram));
    memset(calls, 0, sizeof(calls));
    memset(format_args, 0, sizeof(format_args));
    call_count = read_count = 0u;
    EF_U32(SOURCE) = SCRIPT;
}

static void set_script(const char *contents)
{
    strcpy((char *)ef_guest_memory_ptr(SCRIPT), contents);
    EF_U32(SOURCE) = SCRIPT;
}

static int same_casefold(const char *left, const char *right)
{
    unsigned a, b;
    do {
        a = (unsigned char)*left++;
        b = (unsigned char)*right++;
        if (a >= 'A' && a <= 'Z') a += 'a' - 'A';
        if (b >= 'A' && b <= 'Z') b += 'a' - 'A';
        if (a != b) return 0;
    } while (a != 0u);
    return 1;
}

uint32_t gui_script_guest_call_095(uint32_t target, uint32_t eax, uint32_t edx)
{
    if (target == 0xf2d74u) {
        gui_options_read_command_f2d74();
        ++read_count;
    } else if (target == 0xa4380u) {
        return same_casefold((const char *)ef_guest_memory_ptr(eax),
                             (const char *)ef_guest_memory_ptr(edx)) ? 0u : 1u;
    } else {
        assert(call_count < sizeof(calls) / sizeof(calls[0]));
        calls[call_count++] = target;
    }
    return 0u;
}

void gui_script_report_095(ef_addr32 format, unsigned args,
                           ef_addr32 token, ef_addr32 line)
{
    (void)format; (void)args; (void)token; (void)line;
    assert(!"unreachable diagnostic");
}

uint32_t gui_options_format_096(ef_addr32 format, uint32_t first,
                                uint32_t second, ef_addr32 suffix)
{
    format_args[0] = format;
    format_args[1] = first;
    format_args[2] = second;
    format_args[3] = suffix;
    return 0xfaceu;
}

static void test_filtered_command_reader(void)
{
    reset();
    set_script("   ; comment\r\n\r\n  SECTION:\r\n   NOFADE 1\r\n");
    gui_options_read_command_f2d74();
    assert(strcmp((char *)ef_guest_memory_ptr(COMMAND), "nofade") == 0);
    assert(EF_U8(0x1616ecu) == 4u);
    assert(EF_U32(SOURCE) == SCRIPT + strlen((char *)ef_guest_memory_ptr(SCRIPT)));

    reset();
    set_script("  Draw_All_Boxes\r\n");
    gui_options_read_command_f2d74();
    assert(strcmp((char *)ef_guest_memory_ptr(COMMAND), "draw_all_boxes") == 0);
    assert(EF_U8(0x1616ecu) == 1u);

    reset();
    set_script("  BACKDROP  Star.TGA, 7 \r\n");
    gui_options_read_command_f2d74();
    assert(strcmp((char *)ef_guest_memory_ptr(COMMAND), "backdrop") == 0);
    assert(EF_U16(0x1616eau) == 9u);
    read_script_token_f2cd0();
    assert(strcmp((char *)ef_guest_memory_ptr(COMMAND), "Star.TGA") == 0);
    read_script_token_f2cd0();
    assert(strcmp((char *)ef_guest_memory_ptr(COMMAND), "7") == 0);
}

static void test_real_reader_with_dispatcher(void)
{
    static const struct { uint32_t name; const char *word; } keywords[] = {
        {0x1919f8u,"backdrop"},{0x191a04u,"anim"},
        {0x191a0cu,"clickbox"},{0x191a18u,"lineclickbox"},
        {0x191a28u,"linepoint"},{0x191a34u,"boxcol"},
        {0x191a3cu,"nobox"},{0x191a44u,"nofade"},
        {0x191a4cu,"draw_all_boxes"},{0x191a5cu,"display"},
        {0x191a64u,"goto"},{0x191a6cu,"mouse"},
        {0x191a74u,"return"},{0x1919c8u,"eof"}
    };
    unsigned i;
    reset();
    for (i = 0u; i < sizeof(keywords) / sizeof(keywords[0]); ++i)
        strcpy((char *)ef_guest_memory_ptr(keywords[i].name), keywords[i].word);
    set_script(";ignore\r\n  HEADER:\r\n NOBOX\r\n nofade\r\n DISPlay\r\n");
    gui_options_dispatch_f3b78();
    assert(EF_U8(0x4859dbu) == 1u);
    assert(EF_U8(0x4859dcu) == 1u);
    assert(read_count == 3u);
    assert(EF_U8(0x1616ecu) == 5u);
    assert(call_count == 1u && calls[0] == 0xf3f84u);
}

static void test_all_dispatch_commands_from_script(void)
{
    static const uint32_t expected[] = {
        0xf2dfcu, 0xf2e54u, 0xf2e74u, 0xf2fbcu, 0xf314cu,
        0xf35d8u, 0xf3b1cu, 0xf3b40u, 0xf3b58u, 0xf3f84u
    };
    static const struct { uint32_t address; const char *word; } keywords[] = {
        {0x1919f8u,"backdrop"},{0x191a04u,"anim"},
        {0x191a0cu,"clickbox"},{0x191a18u,"lineclickbox"},
        {0x191a28u,"linepoint"},{0x191a34u,"boxcol"},
        {0x191a3cu,"nobox"},{0x191a44u,"nofade"},
        {0x191a4cu,"draw_all_boxes"},{0x191a5cu,"display"},
        {0x191a64u,"goto"},{0x191a6cu,"mouse"},
        {0x191a74u,"return"},{0x1919c8u,"eof"}
    };
    unsigned i;
    reset();
    for (i = 0u; i < sizeof(keywords) / sizeof(keywords[0]); ++i)
        strcpy((char *)ef_guest_memory_ptr(keywords[i].address), keywords[i].word);
    set_script(" BACKDROP 1\r\n ANIM 2\r\n CLICKBOX 3\r\n LINECLICKBOX 4\r\n"
               " LINEPOINT 5\r\n BOXCOL 6\r\n NoBoX\r\n NOFADE\r\n"
               " DRAW_ALL_BOXES\r\n GOTO elsewhere\r\n MOUSE 1\r\n"
               " unrecognized_keyword\r\n display\r\n return\r\n");
    gui_options_dispatch_f3b78();
    assert(read_count == 13u); /* The return after display is never read. */
    assert(EF_U8(0x1616ecu) == 13u);
    assert(EF_U8(0x4859dbu) == 1u);
    assert(EF_U8(0x4859dcu) == 1u);
    assert(EF_U8(0x4859ddu) == 1u);
    assert(call_count == sizeof(expected) / sizeof(expected[0]));
    for (i = 0u; i < call_count; ++i)
        assert(calls[i] == expected[i]);

    for (i = 0u; i < 2u; ++i) {
        reset();
        {
            unsigned j;
            for (j = 0u; j < sizeof(keywords) / sizeof(keywords[0]); ++j)
                strcpy((char *)ef_guest_memory_ptr(keywords[j].address), keywords[j].word);
        }
        set_script(i == 0u ? " RETURN\r\n display\r\n" : " eof\r\n display\r\n");
        gui_options_dispatch_f3b78();
        assert(read_count == 1u);
        assert(call_count == 0u);
    }
}

static void test_indirect_format_wrapper(void)
{
    reset();
    EF_U32(0x464390u) = 0x12345678u;
    EF_U16(0x16007cu) = 0xabcd;
    assert(gui_options_format_global_f6124() == 0xfaceu);
    assert(format_args[0] == 0x192c4cu);
    assert(format_args[1] == 0x12345678u);
    assert(format_args[2] == 0xabcdu);
    assert(format_args[3] == 0x464396u);
}

int main(void)
{
    test_filtered_command_reader();
    test_real_reader_with_dispatcher();
    test_all_dispatch_commands_from_script();
    test_indirect_format_wrapper();
    return 0;
}
