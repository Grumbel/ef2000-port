/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_environment_092.h"
#include "ef_mem.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t ram[0x500000];
static ef_environment_fields_090 parsed[8];
static struct { uint32_t entry, format, line; } scans[8];
static struct { uint32_t target, eax, edx, ebx; } calls[128];
static unsigned scan_count, call_count, alloc_count, lookup_count;
static uint32_t palette_match;

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(ram));
    return &ram[address];
}

void env_scan_fields_090(uint32_t entry, uint32_t format,
                         ef_addr32 line, ef_environment_fields_090 *out)
{
    assert(scan_count < 8u);
    scans[scan_count].entry = entry;
    scans[scan_count].format = format;
    scans[scan_count].line = line;
    *out = parsed[scan_count];
    ++scan_count;
}

uint32_t env_engine_call_090(uint32_t target, uint32_t eax,
                            uint32_t edx, uint32_t ebx)
{
    assert(call_count < 128u);
    calls[call_count].target = target;
    calls[call_count].eax = eax;
    calls[call_count].edx = edx;
    calls[call_count].ebx = ebx;
    ++call_count;
    if (target == 0xa4380u)
        return eax == palette_match ? 0u : 1u;
    if (target == 0xe29e8u)
        return 0x8000u + alloc_count++ * 0x100u;
    if (target == 0x9ddfcu)
        return 0x1234u + lookup_count++;
    return 0u;
}

static void reset(void)
{
    memset(ram, 0, sizeof(ram));
    memset(parsed, 0, sizeof(parsed));
    scan_count = call_count = alloc_count = lookup_count = 0u;
    palette_match = UINT32_MAX;
    memcpy(&ram[0x100], "value\r\n", 8u);
    EF_U32(0x425af8u) = 0x100u;
    for (unsigned i = 0u; i < 14u; ++i) {
        EF_U32(0x15bd40u + i * 8u) = 0x30000u + i;
        EF_U32(0x15bd44u + i * 8u) = 0xda000u + i * 4u;
    }
}

static void check_one_scan(uint32_t entry, uint32_t format)
{
    assert(scan_count == 1u);
    assert(scans[0].entry == entry && scans[0].format == format);
    assert(scans[0].line == 0x100u);
    assert(strcmp((char *)&ram[0x100], "value") == 0);
    assert(EF_U32(0x425af8u) == 0x107u);
}

static void test_palette_dispatch(void)
{
    reset();
    palette_match = 0x30005u;
    env_palette_dispatch_da5cc();
    check_one_scan(0xda5ccu, 0x190000u);
    assert(call_count == 7u);
    for (unsigned i = 0u; i < 6u; ++i) {
        assert(calls[i].target == 0xa4380u);
        assert(calls[i].eax == 0x30000u + i);
    }
    assert(calls[6].target == 0xda014u);

    reset();
    env_palette_dispatch_da5cc();
    assert(call_count == 15u);
    assert(calls[13].target == 0xa4380u);
    assert(calls[14].target == 0xdbf2cu);
}

static void test_cockpit_palette(void)
{
    reset();
    parsed[0].number[0] = 0x55667788u;
    env_cockpit_palette_da644();
    check_one_scan(0xda644u, 0x190004u);
    assert(call_count == 2u);
    assert(calls[0].target == 0xdbf3cu);
    assert(calls[0].eax == 0x422760u && calls[0].ebx == 0x433f30u);
    assert(calls[1].target == 0xdc832u);
    assert(calls[1].eax == 0x464060u);
    assert(calls[1].edx == 0x422760u && calls[1].ebx == 0x300u);
    assert(EF_U32(0x425b98u) == 0x55667788u);
    assert(EF_U32(0x424e60u) == 0x55667788u);
    assert(EF_U16(0x425bcau) == 1u);

    reset();
    EF_U16(0x425bcau) = 2u;
    parsed[0].number[0] = 0x12345u;
    env_cockpit_palette_da644();
    assert(call_count == 1u);
    assert(calls[0].target == 0xdbf3cu);
    assert(calls[0].eax == 0x422760u + 2u * 768u);
    assert(calls[0].ebx == 0x433f30u + 2u * 0x4004u);
    assert(EF_U32(0x424e60u + 2u * 4u) == 0x12345u);
    assert(EF_U16(0x425bcau) == 3u);

    reset();
    EF_U16(0x425bcau) = 12u;
    env_cockpit_palette_da644();
    assert(call_count == 1u && calls[0].target == 0x132a74u);
    assert(calls[0].eax == 0x19000cu);
    assert(EF_U16(0x425bcau) == 13u);
}

