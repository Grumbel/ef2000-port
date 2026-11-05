/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_gui_script_095.h"
#include "ef_mem.h"

#include <stdint.h>

enum { LINE_BUFFER = 0x4859e0, LINE_CURSOR = 0x161eac,
       TOKEN = 0x485e08, LINE_NUMBER = 0x161ea8,
       OPTION_TOKEN = 0x48592c };

/* 0x000f46cc, 190 bytes. Token length is returned in EAX; DX is an
 * internally incremented 16-bit signed cursor. Quotes remove their opening
 * and closing delimiters, but commas delimit unquoted tokens without being
 * consumed. Original tokens truncate at exactly 80 bytes. */
uint32_t gui_script_next_token_f46cc(void)
{
    uint16_t cursor = EF_U16(LINE_CURSOR);
    uint32_t length = 0u;
    uint8_t ch;

    for (;;) {
        ch = EF_U8(LINE_BUFFER + (uint32_t)(int32_t)(int16_t)cursor);
        if (ch != ' ' && ch != '\t')
            break;
        cursor++;
    }

    if (ch == '"') {
        cursor++;
        for (;;) {
            ch = EF_U8(LINE_BUFFER + (uint32_t)(int32_t)(int16_t)cursor);
            if (ch == '"' || ch == 0u || length >= 80u)
                break;
            EF_U8(TOKEN + length++) = ch;
            cursor++;
        }
        if (ch == '"')
            cursor++;
    } else {
        while (ch != ' ' && ch != '\t' && ch != '\0' && ch != ',' && length < 80u) {
            EF_U8(TOKEN + length++) = ch;
            cursor++;
            ch = EF_U8(LINE_BUFFER + (uint32_t)(int32_t)(int16_t)cursor);
        }
    }
    EF_U8(TOKEN + length) = 0u;
    EF_U16(LINE_CURSOR) = cursor;
    return length;
}

static uint32_t guest(uint32_t target, uint32_t eax, uint32_t edx)
{
    return gui_script_guest_call_095(target, eax, edx);
}

static void report(uint32_t message, unsigned arguments)
{
    gui_script_report_095(message, arguments, TOKEN, EF_U32(LINE_NUMBER));
}

static int32_t read_integer(uint32_t error_format)
{
    uint32_t error;
    int32_t value;
    error = guest(0xebd30u, 0u, 0u);
    EF_U32(error) = 0u;
    value = (int32_t)guest(0x9d4cdu, TOKEN, 0u);
    error = guest(0xebd30u, 0u, 0u);
    if (EF_U32(error) != 0u)
        report(error_format, 3u);
    return value;
}

static int token_equals(uint32_t reference)
{
    return guest(0x9d524u, TOKEN, reference) == 0u;
}

/* 0x000f5484, 531 bytes. Original script syntax is:
 *   [FROM] x y [SIZE] width height [HOTSPOT] x y
 * The markers are optional, but the six numeric tokens are not. Every
 * conversion clears errno independently, unlike SIZE/FROM/BACKDROP in 094.
 * The last call to f5e6c merely writes six words and installs the handler
 * address f5eac; those effects are modeled directly here. */
void gui_script_rect_f5484(void)
{
    static const uint32_t errors[6] = {
        0x1924e4u, 0x192514u, 0x19254cu,
        0x19257cu, 0x1925b4u, 0x1925e8u
    };
    int32_t values[6];
    unsigned i;
    gui_script_next_token_f46cc();
    if (token_equals(0x191bf8u))
        gui_script_next_token_f46cc();
    values[0] = read_integer(errors[0]);
    gui_script_next_token_f46cc();
    values[1] = read_integer(errors[1]);

    gui_script_next_token_f46cc();
    if (token_equals(0x192544u))
        gui_script_next_token_f46cc();
    values[2] = read_integer(errors[2]);
    gui_script_next_token_f46cc();
    values[3] = read_integer(errors[3]);

    gui_script_next_token_f46cc();
    if (token_equals(0x1925acu))
        gui_script_next_token_f46cc();
    values[4] = read_integer(errors[4]);
    gui_script_next_token_f46cc();
    values[5] = read_integer(errors[5]);
    if (values[2] <= 0 || values[3] <= 0)
        report(0x19261cu, 2u);

    for (i = 0u; i < 6u; ++i)
        EF_U16(0x485de8u + i * 2u) = (uint16_t)values[i];
    EF_U32(0x161ea4u) = 0xf5eacu;
}

/* OPTIONS command table from f3b78; token comparator a4380 returns zero
 * for a match, the input reader f2d74 writes the command to 48592c. */
struct option_command {
    uint32_t name;
    uint32_t handler;
    uint32_t set_flag;
    int terminate;
};

static const struct option_command option_commands[] = {
    {0x1919f8u, 0xf2dfcu, 0u, 0}, /* backdrop */
    {0x191a04u, 0xf2e54u, 0u, 0}, /* anim */
    {0x191a0cu, 0xf2e74u, 0u, 0}, /* clickbox */
    {0x191a18u, 0xf2fbcu, 0u, 0}, /* lineclickbox */
    {0x191a28u, 0xf314cu, 0u, 0}, /* linepoint */
    {0x191a34u, 0xf35d8u, 0u, 0}, /* boxcol */
    {0x191a3cu, 0u, 0x4859dbu, 0}, /* nobox */
    {0x191a44u, 0u, 0x4859dcu, 0}, /* nofade */
    {0x191a4cu, 0u, 0x4859ddu, 0}, /* draw_all_boxes */
    {0x191a5cu, 0xf3f84u, 0u, 1}, /* display */
    {0x191a64u, 0xf3b1cu, 0u, 0}, /* goto */
    {0x191a6cu, 0xf3b40u, 0u, 0}, /* mouse */
    {0x191a74u, 0u, 0u, 1}, /* return */
    {0x1919c8u, 0u, 0u, 1}  /* eof */
};

/* 0x000f3b78, 476 bytes. Dispatch continues after unknown tokens (the
 * original fallback f3b58) and every nonterminal command. display, return
 * and eof terminate the loop; display invokes f3f84 before returning. */
void gui_options_dispatch_f3b78(void)
{
    for (;;) {
        unsigned i;
        int matched = 0;
        guest(0xf2d74u, 0u, 0u);
        for (i = 0u; i < sizeof(option_commands) / sizeof(option_commands[0]); ++i) {
            const struct option_command *entry = &option_commands[i];
            if (guest(0xa4380u, entry->name, OPTION_TOKEN) != 0u)
                continue;
            matched = 1;
            if (entry->handler != 0u)
                guest(entry->handler, 0u, 0u);
            if (entry->set_flag != 0u)
                EF_U8(entry->set_flag) = 1u;
            if (entry->terminate)
                return;
            break;
        }
        if (!matched)
            guest(0xf3b58u, 0u, 0u);
    }
}
