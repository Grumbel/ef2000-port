/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_mem.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t ram[0x200000];

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(ram));
    return &ram[address];
}

void bitstream_refill_fa332(uint32_t *source, int32_t *remaining)
{
    (void)source;
    (void)remaining;
    assert(!"unexpected bitstream refill in bounded test");
}

static void test_direct_and_escape(void)
{
    ef_addr32 src = 0x200u;
    int32_t remaining = 8;
    uint8_t buffer = 0x80u;
    memset(ram, 0, sizeof(ram));
    EF_U8(src) = 0x00u;
    EF_U8(0x162da0u) = 42u;
    assert(bitstream_decode_extended_fa16e(&buffer, &src, &remaining) == 42u);
    assert(src == 0x201u && buffer == 1u);

    src = 0x200u;
    remaining = 8;
    buffer = 0x80u;
    EF_U8(src) = 0xc0u;
    EF_U8(src + 1u) = 0x34u;
    EF_U8(0x162da1u) = 0xffu;
    assert(bitstream_decode_extended_fa16e(&buffer, &src, &remaining) == 0x333u);
    assert(src == 0x202u && remaining == 6);
}

static void test_two_streams(void)
{
    ef_addr32 src = 0x200u;
    int32_t remaining = 8;
    uint8_t buffer = 0x80u;
    memset(ram, 0, sizeof(ram));
    EF_U8(0x200u) = 1u;    /* one pair = two output positions */
    EF_U8(0x201u) = 0x05u; /* widths 1 and 2; choice 0, value 1 */
    EF_U8(0x202u) = 0xc0u; /* choice 1, value 2 */
    EF_U8(0x400u) = 0xaau;
    EF_U8(0x401u) = 0xaau;
    bitstream_decode_pair_streams_fa251(&buffer, &src, &remaining, 0x300u, 0x400u);
    assert(EF_U8(0x300u) == 1u);
    assert(EF_U8(0x301u) == 0u);
    assert(EF_U8(0x400u) == 0xaau); /* not overwritten on A-selected bit */
    assert(EF_U8(0x401u) == 2u);
    assert(EF_U8(0x1631e4u) == 1u && EF_U8(0x1631e5u) == 2u);
    assert(EF_U32(0x1631e6u) == 0);
}

int main(void)
{
    test_direct_and_escape();
    test_two_streams();
    return 0;
}
