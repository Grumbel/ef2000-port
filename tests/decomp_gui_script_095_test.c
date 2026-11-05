/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_gui_script_095.h"
#include "ef_mem.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { TOKEN = 0x485e08, OPTION_TOKEN = 0x48592c, ERRNO_WORD = 0x6000 };
static uint8_t ram[0x500000];
static uint32_t called[256];
static unsigned calls;
static struct { uint32_t format, args, token, line; } reports[32];
static unsigned report_count;
static const char *option_input[32];
static unsigned option_position;

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(ram));
    return ram + address;
}

static int ascii_same(const char *a, const char *b)
{
    while (*a && *b) {
        unsigned x = (unsigned char)*a++;
        unsigned y = (unsigned char)*b++;
        if (x >= 'A' && x <= 'Z') x += 'a' - 'A';
        if (y >= 'A' && y <= 'Z') y += 'a' - 'A';
        if (x != y) return 0;
    }
    return *a == *b;
}

uint32_t gui_script_guest_call_095(uint32_t target, uint32_t eax, uint32_t edx)
{
    char *end;
    long result;
    assert(calls < sizeof(called) / sizeof(called[0]));
    called[calls++] = target;
    if (target == 0xebd30u)
        return ERRNO_WORD;
    if (target == 0x9d524u || target == 0xa4380u)
        return ascii_same((const char *)ef_guest_memory_ptr(eax),
                          (const char *)ef_guest_memory_ptr(edx)) ? 0u : 1u;
    if (target == 0x9d4cdu) {
        const char *source = (const char *)ef_guest_memory_ptr(eax);
        result = strtol(source, &end, 10);
        if (end == source || *end != '\0')
            EF_U32(ERRNO_WORD) = 1u;
        return (uint32_t)result;
    }
    if (target == 0xf2d74u) {
        assert(option_position < sizeof(option_input) / sizeof(option_input[0]));
        assert(option_input[option_position] != NULL);
        strcpy((char *)ef_guest_memory_ptr(OPTION_TOKEN), option_input[option_position++]);
    }
    return 0u;
}

void gui_script_report_095(ef_addr32 format, unsigned arguments,
                           ef_addr32 token, ef_addr32 line)
{
    assert(report_count < sizeof(reports) / sizeof(reports[0]));
    reports[report_count].format = format;
    reports[report_count].args = arguments;
    reports[report_count].token = token;
    reports[report_count].line = line;
    ++report_count;
}

static void reset(void)
{
    memset(ram, 0, sizeof(ram));
    memset(called, 0, sizeof(called));
    memset(reports, 0, sizeof(reports));
    memset(option_input, 0, sizeof(option_input));
    calls = report_count = option_position = 0u;
    EF_U32(0x161ea8u) = 87u;
    strcpy((char *)ef_guest_memory_ptr(0x191bf8u), "FROM");
    strcpy((char *)ef_guest_memory_ptr(0x192544u), "size");
    strcpy((char *)ef_guest_memory_ptr(0x1925acu), "hotspot");
}

static void set_line(const char *line)
{
    strcpy((char *)ef_guest_memory_ptr(0x4859e0u), line);
    EF_U16(0x161eacu) = 0u;
}

static void check_token(const char *text, uint32_t expected_size, uint16_t cursor)
{
    assert(gui_script_next_token_f46cc() == expected_size);
    assert(strcmp((char *)ef_guest_memory_ptr(TOKEN), text) == 0);
    assert(EF_U16(0x161eacu) == cursor);
}

static void test_lexer(void)
{
    char long_input[96];
    reset();
    set_line("  hello\t\"two words\" ,trailer");
    check_token("hello", 5u, 7u);
    check_token("two words", 9u, 19u);
    check_token("", 0u, 20u); /* comma not consumed */
    check_token("", 0u, 20u); /* original behavior, repeated empty token */

    reset();
    set_line("\"open ending");
    check_token("open ending", 11u, 12u); /* unterminated quote */
    check_token("", 0u, 12u);

    reset();
    memset(long_input, 'x', 81u);
    long_input[81] = ' ';
    long_input[82] = 'z';
    long_input[83] = 0;
    set_line(long_input);
    assert(gui_script_next_token_f46cc() == 80u);
    assert(EF_U8(TOKEN + 80u) == 0u);
    assert(EF_U16(0x161eacu) == 80u);
    check_token("x", 1u, 81u);
    check_token("z", 1u, 83u);

    reset();
    set_line("\"a,b\" ");
    check_token("a,b", 3u, 5u);
    check_token("", 0u, 6u);
}

