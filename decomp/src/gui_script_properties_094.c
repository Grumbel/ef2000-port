/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_gui_script_094.h"
#include "ef_mem.h"

#include <stdint.h>

enum { CURRENT_NODE = 0x485de0, TOKEN = 0x485e08,
       SCRIPT_LINE = 0x161ea8, SCRIPT_MODE = 0x485dfc };

static uint32_t guest(uint32_t target, uint32_t eax, uint32_t edx)
{
    return gui_script_guest_call_094(target, eax, edx);
}

static void report(uint32_t format, unsigned count)
{
    gui_script_report_094(format, count, TOKEN, EF_U32(SCRIPT_LINE));
}

/* One original token reader and two decimal conversions. The error value
 * returned indirectly through ebd30 is zeroed ONLY before the first read.
 * An error on the first conversion can therefore be reported twice. */
static void read_pair(uint32_t first_error, uint32_t second_error,
                      int32_t *first, int32_t *second)
{
    ef_addr32 error_pointer;
    guest(0xf46ccu, 0u, 0u);
    error_pointer = guest(0xebd30u, 0u, 0u);
    EF_U32(error_pointer) = 0u;
    *first = (int32_t)guest(0x9d4cdu, TOKEN, 0u);
    error_pointer = guest(0xebd30u, 0u, 0u);
    if (EF_U32(error_pointer) != 0u)
        report(first_error, 3u);
    guest(0xf46ccu, 0u, 0u);
    *second = (int32_t)guest(0x9d4cdu, TOKEN, 0u);
    error_pointer = guest(0xebd30u, 0u, 0u);
    if (EF_U32(error_pointer) != 0u)
        report(second_error, 3u);
}

static void set_pair(uint32_t first_error, uint32_t second_error,
                     uint32_t missing_node, uint32_t bad_mode,
                     uint32_t mode1_first, uint32_t mode1_second,
                     uint32_t mode2_first, uint32_t mode2_second,
                     uint32_t mode3_first, uint32_t mode3_second,
                     uint32_t mode1_flag, uint32_t mode2_flag,
                     uint32_t mode3_flag, int three_modes)
{
    int32_t first, second;
    uint32_t node = EF_U32(CURRENT_NODE);
    uint16_t mode;
    if (node == 0u)
        report(missing_node, 2u);
    read_pair(first_error, second_error, &first, &second);
    mode = EF_U16(SCRIPT_MODE);
    if (mode == 1u) {
        EF_U16(node + mode1_first) = (uint16_t)first;
        EF_U16(node + mode1_second) = (uint16_t)second;
        EF_U8(mode1_flag) = 1u;
    } else if (mode == 2u) {
        EF_U16(node + mode2_first) = (uint16_t)first;
        EF_U16(node + mode2_second) = (uint16_t)second;
        EF_U8(mode2_flag) = 1u;
    } else if (three_modes && mode == 3u) {
        EF_U16(node + mode3_first) = (uint16_t)first;
        EF_U16(node + mode3_second) = (uint16_t)second;
        EF_U8(mode3_flag) = 1u;
    } else {
        report(bad_mode, 2u);
    }
}

/* 0xf4db8, 281 bytes. SIZE stores two 16-bit values, interpreted as width
 * and height, in one of three pairs of fields of the current widget. */
void gui_script_size_f4db8(void)
{
    set_pair(0x1920f0u, 0x19211cu, 0x1920c4u, 0x192148u,
             0x57u, 0x59u, 0x63u, 0x65u, 0x4bu, 0x4du,
             0x485f6fu, 0x485f6cu, 0x485f70u, 1);
}

/* 0xf4ed4, 281 bytes. FROM follows the same conversion sequence, but
 * chooses origin fields and independent presence flags. */
void gui_script_from_f4ed4(void)
{
    set_pair(0x19219cu, 0x1921c8u, 0x19216cu, 0x1921fcu,
             0x5bu, 0x5du, 0x67u, 0x69u, 0x4fu, 0x51u,
             0x485f73u, 0x485f6eu, 0x485f71u, 1);
}

/* 0xf4ff0, 249 bytes. BACKDROP allows modes 1 and 2 only. In mode 1 the
 * destination order is opposite to SIZE/FROM: second goes to +0x53 and
 * first to +0x55. Mode 2 uses second at +0x5f, first at +0x61. */
void gui_script_backdrop_f4ff0(void)
{
    set_pair(0x192250u, 0x192280u, 0x192220u, 0x1922b0u,
             0x55u, 0x53u, 0x61u, 0x5fu, 0u, 0u,
             0x485f72u, 0x485f6du, 0u, 0);
}

/* ORIGINAL BUG: strcpy at 0x9d749 copies NUL but not padding, and does not
 * enforce the preceding 32-byte limit. Caller-provided guest RAM must be
 * sufficiently large to exercise that original out-of-bounds behavior. */
static void copy_token(ef_addr32 destination)
{
    uint32_t i = 0u;
    do {
        EF_U8(destination + i) = EF_U8(TOKEN + i);
    } while (EF_U8(TOKEN + i++) != 0u);
}

static unsigned token_length(void)
{
    unsigned n = 0u;
    while (EF_U8(TOKEN + n) != 0u)
        ++n;
    return n;
}

static void read_string(uint32_t missing_node, uint32_t too_long,
                        uint32_t conflict, uint32_t offset, uint32_t flag)
{
    uint32_t node = EF_U32(CURRENT_NODE);
    if (node == 0u)
        report(missing_node, 2u);
    guest(0xf46ccu, 0u, 0u);
    if (token_length() > 32u)
        report(too_long, 3u);
    /* ORIGINAL BUG: both ACTION and POINTER examine the POINTER flag at 485f78.
     * In particular ACTION does NOT check its own flag at 485f76. */
    if (EF_U8(0x485f78u) != 0u)
        report(conflict, 3u);
    copy_token(node + offset);
    EF_U8(flag) = 1u;
}

/* 0xf50ec, 145 bytes: ACTION copies into node+8 and sets flag 0x485f76. */
void gui_script_action_f50ec(void)
{
    read_string(0x1922d4u, 0x192304u, 0x192320u,
                0x08u, 0x485f76u);
}

/* 0xf5180, 145 bytes: POINTER copies into node+0x29, sets 0x485f78. */
void gui_script_pointer_f5180(void)
{
    read_string(0x192348u, 0x192378u, 0x192398u,
                0x29u, 0x485f78u);
}
