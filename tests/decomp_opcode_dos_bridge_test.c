/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_mem.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t ram[0x200000];
static char sequence[4];
static unsigned n;
static uint32_t entry_seen, index_seen, context_seen, mask_seen;
static uint8_t next_carry;

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(ram));
    return &ram[address];
}

void renderer_opcode_setup_c54xx(uint32_t code)
{
    assert(code == 0x0fu && n < 3u);
    sequence[n++] = 'S';
}
void renderer_opcode_dispatch_c54xx(ef_addr32 entry, uint32_t index)
{
    entry_seen = entry;
    index_seen = index;
    assert(n < 3u);
    sequence[n++] = 'D';
}
void renderer_opcode_install_c54xx(void)
{
    assert(n < 3u);
    sequence[n++] = 'I';
}

uint8_t dos_dispatch_int21_11f287(uint32_t *a, uint32_t *b, uint32_t *c,
                                  uint32_t *d, uint32_t *s, uint32_t *i)
{
    assert(*a == 1u && *b == 2u && *c == 3u && *d == 4u && *s == 5u && *i == 6u);
    *a = 10; *b = 20; *c = 30; *d = 40; *s = 50; *i = 60;
    return next_carry;
}
void dos_status_bridge_11f287(uint32_t context, uint32_t carry_mask)
{
    context_seen = context;
    mask_seen = carry_mask;
}

static void reset(void)
{
    memset(ram, 0, sizeof(ram));
    memset(sequence, 0, sizeof(sequence));
    n = 0;
    entry_seen = index_seen = context_seen = mask_seen = 0u;
}

static void test_opcodes(void)
{
    reset();
    renderer_toggle_opcode_a_c547f(8);
    assert(n == 3u && !memcmp(sequence, "SDI", 3));
    assert(entry_seen == 0xc5551u && index_seen == 8u);
    assert(EF_U8(0x149658u + 8u) == 0x80u);
    n = 0;
    renderer_toggle_opcode_a_c547f(8);
    assert(n == 1u && sequence[0] == 'D');
    assert(EF_U8(0x149658u + 8u) == 0u);

    n = 0;
    renderer_reset_opcode_a_c549e(8);
    assert(EF_U8(0x148e58u + 8u) == 0u);
    assert(n == 3u && !memcmp(sequence, "SDI", 3));
    assert(entry_seen == 0xc5551u);
    n = 0;
    renderer_toggle_opcode_b_c54b7(9);
    assert(entry_seen == 0xc5681u && n == 3u);
    n = 0;
    renderer_toggle_opcode_b_c54b7(9);
    assert(entry_seen == 0xc5681u && n == 1u);
    n = 0;
    renderer_reset_opcode_b_c54d6(9);
    assert(EF_U8(0x148e58u + 9u) == 4u);
    assert(entry_seen == 0xc5681u && n == 3u);
}

static void test_dos(void)
{
    unsigned i;
    reset();
    for (i = 0; i < 6u; ++i)
        EF_U32(0x200u + i * 4u) = i + 1u;
    EF_U32(0x400u) = 0xabcdefu;
    next_carry = 1u;
    assert(dos_regblock_int21_11f287(0x200u, 0x400u) == 0xabcdefu);
    for (i = 0; i < 6u; ++i)
        assert(EF_U32(0x200u + i * 4u) == (i + 1u) * 10u);
    assert(EF_U32(0x418u) == 0xffffffffu);
    assert(context_seen == 0xabcdefu && mask_seen == 0xffffffffu);

    for (i = 0; i < 6u; ++i)
        EF_U32(0x200u + i * 4u) = i + 1u;
    next_carry = 0;
    dos_regblock_int21_11f287(0x200u, 0x400u);
    assert(EF_U32(0x418u) == 0u && mask_seen == 0u);
}

int main(void)
{
    test_opcodes();
    test_dos();
    return 0;
}