static void check_coordinates(const int16_t values[6])
{
    unsigned i;
    for (i = 0u; i < 6u; ++i)
        assert(EF_S16(0x485de8u + 2u * i) == values[i]);
    assert(EF_U32(0x161ea4u) == 0xf5eacu);
}

static void test_rectangle_parser(void)
{
    static const int16_t expected[6] = {-7, 12, 320, 200, 5, -6};
    unsigned i, conversion_calls = 0u, errno_calls = 0u;
    reset();
    set_line("FROM -7 12 size 320 200 hotspot 5 -6");
    gui_script_rect_f5484();
    check_coordinates(expected);
    assert(report_count == 0u);
    for (i = 0u; i < calls; ++i) {
        if (called[i] == 0x9d4cdu) ++conversion_calls;
        if (called[i] == 0xebd30u) ++errno_calls;
    }
    assert(conversion_calls == 6u && errno_calls == 12u);

    reset();
    set_line("-7 12 320 200 5 -6");
    gui_script_rect_f5484();
    check_coordinates(expected);
    assert(report_count == 0u);

    reset();
    set_line("from bad 12 size -3 nope hotspot wrong 6");
    gui_script_rect_f5484();
    assert(report_count == 4u);
    assert(reports[0].format == 0x1924e4u);
    assert(reports[1].format == 0x19257cu);
    assert(reports[2].format == 0x1925b4u);
    assert(reports[report_count - 1u].format == 0x19261cu);
    assert(reports[0].args == 3u && reports[0].token == TOKEN);
    assert(reports[0].line == 87u);
    assert(EF_S16(0x485decu) == -3);
    assert(EF_S16(0x485deeu) == 0);
}

static void set_option_names(void)
{
    static const struct { uint32_t addr; const char *name; } names[] = {
        {0x1919f8u,"backdrop"},{0x191a04u,"anim"},
        {0x191a0cu,"clickbox"},{0x191a18u,"lineclickbox"},
        {0x191a28u,"linepoint"},{0x191a34u,"boxcol"},
        {0x191a3cu,"nobox"},{0x191a44u,"nofade"},
        {0x191a4cu,"draw_all_boxes"},{0x191a5cu,"display"},
        {0x191a64u,"goto"},{0x191a6cu,"mouse"},
        {0x191a74u,"return"},{0x1919c8u,"eof"}
    };
    unsigned i;
    for (i = 0u; i < sizeof(names) / sizeof(names[0]); ++i)
        strcpy((char *)ef_guest_memory_ptr(names[i].addr), names[i].name);
}

static int called_target(uint32_t target)
{
    unsigned i;
    for (i = 0u; i < calls; ++i)
        if (called[i] == target)
            return 1;
    return 0;
}

static void test_options_dispatch(void)
{
    static const struct { const char *name; uint32_t target; uint32_t flag; } commands[] = {
        {"BACKDROP",0xf2dfcu,0u},{"anim",0xf2e54u,0u},
        {"clickbox",0xf2e74u,0u},{"lineclickbox",0xf2fbcu,0u},
        {"linepoint",0xf314cu,0u},{"boxcol",0xf35d8u,0u},
        {"nobox",0u,0x4859dbu},{"nofade",0u,0x4859dcu},
        {"draw_all_boxes",0u,0x4859ddu},{"goto",0xf3b1cu,0u},
        {"mouse",0xf3b40u,0u}
    };
    unsigned i;
    for (i = 0u; i < sizeof(commands) / sizeof(commands[0]); ++i) {
        reset();
        set_option_names();
        option_input[0] = commands[i].name;
        option_input[1] = "return";
        gui_options_dispatch_f3b78();
        assert(option_position == 2u);
        if (commands[i].target)
            assert(called_target(commands[i].target));
        if (commands[i].flag)
            assert(EF_U8(commands[i].flag) == 1u);
    }

    reset();
    set_option_names();
    option_input[0] = "unrecognized";
    option_input[1] = "eof";
    gui_options_dispatch_f3b78();
    assert(called_target(0xf3b58u));
    assert(option_position == 2u);

    reset();
    set_option_names();
    option_input[0] = "display";
    gui_options_dispatch_f3b78();
    assert(called_target(0xf3f84u) && option_position == 1u);
}

int main(void)
{
    test_lexer();
    test_rectangle_parser();
    test_options_dispatch();
    return 0;
}
