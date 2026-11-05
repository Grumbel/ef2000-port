/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_environment_091.h"
#include "ef_mem.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t ram[0x500000];
static ef_environment_fields_090 values[4];
static struct { uint32_t entry, format, line; } scans[8];
static struct { uint32_t target, eax, edx, ebx; } calls[40];
static unsigned scan_count, call_count, alloc_count;
static uint32_t exists, engine_result;

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(ram));
    return &ram[address];
}

void env_scan_fields_090(uint32_t entry, uint32_t format,
                         ef_addr32 line, ef_environment_fields_090 *out)
{
    assert(scan_count < 4u);
    scans[scan_count].entry = entry;
    scans[scan_count].format = format;
    scans[scan_count].line = line;
    *out = values[scan_count];
    ++scan_count;
}

uint32_t env_engine_call_090(uint32_t target, uint32_t eax,
                            uint32_t edx, uint32_t ebx)
{
    assert(call_count < 40u);
    calls[call_count].target = target;
    calls[call_count].eax = eax;
    calls[call_count].edx = edx;
    calls[call_count].ebx = ebx;
    ++call_count;
    if (target == 0x132ab4u)
        return exists;
    if (target == 0xe29e8u)
        return 0x8000u + alloc_count++ * 0x100u;
    return engine_result;
}

static void reset(void)
{
    memset(ram, 0, sizeof(ram));
    memset(values, 0, sizeof(values));
    memset(scans, 0, sizeof(scans));
    memset(calls, 0, sizeof(calls));
    scan_count = call_count = alloc_count = 0u;
    exists = 1u;
    engine_result = 0x12345u;
    memcpy(&ram[0x100], "value\r\n", 8u);
    EF_U32(0x425af8u) = 0x100u;
    memcpy(&ram[0x190504], ".3d", 4u);
}

static void check_scan(uint32_t entry, uint32_t format)
{
    assert(scan_count == 1u);
    assert(scans[0].entry == entry && scans[0].format == format);
    assert(scans[0].line == 0x100u);
    assert(EF_U32(0x425af8u) == 0x107u);
    assert(strcmp((char *)&ram[0x100], "value") == 0);
}

static void test_darkside_palettes(void)
{
    reset();
    exists = 0u;
    env_load_darkside_palette_da748();
    check_scan(0xda748u, 0x190000u);
    assert(call_count == 4u);
    assert(calls[0].target == 0xe7cecu && calls[0].ebx == 0x190060u);
    assert(calls[1].target == 0x132ab4u);
    assert(calls[2].target == 0x132a74u && calls[2].eax == 0x190070u);
    assert(calls[3].target == 0x132ab8u && calls[3].edx == 0x421750u);

    reset();
    env_set_darkside_values_da8c4();
    check_scan(0xda8c4u, 0x190000u);
    assert(call_count == 3u);
    assert(calls[0].ebx == 0x1900a4u);
    assert(calls[2].target == 0x132ab8u && calls[2].edx == 0x425190u);
}

