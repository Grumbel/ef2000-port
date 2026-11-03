/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

/* Host model only: does not establish original-game equivalence. */
#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

#include <assert.h>
#include <limits.h>
#include <string.h>

#define OBJECT_ADDRESS 0x002a0000u
#define OBJECT_TYPE_STRIDE 0xe5u

int16_t g_indexed_object_count;
ef_addr32 g_indexed_object_table[258];
uint32_t g_filtered_object_lookup_calls;
int16_t g_excluded_object_index;
uint8_t g_object_type_flags[256 * OBJECT_TYPE_STRIDE];

static uint8_t object[0x150];

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address == OBJECT_ADDRESS);
    return object;
}

static void reset_fixture(void)
{
    memset(g_indexed_object_table, 0, sizeof(g_indexed_object_table));
    memset(g_object_type_flags, 0, sizeof(g_object_type_flags));
    memset(object, 0x5a, sizeof(object));
    g_indexed_object_count = 4;
    g_indexed_object_table[0] = OBJECT_ADDRESS;
    g_indexed_object_table[1] = OBJECT_ADDRESS;
    g_indexed_object_table[2] = 0xff;
    g_excluded_object_index = -1;
    g_filtered_object_lookup_calls = 0;
}

static void test_filtered_lookup(void)
{
    reset_fixture();
    assert(lookup_filtered_indexed_object(0) == OBJECT_ADDRESS);
    assert(lookup_filtered_indexed_object(1) == OBJECT_ADDRESS);
    assert(lookup_filtered_indexed_object(2) == 0); /* invalid pointer sentinel */
    assert(lookup_filtered_indexed_object(3) == 0); /* null entry */
    assert(lookup_filtered_indexed_object(4) == 0); /* beyond count */
    assert(lookup_filtered_indexed_object(-1) == 0);
    assert(lookup_filtered_indexed_object(INT16_MIN) == 0);
    assert(g_filtered_object_lookup_calls == 7);

    g_excluded_object_index = 1;
    assert(lookup_filtered_indexed_object(1) == 0);
    assert(g_filtered_object_lookup_calls == 8);

    g_excluded_object_index = -1;
    g_indexed_object_count = 257;
    g_indexed_object_table[0xff] = OBJECT_ADDRESS;
    assert(lookup_filtered_indexed_object(0xff) == 0); /* index sentinel */
    assert(g_filtered_object_lookup_calls == 9);

    g_filtered_object_lookup_calls = UINT32_MAX;
    assert(lookup_filtered_indexed_object(0) == OBJECT_ADDRESS);
    assert(g_filtered_object_lookup_calls == 0); /* original INC wraps */
}

static void test_enabled_state_writes(void)
{
    reset_fixture();
    object[0x140] = 1;
    object[0x141] = 0x21;
    object[0x142] = 0x22;
    set_indexed_object_activation(0);
    assert(object[0x140] == 1);
    assert(object[0x141] == 1);
    assert(object[0x142] == 0);
    assert(g_filtered_object_lookup_calls == 1);

    object[0x142] = 0x24;
    clear_indexed_object_activation(0);
    assert(object[0x141] == 0);
    assert(object[0x142] == 0x24); /* does not clear second flag */
    assert(g_filtered_object_lookup_calls == 2);

    object[0xe9] = 3;
    g_object_type_flags[3 * OBJECT_TYPE_STRIDE] = 1;
    object[0x12a] = 0x5a;
    object[0x13d] = 0x5a;
    object[0x13e] = 0x5a;
    set_indexed_object_state(0);
    assert(object[0x12a] == 1);
    assert(object[0x13d] == 3);
    assert(object[0x13e] == 8);
    assert(g_filtered_object_lookup_calls == 3);

    clear_indexed_object_state(0);
    assert(object[0x12a] == 0);
    assert(object[0x13d] == 3);
    assert(object[0x13e] == 8);
    assert(g_filtered_object_lookup_calls == 4);
}

static void test_guard_conditions(void)
{
    uint8_t saved[sizeof(object)];

    reset_fixture();
    object[0x140] = 0;
    memcpy(saved, object, sizeof(saved));
    set_indexed_object_activation(0);
    clear_indexed_object_activation(0);
    assert(memcmp(object, saved, sizeof(saved)) == 0);
    assert(g_filtered_object_lookup_calls == 2);

    object[0xe9] = 255;
    object[0x12a] = 0x23;
    object[0x13d] = 0x34;
    object[0x13e] = 0x45;
    set_indexed_object_state(0);
    assert(object[0x12a] == 0x23);
    assert(object[0x13d] == 0x34);
    assert(object[0x13e] == 0x45);
    assert(g_filtered_object_lookup_calls == 3);

    /* Clear does not inspect +0x140 or the type table. */
    clear_indexed_object_state(0);
    assert(object[0x12a] == 0);
    assert(g_filtered_object_lookup_calls == 4);

    reset_fixture();
    memcpy(saved, object, sizeof(saved));
    set_indexed_object_activation(-1);
    clear_indexed_object_activation(4);
    set_indexed_object_state(2);
    clear_indexed_object_state(3);
    assert(memcmp(object, saved, sizeof(saved)) == 0);
    assert(g_filtered_object_lookup_calls == 4);

    reset_fixture();
    g_excluded_object_index = 0;
    memcpy(saved, object, sizeof(saved));
    set_indexed_object_activation(0);
    set_indexed_object_state(0);
    assert(memcmp(object, saved, sizeof(saved)) == 0);
    assert(g_filtered_object_lookup_calls == 2);
}

int main(void)
{
    test_filtered_lookup();
    test_enabled_state_writes();
    test_guard_conditions();
    return 0;
}
