/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_mem.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t ram[0x500000];
static uint32_t last_source, last_format, last_destination;
static unsigned scan_count;
static uint32_t scan_result;

void *ef_guest_memory_ptr(ef_addr32 addr)
{
    assert(addr < sizeof(ram));
    return &ram[addr];
}

uint32_t environment_scan_value_087(ef_addr32 source, ef_addr32 format,
                                    ef_addr32 output)
{
    last_source = source;
    last_format = format;
    last_destination = output;
    ++scan_count;
    EF_U16(output) = 0xa55au; /* The unimplemented guest sscanf does this. */
    return scan_result;
}

typedef uint32_t (*Reader)(void);
struct ReaderEntry { Reader reader; ef_addr32 original, destination; };

static const struct ReaderEntry readers[] = {
    { env_craft_hit_water_db95c,      0xdb95cu, 0x425bdcu },
    { env_craft_hit_ground_db9a8,     0xdb9a8u, 0x425be0u },
    { env_cannon_hit_concrete_db9f4,  0xdb9f4u, 0x425bdau },
    { env_missile_hit_dba40,          0xdba40u, 0x425bdeu },
    { env_cannon_hit_water_dba8c,     0xdba8cu, 0x425beau },
    { env_cannon_hit_ground_dbad8,    0xdbad8u, 0x425beeu },
    { env_missile_hit_concrete_dbb24, 0xdbb24u, 0x425bd4u },
    { env_bomb_hit_dbb70,             0xdbb70u, 0x425be6u },
    { env_missile_hit_water_dbbbc,    0xdbbbcu, 0x425becu },
    { env_missile_hit_ground_dbc08,   0xdbc08u, 0x425bd6u },
    { env_bomb_hit_concrete_dbc54,    0xdbc54u, 0x425bd8u },
    { env_craft_hit_building_dbca0,   0xdbca0u, 0x425bd2u },
    { env_bomb_hit_water_dbcec,       0xdbcecu, 0x425be2u },
    { env_bomb_hit_ground_dbd38,      0xdbd38u, 0x425be8u },
    { env_depth_fade_dbd84,           0xdbd84u, 0x425bceu },
};

static void reset(void)
{
    memset(ram, 0, sizeof(ram));
    scan_count = 0u;
    scan_result = 1u;
    last_source = last_format = last_destination = 0u;
}

static void test_all_readers(void)
{
    unsigned i;
    reset();
    assert(sizeof(readers) / sizeof(readers[0]) == 15u);
    for (i = 0; i < sizeof(readers) / sizeof(readers[0]); ++i) {
        ef_addr32 source = 0x300u + i * 32u;
        memcpy(&ram[source], "123\r\nNEXT", 9u);
        EF_U32(0x425af8u) = source;
        assert(readers[i].reader() == 1u);
        assert(last_source == source);
        assert(last_format == 0x18fffcu);
        assert(last_destination == readers[i].destination);
        assert(EF_U16(readers[i].destination) == 0xa55au);
        assert(EF_U32(0x425af8u) == source + 5u);
        assert(memcmp(&ram[source], "123\0\0NEXT", 9u) == 0);
        assert(scan_count == i + 1u);
    }
}

static void test_lf_only_and_end_of_string(void)
{
    reset();
    memcpy(&ram[0x300], "ABC\nDEF", 8u);
    EF_U32(0x425af8u) = 0x300u;
    scan_result = 0u;
    assert(env_bomb_hit_dbb70() == 0u);
    /* Original writes NUL over C as well as LF. */
    assert(memcmp(&ram[0x300], "AB\0\0DEF", 7u) == 0);
    assert(EF_U32(0x425af8u) == 0x304u);
    assert(scan_count == 1u);

    reset();
    memcpy(&ram[0x400], "789", 4u);
    EF_U32(0x425af8u) = 0x400u;
    env_depth_fade_dbd84();
    /* Original also truncates NUL-terminated input by one byte. */
    assert(ram[0x400u] == '7');
    assert(ram[0x401u] == '8');
    assert(ram[0x402u] == 0u);
    assert(EF_U32(0x425af8u) == 0x404u);

    reset();
    ram[0x4ffu] = 0x7au;
    ram[0x500u] = '\n';
    EF_U32(0x425af8u) = 0x500u;
    env_missile_hit_dba40();
    /* Original empty-line case writes one byte BEFORE the starting line. */
    assert(ram[0x4ffu] == 0u);
    assert(ram[0x500u] == 0u);
    assert(EF_U32(0x425af8u) == 0x501u);
}

static void test_environment_flags(void)
{
    reset();
    env_unfix_horizon_db8a8();
    assert(EF_U8(0x425c86u) == 1u);
    env_load_horizon_db8b0();
    assert(EF_U8(0x425c86u) == 0u);

    EF_U8(0x425bc4u) = 0x80u;
    env_corridor_dbe28();
    assert(EF_U8(0x425bc4u) == 0x82u);
    assert(env_planet2_dbe30(0x1234ffe1u) == 0x123486e1u);
    assert(EF_U8(0x425bc4u) == 0x86u);
    assert(EF_U8(0x159130u) == 0x04u);
    env_space_dbe54();
    assert(EF_U8(0x425bc4u) == 0xc6u);
    assert(EF_U8(0x159130u) == 0x04u);
    env_planet2_dbe30(0u);
    assert(EF_U8(0x425bc4u) == 0xc6u);

    reset();
    EF_U8(0x159130u) = 0x80u;
    assert(env_planet2_dbe30(0xdeadbeefu) == 0xdead04efu);
    assert(EF_U8(0x425bc4u) == 4u);
    assert(EF_U8(0x159130u) == 0x84u);
}

int main(void)
{
    test_all_readers();
    test_lf_only_and_end_of_string();
    test_environment_flags();
    return 0;
}