static void test_crater_records(void)
{
    reset();
    EF_U16(0x425bbau) = 0x5555u;
    EF_U16(0x425bbcu) = 2u;
    for (unsigned i = 0u; i < 5u; ++i)
        values[0].number[i] = i + 10u;
    env_set_crater_count_dad4c();
    check_scan(0xdad4cu, 0x190250u);
    for (unsigned i = 0u; i < 4u; ++i)
        assert(EF_U32(0x4218e0u + 36u + 4u * i) == (i + 10u) << 11);
    assert(EF_U16(0x4218e0u + 36u + 16u) == 14u);
    assert(EF_U16(0x425bbau) == 0x5555u);
    assert(EF_U16(0x425bbcu) == 3u);

    reset();
    EF_U16(0x425bbcu) = 20u;
    env_set_crater_count_dad4c();
    assert(call_count == 1u && calls[0].target == 0x132a74u);
    assert(calls[0].eax == 0x190218u);
    assert(EF_U16(0x425bbcu) == 21u);

    reset();
    values[0].number[0] = 3u;
    values[0].number[1] = 44u;
    values[0].number[2] = 5u;
    env_allocate_craters_dae40();
    check_scan(0xdae40u, 0x18ff74u);
    assert(EF_U32(0x425ae0u) == 3u);
    assert(EF_U32(0x425ae4u) == 44u);
    assert(EF_U32(0x425ae8u) == 5u);
    assert(call_count == 3u);
    assert(calls[0].target == 0x132a74u && calls[0].eax == 0x190264u);
    assert(calls[1].target == 0xe29e8u && calls[1].eax == 330u);
    assert(calls[2].target == 0xe29e8u && calls[2].eax == 550u);
    assert(EF_U32(0x425af4u) == 0x8000u);
    assert(EF_U32(0x425aecu) == 0x8100u);
}

static void test_texture_hit_and_flare(void)
{
    reset();
    EF_U32(0x425b20u) = 0x9000u;
    EF_U32(0x15bb60u) = 2u;
    values[0].number[0] = 0x33u;
    values[0].number[1] = 0x44u;
    values[0].number[2] = 2u;
    values[0].number[3] = 0x1111u;
    values[0].number[4] = 0x2222u;
    env_sun_texture_db654();
    check_scan(0xdb654u, 0x190374u);
    assert(call_count == 2u);
    assert(calls[0].target == 0xe29e8u && calls[0].eax == 16u);
    assert(calls[1].target == 0x132a74u && calls[1].eax == 0x190388u);
    assert(EF_U32(0x425b20u) == 0x8000u);
    assert(EF_U32(0x800cu) == 0x9000u);
    assert(EF_U8(0x8003u) == 0x33u && EF_U8(0x8002u) == 0x44u);
    assert(EF_U16(0x800au) == 2u && EF_U16(0x8004u) == 0x1111u);
    assert(EF_U16(0x8008u) == 1u && EF_U16(0x8006u) == 0x2222u);

    reset();
    values[0].number[0] = 11u;
    values[0].number[1] = 22u;
    values[0].number[2] = 33u;
    values[0].number[3] = 44u;
    values[0].number[4] = 55u;
    values[0].number[5] = 66u;
    EF_U32(0x425b8cu) = 1u;
    env_cannon_hit_db730();
    check_scan(0xdb730u, 0x1903a8u);
    for (unsigned i = 0u; i < 4u; ++i)
        assert(EF_U32(0x425b64u + 18u + 4u * i) == 11u * (i + 1u));
    assert(EF_U8(0x425b64u + 34u) == 55u);
    assert(EF_U8(0x425b64u + 35u) == 66u);
    assert(EF_U32(0x425b8cu) == 2u);

    reset();
    EF_U32(0x425b8cu) = 2u;
    env_cannon_hit_db730();
    assert(call_count == 1u && calls[0].eax == 0x1903bcu);
    assert(EF_U32(0x425b8cu) == 2u);

    reset();
    values[0].number[1] = 7u;
    values[0].number[2] = 8u;
    values[0].number[3] = 9u;
    values[0].number[4] = 10u;
    env_sun_flare_db28c();
    check_scan(0xdb28cu, 0x190310u);
    assert(EF_U16(0x425bb4u) == 7u);
    assert(EF_U16(0x425bb0u) == 8u);
    assert(EF_U16(0x425bb8u) == 0x2345u);
    assert(EF_U16(0x425ba2u) == 81u);
    assert(EF_U16(0x425ba4u) == 10u);
    assert(call_count == 2u && calls[0].target == 0x9ca57u);
    assert(calls[1].target == 0xde864u);
}

