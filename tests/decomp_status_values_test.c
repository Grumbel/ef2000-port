/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"

#include <assert.h>
#include <string.h>

static uint8_t mem[0x200000];
static ef_addr32 f_dest, f_fmt;
static uint32_t f_value, query_ret;
static unsigned formats, released;
static ef_addr32 released_block;

void *ef_guest_memory_ptr(ef_addr32 a) { assert(a < sizeof(mem)); return mem + a; }
void format_ui_status_value(ef_addr32 d, ef_addr32 f, uint32_t v)
{
    ++formats; f_dest = d; f_fmt = f; f_value = v;
}
uint32_t guest_query_status_value(ef_addr32 d) { assert(d == 0x4000); return query_ret; }
uint16_t guest_load_halfword(ef_addr32 s) { return (uint16_t)(s >> 4); }
void guest_release_block(ef_addr32 b) { ++released; released_block = b; }

#define CHECK(call, fmt, val) \
    do { call; assert(f_dest == 0x4000 && f_fmt == (fmt) && f_value == (uint32_t)(val)); } while (0)

int main(void)
{
    mem[0x1fcb30] = 0xa5; CHECK(write_status_byte_1fcb30(0x4000), 0x188b24, 0xa5);
    mem[0x1a5c0d] = 0x80; CHECK(write_status_byte_1a5c0d(0x4000), 0x188b34, 0x80);
    mem[0x12b364] = 0xff; mem[0x12b365] = 0xff; mem[0x12b366] = 0xfe; mem[0x12b367] = 0xff;
    CHECK(write_status_high_word(0x4000), 0x188b40, -2);
    mem[0x15b7e4] = 7; CHECK(write_status_byte_15b7e4(0x4000), 0x188b4c, 7);
    query_ret = 0xdeadbeef; CHECK(write_status_queried_value(0x4000), 0x188b68, 0xdeadbeef);
    mem[0x122dc4] = 0x78; mem[0x122dc5] = 0x56; mem[0x122dc6] = 0x34; mem[0x122dc7] = 0x12;
    CHECK(write_status_dword_122dc4(0x4000), 0x188b84, 0x12345678);
    mem[0x15bdd0] = 0xfe; CHECK(write_status_byte_15bdd0(0x4000), 0x188c10, 0xfe);
    assert(formats == 7);

    assert(set_pending_mode_two() == 0);
    assert(mem[0x12b4a6] == 2 && mem[0x12b4a7] == 0 && mem[0x1fcc43] == 1);
    load_halfword_1fcaec();
    assert(mem[0x1fcaec] == 0x9d && mem[0x1fcaed] == 0x88);

    release_block_12afc4();
    assert(released == 0);
    mem[0x12afc4] = 0x34; mem[0x12afc5] = 0x12; mem[0x12afc6] = 0x1;
    release_block_12afc4();
    assert(released == 1 && released_block == 0x011234 && mem[0x12afc4] == 0 && mem[0x12afc6] == 0);
    return 0;
}
