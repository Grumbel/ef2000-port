/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_gui_options_095.h"
#include "ef_mem.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t ram[0x200000];
static uint32_t heap_next;
static uint32_t released[16];
static unsigned released_count, allocation_count;
static int fail_next;
static struct { uint32_t addr, size; } allocations[32];

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(ram));
    return ram + address;
}

ef_addr32 gui_options_allocate_095(uint32_t helper, ef_addr32 old, uint32_t size)
{
    uint32_t address = heap_next;
    unsigned i;
    assert(helper == 0xfc795u || helper == 0xe8651u);
    assert(allocation_count < sizeof(allocations) / sizeof(allocations[0]));
    assert(old == 0u || helper == 0xe8651u);
    if (fail_next) {
        fail_next = 0;
        return 0u;
    }
    heap_next += (size + 15u) & ~15u;
    assert(heap_next < sizeof(ram));
    if (old != 0u) {
        for (i = 0u; i < allocation_count; ++i)
            if (allocations[i].addr == old)
                break;
        assert(i < allocation_count);
        memcpy(ef_guest_memory_ptr(address), ef_guest_memory_ptr(old),
               allocations[i].size < size ? allocations[i].size : size);
    }
    allocations[allocation_count].addr = address;
    allocations[allocation_count++].size = size;
    return address;
}

void gui_options_release_095(ef_addr32 address)
{
    assert(released_count < sizeof(released) / sizeof(released[0]));
    released[released_count++] = address;
}

static void reset(void)
{
    memset(ram, 0, sizeof(ram));
    memset(released, 0, sizeof(released));
    memset(allocations, 0, sizeof(allocations));
    heap_next = 0x20000u;
    allocation_count = released_count = 0u;
    fail_next = 0;
}

static void input(uint32_t address, const char *value)
{
    strcpy((char *)ef_guest_memory_ptr(address), value);
}

static uint32_t table(void)
{
    return EF_U32(0x160a18u);
}

static uint32_t flags(void)
{
    return EF_U32(0x160a1cu);
}

static void test_first_entry(void)
{
    reset();
    input(0x7000u, "Foo=one");
    input(0x7100u, "foo=other");
    input(0x7200u, "no-equals");
    assert(gui_options_store_f4347(0u) == -1);
    assert(gui_options_store_f4347(0x7200u) == -1);
    assert(gui_options_store_f4347(0x7000u) == 0);
    assert(table() != 0u && flags() == table() + 8u);
    assert(EF_U32(table()) == 0x7000u);
    assert(EF_U32(table() + 4u) == 0u);
    assert(EF_U8(flags()) == 0u);
    assert(gui_options_find_f44b6(0x7100u, 0) == 1);

    input(0x7300u, "missing=value");
    assert(gui_options_find_f44b6(0x7300u, 0) == -1);
}

static void test_append_overwrite_and_remove(void)
{
    reset();
    input(0x7000u, "Foo=one");
    input(0x7100u, "bar=two");
    input(0x7200u, "fOO=three");
    input(0x7300u, "FOO=");
    input(0x7400u, "BAR=anything");
    input(0x7500u, "unknown=");
    assert(gui_options_store_f4347(0x7000u) == 0);
    assert(gui_options_store_f4347(0x7100u) == 0);
    assert(allocation_count == 2u); /* move-on-grow test allocator */
    assert(EF_U32(table()) == 0x7000u);
    assert(EF_U32(table() + 4u) == 0x7100u);
    assert(EF_U32(table() + 8u) == 0u);
    assert(gui_options_find_f44b6(0x7400u, 0) == 2);
    assert(gui_options_store_f4347(0x7200u) == 0);
    assert(EF_U32(table()) == 0x7200u);
    assert(allocation_count == 2u); /* replacement needs no allocation */

    EF_U8(flags()) = 1u; /* ownership marker from original script loader */
    assert(gui_options_store_f4347(0x7300u) == 0);
    assert(released_count == 1u && released[0] == 0x7200u);
    assert(EF_U32(table()) == 0x7100u);
    assert(EF_U32(table() + 4u) == 0u);
    assert(flags() == table() + 8u);
    assert(gui_options_find_f44b6(0x7400u, 0) == 1);
    assert(gui_options_store_f4347(0x7500u) == 0);
    assert(EF_U32(table()) == 0x7100u);
}

static void test_remove_last_entry(void)
{
    reset();
    input(0x7000u, "a=one");
    input(0x7100u, "b=two");
    input(0x7200u, "B=");
    assert(gui_options_store_f4347(0x7000u) == 0);
    assert(gui_options_store_f4347(0x7100u) == 0);
    EF_U8(flags()) = 1u;
    assert(gui_options_store_f4347(0x7200u) == 0);
    assert(EF_U32(table()) == 0x7000u);
    assert(EF_U32(table() + 4u) == 0u);
    assert(flags() == table() + 8u);
    assert(EF_U8(flags()) == 1u);
    assert(released_count == 0u);
}

static void test_empty_store_and_missing_flags(void)
{
    reset();
    input(0x7000u, "empty=");
    assert(gui_options_store_f4347(0x7000u) == 0);
    assert(table() == 0u && allocation_count == 0u);

    input(0x7100u, "first=yes");
    input(0x7200u, "second=yes");
    assert(gui_options_store_f4347(0x7100u) == 0);
    EF_U32(0x160a1cu) = 0u; /* possible imported table without flags */
    assert(gui_options_store_f4347(0x7200u) == 0);
    assert(EF_U32(table()) == 0x7100u);
    assert(EF_U32(table() + 4u) == 0x7200u);
    assert(flags() == table() + 12u);
    assert(EF_U8(flags()) == 0u && EF_U8(flags() + 1u) == 0u);
}

static void test_allocation_failure(void)
{
    reset();
    input(0x7000u, "first=1");
    input(0x7100u, "second=2");
    fail_next = 1;
    assert(gui_options_store_f4347(0x7000u) == -1);
    assert(table() == 0u);
    assert(gui_options_store_f4347(0x7000u) == 0);
    fail_next = 1;
    assert(gui_options_store_f4347(0x7100u) == -1);
    assert(EF_U32(table()) == 0x7000u);
    assert(EF_U32(table() + 4u) == 0u);
}

int main(void)
{
    test_first_entry();
    test_append_overwrite_and_remove();
    test_remove_last_entry();
    test_empty_store_and_missing_flags();
    test_allocation_failure();
    return 0;
}
