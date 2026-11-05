/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_environment_090.h"
#include "ef_mem.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t ram[0x500000];
static ef_environment_fields_090 fixture;
static unsigned scanner_calls;
static uint32_t seen_entry, seen_format;
static struct { uint32_t target, eax, edx, ebx; } calls[32];
static unsigned call_count;
static uint32_t engine_return;

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(ram));
    return &ram[address];
}

void env_scan_fields_090(uint32_t entry, uint32_t format,
                         ef_addr32 line, ef_environment_fields_090 *out)
{
    assert(scanner_calls++ == 0u);
    seen_entry = entry;
    seen_format = format;
    assert(strcmp((char *)ef_guest_memory_ptr(line), "value") == 0);
    *out = fixture;
}

uint32_t env_engine_call_090(uint32_t target, uint32_t eax,
                            uint32_t edx, uint32_t ebx)
{
    assert(call_count < sizeof(calls) / sizeof(calls[0]));
    calls[call_count].target = target;
    calls[call_count].eax = eax;
    calls[call_count].edx = edx;
    calls[call_count].ebx = ebx;
    ++call_count;
    if (target == 0x80d8cu)
        return 0x0000a700u;
    if (target == 0x132ab4u)
        return engine_return;
    if (target == 0xe7ea4u)
        return engine_return;
    return 0u;
}

static void reset(void)
{
    memset(ram, 0, sizeof(ram));
    memset(&fixture, 0, sizeof(fixture));
    scanner_calls = seen_entry = seen_format = call_count = 0u;
    engine_return = 1u;
    memcpy(&ram[0x100], "value\r\n", 8u);
    EF_U32(0x425af8u) = 0x100u;
    memcpy(&ram[0x190118], "at.lbm", 7u);
}

static void scanned(uint32_t entry, uint32_t format)
{
    assert(scanner_calls == 1u);
    assert(seen_entry == entry);
    assert(seen_format == format);
    assert(EF_U32(0x425af8u) == 0x107u);
    assert(strcmp((char *)&ram[0x100], "value") == 0);
    assert(ram[0x106] == 0u);
}

static void test_numeric_environment(void)
{
    reset();
    fixture.number[0] = 0x123u;
    fixture.number[1] = (uint32_t)(int32_t)-4;
    env_darkside_fade_da1b4();
    scanned(0xda1b4u, 0x18ff6cu);
    assert(EF_U16(0x12afd8u) == 0x123u);
    assert(call_count == 1u && calls[0].target == 0xc486cu);
    assert(calls[0].eax == (uint32_t)(int32_t)-4);

    reset();
    fixture.number[0] = 0x432u;
    fixture.number[1] = 11u;
    fixture.number[2] = 22u;
    engine_return = 0u;
    env_coltab_da224();
    scanned(0xda224u, 0x18ff74u);
    assert(EF_U16(0x12afd8u) == 0x432u);
    assert(call_count == 2u);
    assert(calls[0].target == 0xe7ea4u && calls[0].eax == 11u);
    assert(calls[1].target == 0x132a74u && calls[1].eax == 0x18ff80u);

    reset();
    fixture.number[0] = 47u;
    fixture.number[1] = 0x220u;
    env_altitude_da2b8();
    scanned(0xda2b8u, 0x18ffe8u);
    assert(EF_U32(0x425b14u) == (uint32_t)-47);
    assert(EF_U32(0x425b18u) == 1u);
    assert(calls[0].target == 0x9d749u && calls[0].eax == 0x425b00u);

    reset();
    fixture.number[0] = 15u;
    fixture.number[1] = 4u;
    fixture.number[2] = 0x310u;
    env_altitude2_da360();
    scanned(0xda360u, 0x18fff0u);
    assert(EF_S16(0x425bd0u) == -15);
    assert(EF_S16(0x425baau) == -4);
    assert(EF_U16(0x425bc8u) == 1u);
    assert(calls[0].target == 0x9ca57u && calls[0].eax == 0x425490u);

    reset();
    fixture.number[0] = 19u;
    fixture.number[1] = 0u;
    env_ground_da408();
    scanned(0xda408u, 0x18fff0u);
    assert(EF_S16(0x425baeu) == -19);
    assert(EF_U16(0x425bb6u) == 0u);
    assert(EF_U16(0x425ba6u) == 1u);
    assert(calls[0].target == 0x9ca57u && calls[0].eax == 0x422348u);

    reset();
    fixture.number[0] = 200u;
    env_ceiling_da4b0();
    scanned(0xda4b0u, 0x18fffcu);
    assert(EF_S16(0x425ba8u) == -200);
    assert(EF_U16(0x425bbeu) == 1u);

    reset();
    fixture.number[0] = 255u;
    env_planet_number_da518();
    scanned(0xda518u, 0x18fffcu);
    assert(EF_S16(0x425bacu) == -255);
    assert(EF_U16(0x425bc0u) == 1u);

    reset();
    fixture.number[0] = 6u;
    env_game_type_da580();
    scanned(0xda580u, 0x18fffcu);
    assert(EF_U16(0x425be4u) == 6u);

    /* The guest also strips the preceding byte when the terminator is
     * already NUL, not just on CR-LF input. */
    reset();
    memcpy(&ram[0x100], "valueX", 7u);
    fixture.number[0] = 0x8000u;
    env_ceiling_da4b0();
    scanned(0xda4b0u, 0x18fffcu);
    assert(EF_U16(0x425ba8u) == 0x8000u);
}