static void test_lightning_point(void)
{
    reset();
    EF_U8(0x425c87u) = 3u;
    parsed[0].number[0] = 0x1212u;
    parsed[0].number[1] = 0x3434u;
    parsed[0].number[2] = 2u;
    memcpy(parsed[0].text, "storm", 6u);
    env_lightning_point_dac2c();
    check_one_scan(0xdac2cu, 0x1901e4u);
    assert(EF_U16(0x421a48u + 27u) == 0x1212u);
    assert(EF_U16(0x421a48u + 29u) == 0x3434u);
    assert(EF_U32(0x421a48u + 31u) == 0x8000u);
    assert(EF_U8(0x421a48u + 35u) == 2u);
    assert(strcmp((char *)&ram[0x8000], "storm") == 0);
    assert(EF_U8(0x425c87u) == 4u);
    assert(call_count == 3u);
    assert(calls[0].target == 0xa4421u);
    assert(calls[1].target == 0xe29e8u && calls[1].eax == 6u);
    assert(calls[2].target == 0xb3fb0u && calls[2].ebx == 0x50u);

    reset();
    EF_U8(0x425c87u) = 255u;
    parsed[0].number[2] = 7u;
    env_lightning_point_dac2c();
    assert(EF_U8(0x425c87u) == 0u);
    assert(EF_U8(0x421a48u + 255u * 9u + 8u) == 0u);
    assert(call_count == 4u);
    assert(calls[2].target == 0x132a74u && calls[2].eax == 0x1901f4u);
    assert(calls[3].target == 0xb3fb0u);
}

static void test_smoke_rings(void)
{
    reset();
    memcpy(&ram[0x100], "alpha\r\nbeta\r\n", 13u);
    EF_U32(0x425ae4u) = 2u;
    EF_U32(0x425ae0u) = 2u;
    EF_U32(0x425ae8u) = 1u;
    EF_U32(0x425af4u) = 0x12000u;
    EF_U32(0x425aecu) = 0x13000u;
    parsed[0].number[0] = 1u;
    parsed[1].number[0] = 2u;
    env_smoke_rings_daef4();
    assert(scan_count == 2u && call_count == 6u);
    assert(scans[0].entry == 0xdaef4u && scans[1].entry == 0xdaef4u);
    assert(scans[0].format == 0x190004u && scans[1].format == 0x190004u);
    assert(strcmp((char *)&ram[scans[0].line], "alpha") == 0);
    assert(strcmp((char *)&ram[scans[1].line], "beta") == 0);
    assert(EF_U32(0x425af8u) == 0x10du);
    for (unsigned i = 0u; i < 6u; ++i)
        assert(calls[i].target == 0x9ddfcu);
    assert(EF_U16(0x12000u + 0x14u + 2u) == 0x1234u);
    assert(EF_U16(0x12000u + 110u + 0x14u + 2u) == 0x1235u);
    assert(EF_U16(0x13000u + 0x14u + 2u) == 0x1236u);
    assert(EF_U16(0x12000u + 0x14u + 4u) == 0x1237u);
    assert(EF_U16(0x12000u + 110u + 0x14u + 4u) == 0x1238u);
    assert(EF_U16(0x13000u + 0x14u + 4u) == 0x1239u);
    assert(EF_U16(0x12000u + 0x12u) == 0xffffu);
    assert(EF_U32(0x12000u + 0x6au) == 0x12000u + 110u);
    assert(EF_U32(0x12000u + 110u + 0x6au) == 0x12000u);
    assert(EF_U32(0x13000u + 0x6au) == 0x13000u);
    assert(EF_U32(0x425b88u) == 0x12000u);
    assert(EF_U32(0x425b24u) == 0x13000u);

    reset();
    EF_U32(0x425ae4u) = UINT32_MAX;
    EF_U32(0x425ae0u) = 1u;
    EF_U32(0x425ae8u) = 1u;
    EF_U32(0x425af4u) = 0x12000u;
    EF_U32(0x425aecu) = 0x13000u;
    env_smoke_rings_daef4();
    assert(scan_count == 0u && call_count == 1u);
    assert(calls[0].target == 0x132a74u && calls[0].eax == 0x19029cu);
    assert(EF_U32(0x12000u + 0x6au) == 0x12000u);
}

int main(void)
{
    test_palette_dispatch();
    test_cockpit_palette();
    test_lightning_point();
    test_smoke_rings();
    return 0;
}