static void test_horizon_and_heaven(void)
{
    reset();
    memcpy(&ram[0x100], "first\r\nsecond\r\n", 15u);
    EF_U32(0x425afcu) = 2u;
    values[0].number[0] = 41u;
    values[1].number[0] = 42u;
    env_fix_horizon_db808();
    assert(scan_count == 2u && call_count == 2u);
    assert(scans[0].entry == 0xdb808u && scans[1].entry == 0xdb808u);
    assert(scans[0].format == 0x190004u && scans[1].format == 0x190004u);
    assert(strcmp((char *)&ram[scans[0].line], "first") == 0);
    assert(strcmp((char *)&ram[scans[1].line], "second") == 0);
    assert(calls[0].target == 0x2a7acu && calls[0].edx == 41u);
    assert(calls[1].edx == 42u);

    reset();
    EF_U32(0x425afcu) = UINT32_MAX;
    env_fix_horizon_db808();
    assert(scan_count == 0u && call_count == 1u);
    assert(calls[0].target == 0x132a74u && calls[0].eax == 0x190400u);

    reset();
    values[0].number[0] = 39u;
    values[0].number[1] = 5u;
    EF_U8(0x425c86u) = 0x9au;
    env_heavenly_body_db8bc();
    check_scan(0xdb8bcu, 0x19043cu);
    assert(EF_U16(0x422398u + 39u * 5u) == (uint16_t)-5);
    assert(EF_U8(0x422398u + 39u * 5u + 2u) == 0x9au);
    assert(EF_U16(0x422398u + 39u * 5u + 3u) == 0x2345u);
    assert(call_count == 1u && calls[0].target == 0x9ddfcu);

    reset();
    values[0].number[0] = 40u;
    env_heavenly_body_db8bc();
    assert(scan_count == 1u && call_count == 0u);
}

static void test_file_names_and_setsun(void)
{
    reset();
    memcpy(values[0].text, "CLOUD", 6u);
    env_init3d_file_dbff8();
    check_scan(0xdbff8u, 0x190000u);
    assert(strcmp((char *)&ram[0x17d368], "CLOUD.3d") == 0);
    assert(call_count == 1u && calls[0].target == 0x107310u);

    reset();
    env_init3d_file_dbff8();
    assert(call_count == 2u && calls[0].eax == 0x1904d4u);
    assert(calls[1].target == 0x107310u);

    reset();
    memcpy(values[0].text, "palettes", 9u);
    env_east_west_palettes_dc094();
    check_scan(0xdc094u, 0x190000u);
    assert(strcmp((char *)&ram[0x15bb64], "palettes") == 0);
    assert(call_count == 1u && calls[0].target == 0xe4010u);
    assert(calls[0].eax == 0x15bb64u);

    reset();
    memcpy(values[0].text, "thirteenchars", 14u);
    env_east_west_palettes_dc094();
    assert(call_count == 2u && calls[0].eax == 0x19050cu);

    reset();
    memcpy(&ram[0x100], "day night\r\n", 12u);
    env_setsun_dc124();
    assert(scan_count == 0u && call_count == 4u);
    assert(strcmp((char *)&ram[0x100], "day") == 0);
    assert(strcmp((char *)&ram[0x104], "night") == 0);
    assert(EF_U32(0x425af8u) == 0x10bu);
    assert(calls[0].target == 0xe7cecu && calls[0].edx == 0x100u);
    assert(calls[0].eax == 0x190064u && calls[0].ebx == 0x1900a4u);
    assert(calls[1].target == 0x132ab8u && calls[1].edx == 0x420f90u);
    assert(calls[2].target == 0xe7cecu && calls[2].edx == 0x104u);
    assert(calls[3].target == 0x132ab8u && calls[3].edx == 0x421290u);
    assert(EF_U8(0x15bb74u) == 1u);
}

int main(void)
{
    test_darkside_palettes();
    test_crater_records();
    test_texture_hit_and_flare();
    test_horizon_and_heaven();
    test_file_names_and_setsun();
    return 0;
}