static void test_world_and_names(void)
{
    reset();
    EF_U16(0x425ba0u) = 3u;
    env_load_missile_palette_da7fc();
    scanned(0xda7fcu, 0x190000u);
    assert(EF_U16(0x425ba0u) == 4u);
    assert(call_count == 3u);
    assert(calls[2].target == 0xdbf3cu);
    assert(calls[2].eax == 0x4254e0u && calls[2].ebx == 0x43ff3cu);

    reset();
    EF_U16(0x425ba0u) = 5u;
    engine_return = 0u;
    env_load_missile_palette_da7fc();
    assert(EF_U16(0x425ba0u) == 6u);
    assert(call_count == 3u);
    assert(calls[2].target == 0x132a74u && calls[2].eax == 0x1900a8u);

    reset();
    fixture.number[0] = 9u;
    fixture.number[1] = 10u;
    fixture.number[2] = 11u;
    fixture.number[3] = 12u;
    env_world_name_da964();
    scanned(0xda964u, 0x1900dcu);
    assert(EF_U32(0x425b94u) == 9u);
    assert(EF_U32(0x425b90u) == 10u);
    assert(EF_U16(0x425bc6u) == 11u);
    assert(EF_U16(0x425bccu) == 12u);

    reset();
    memcpy(fixture.text, "terra", 6u);
    env_xmap_name_da9d4();
    scanned(0xda9d4u, 0x190000u);
    assert(strcmp((char *)&ram[0x425bf0], "terraat.lbm") == 0);

    reset();
    fixture.number[0] = 2u;
    memcpy(fixture.text, "map", 4u);
    env_mmap_name_daa5c();
    scanned(0xdaa5cu, 0x190120u);
    assert(strcmp((char *)&ram[0x425bf0 + 50], "mapat.lbm") == 0);

    reset();
    memcpy(fixture.text, "anchor", 7u);
    env_ref_point_dab40();
    scanned(0xdab40u, 0x190000u);
    assert(strcmp((char *)&ram[0x425c6d], "anchorat.lbm") == 0);

    reset();
    memcpy(fixture.text, "radar", 6u);
    env_map_name_dabc8();
    scanned(0xdabc8u, 0x190000u);
    assert(strcmp((char *)&ram[0x425b28], "radar") == 0);
    assert(call_count == 1u && calls[0].target == 0xa4421u);
}

static void test_player_and_clouds(void)
{
    reset();
    env_load_player_virtual_db0b4();
    scanned(0xdb0b4u, 0x190000u);
    assert(EF_U8(0x15bb5cu) == 0xa7u);
    assert(call_count == 1u && calls[0].target == 0x80d8cu);

    reset();
    env_load_player_sort_db114();
    scanned(0xdb114u, 0x190000u);
    assert(call_count == 1u && calls[0].target == 0x392fcu);

    reset();
    env_load_player_db16c();
    scanned(0xdb16cu, 0x190000u);
    assert(call_count == 1u && calls[0].target == 0x39300u);

    reset();
    env_set_flak_count_db1c4();
    scanned(0xdb1c4u, 0x190000u);
    assert(call_count == 1u && calls[0].target == 0x3930cu);

    reset();
    fixture.number[0] = 3u;
    EF_U32(0x425ae4u) = 21u;
    env_load_flak_db21c();
    scanned(0xdb21cu, 0x1902d8u);
    assert(EF_U32(0x425afcu) == 3u);
    assert(call_count == 2u);
    assert(calls[0].target == 0x132a74u && calls[0].eax == 0x1902dcu);
    assert(calls[1].target == 0x2a794u);

    reset();
    EF_U32(0x15bb60u) = 1u;
    for (unsigned i = 0u; i < 5u; ++i)
        fixture.number[i] = i + 5u;
    env_cloud_fade_db420();
    scanned(0xdb420u, 0x19034cu);
    assert(EF_U32(0x15bb60u) == 2u);
    for (unsigned i = 0u; i < 4u; ++i)
        assert(EF_U32(0x421590u + 20u + 4u * i) == i + 5u);
    assert(EF_U32(0x421590u + 36u) == 8u);

    reset();
    EF_U32(0x15bb60u) = 20u;
    env_cloud_fade_db420();
    assert(call_count == 1u);
    assert(calls[0].target == 0x132a74u && calls[0].eax == 0x190334u);
    assert(EF_U32(0x421590u + 20u * 20u + 16u) == 0xffffffffu);
    assert(EF_U32(0x15bb60u) == 21u);
}

int main(void)
{
    test_numeric_environment();
    test_world_and_names();
    test_player_and_clouds();
    return 0;
}
