/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_environment_090.h"
#include "ef_mem.h"

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

static uint8_t ram[0x500000];
/* Scanner destinations are either offsets into ram or native stack pointers.
 * Other guest calls receive the 32-bit low word of the same stack address. */
static uintptr_t scanned_text;
static unsigned scanner_calls;
static uint32_t seen_format;
static struct { uint32_t target, eax, edx, ebx; } calls[32];
static unsigned call_count;
static uint32_t engine_return;
static ef_environment_fields_090 fixture;

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(ram));
    return &ram[address];
}

static void *scan_destination(uintptr_t destination)
{
    return destination < sizeof(ram) ? &ram[destination] : (void *)destination;
}

static char *guest_string(ef_addr32 source)
{
    if (scanned_text != 0 && source == (ef_addr32)scanned_text)
        return (char *)scanned_text;
    return ef_guest_memory_ptr(source);
}

int32_t EF_CDECL guest_fn_df26f_x(ef_addr32 line, ef_addr32 format, ...)
{
    const char *layout;
    unsigned field = 0;
    va_list outputs;

    assert(scanner_calls++ == 0u);
    seen_format = format;
    assert(strcmp(ef_guest_memory_ptr(line), "value") == 0);
    assert(EF_U32(0x425af8u) == 0x107u);
    switch (format) {
    case 0x18ff6c: layout = "dd"; break;
    case 0x18ff74: layout = "ddd"; break;
    case 0x18ffe8: layout = "ds"; break;
    case 0x18fff0: layout = "hhs"; break;
    case 0x18fffc: layout = "h"; break;
    case 0x190000: layout = "s"; break;
    case 0x1900dc: layout = "dddd"; break;
    case 0x190120: layout = "hs"; break;
    case 0x1902d8: layout = "d"; break;
    case 0x19034c: layout = "ddddd"; break;
    default: assert(0); return 0;
    }
    va_start(outputs, format);
    for (; *layout; ++layout) {
        uintptr_t destination = va_arg(outputs, uintptr_t);
        void *output = scan_destination(destination);
        if (*layout == 's') {
            scanned_text = destination;
            strcpy(output, fixture.text);
        } else if (*layout == 'h') {
            uint16_t value = (uint16_t)fixture.number[field++];
            memcpy(output, &value, sizeof(value));
        } else {
            uint32_t value = fixture.number[field++];
            memcpy(output, &value, sizeof(value));
        }
    }
    va_end(outputs);
    return (int32_t)field;
}

static void record_call(uint32_t target, uint32_t eax, uint32_t edx, uint32_t ebx)
{
    assert(call_count < sizeof(calls) / sizeof(calls[0]));
    calls[call_count].target = target;
    calls[call_count].eax = eax;
    calls[call_count].edx = edx;
    calls[call_count].ebx = ebx;
    ++call_count;
}

void guest_set_scale_pair_x(uint32_t value)
{
    record_call(0xc486cu, value, 0u, 0u);
}

uint32_t build_ramp_table_e7ea4(int32_t lo, int32_t hi)
{
    record_call(0xe7ea4u, (uint32_t)lo, (uint32_t)hi, 0u);
    return engine_return;
}

void guest_callptr1_x(ef_addr32 callback, ef_addr32 message)
{
    assert(callback == EF_U32(0x132a74u) && callback != 0u);
    record_call(0x132a74u, message, 0u, 0u);
}

void guest_callptr2_x(ef_addr32 callback, uint32_t message, uint32_t value)
{
    assert(callback == EF_U32(0x132a74u) && callback != 0u);
    record_call(0x132a74u, message, value, 0u);
}

ef_addr32 copy_string_9d749(ef_addr32 destination, ef_addr32 source)
{
    record_call(0x9d749u, destination, source, 0u);
    strcpy(ef_guest_memory_ptr(destination), guest_string(source));
    return destination;
}

ef_addr32 strcat_9ca57(ef_addr32 destination, ef_addr32 source)
{
    record_call(0x9ca57u, destination, source, 0u);
    strcat(ef_guest_memory_ptr(destination), guest_string(source));
    return destination;
}

uint32_t strlen_ds(ef_addr32 source)
{
    return (uint32_t)strlen(guest_string(source));
}

ef_addr32 uppercase_string(ef_addr32 source)
{
    char *text = guest_string(source);
    record_call(0xa4421u, source, 0u, 0u);
    for (; *text; ++text)
        *text = (char)toupper((unsigned char)*text);
    return source;
}

void load_companion_files_dbf3c(ef_addr32 destination, ef_addr32 name, ef_addr32 data)
{
    assert(strcmp(guest_string(name), fixture.text) == 0);
    record_call(0xdbf3cu, destination, name, data);
}

void init_record_pool_80d8c(ef_addr32 name)
{
    assert(strcmp(guest_string(name), fixture.text) == 0);
    record_call(0x80d8cu, name, 0u, 0u);
}

uint32_t store_word_39300(ef_addr32 name)
{
    assert(strcmp(guest_string(name), fixture.text) == 0);
    record_call(0x39300u, name, 0u, 0u);
    return 0u;
}

void nop_392fc(void) { record_call(0x392fcu, 0u, 0u, 0u); }
void nop_3930c(void) { record_call(0x3930cu, 0u, 0u, 0u); }
void clear_dwords_19bb38(void) { record_call(0x2a794u, 0u, 0u, 0u); }

/* The scanner module still exposes these seams for the unrecovered
 * environment callbacks exercised by the 091 and 092 host suites. */
void env_scan_fields_090(uint32_t entry, uint32_t format, ef_addr32 line,
                         ef_environment_fields_090 *fields)
{
    (void)entry;
    (void)format;
    (void)line;
    (void)fields;
    assert(0);
}

uint32_t env_engine_call_090(uint32_t target, uint32_t eax, uint32_t edx, uint32_t ebx)
{
    (void)target;
    (void)eax;
    (void)edx;
    (void)ebx;
    assert(0);
    return 0u;
}

static void reset(void)
{
    memset(ram, 0, sizeof(ram));
    memset(&fixture, 0, sizeof(fixture));
    scanned_text = 0;
    scanner_calls = seen_format = call_count = 0u;
    engine_return = 1u;
    memcpy(&ram[0x100], "value\r\n", 8u);
    EF_U32(0x425af8u) = 0x100u;
    EF_U32(0x132a74u) = 0x200u;
    memcpy(&ram[0x190118], "at.lbm", 7u);
}

static void scanned(uint32_t format)
{
    assert(scanner_calls == 1u);
    assert(seen_format == format);
    assert(EF_U32(0x425af8u) == 0x107u);
    assert(strcmp((char *)&ram[0x100], "value") == 0);
    assert(ram[0x106] == 0u);
}

static void test_numeric_environment(void)
{
    reset();
    fixture.number[0] = (uint32_t)(int32_t)-4;
    fixture.number[1] = 0x123u;
    env_darkside_fade_da1b4();
    scanned(0x18ff6cu);
    assert(EF_U16(0x12afd8u) == 0x123u);
    assert(call_count == 1u && calls[0].target == 0xc486cu);
    assert(calls[0].eax == (uint32_t)(int32_t)-4);

    reset();
    fixture.number[0] = 22u;
    fixture.number[1] = 11u;
    fixture.number[2] = 0x432u;
    engine_return = 0u;
    env_coltab_da224();
    scanned(0x18ff74u);
    assert(EF_U16(0x12afd8u) == 0x432u);
    assert(call_count == 2u);
    assert(calls[0].target == 0xe7ea4u && calls[0].eax == 11u && calls[0].edx == 22u);
    assert(calls[1].target == 0x132a74u && calls[1].eax == 0x18ff80u);

    reset();
    fixture.number[0] = 47u;
    strcpy(fixture.text, "cloud");
    env_altitude_da2b8();
    scanned(0x18ffe8u);
    assert(EF_U32(0x425b14u) == (uint32_t)-47);
    assert(EF_U32(0x425b18u) == 1u);
    assert(calls[0].target == 0x9d749u && calls[0].eax == 0x425b00u);

    reset();
    fixture.number[0] = 15u;
    fixture.number[1] = 4u;
    strcpy(fixture.text, ".tex");
    env_altitude2_da360();
    scanned(0x18fff0u);
    assert(EF_S16(0x425bd0u) == -15);
    assert(EF_S16(0x425baau) == -4);
    assert(EF_U16(0x425bc8u) == 1u);
    assert(calls[0].target == 0x9ca57u && calls[0].eax == 0x425490u);
    assert(strcmp((char *)&ram[0x425490], ".tex") == 0);

    reset();
    fixture.number[0] = 19u;
    fixture.number[1] = 0u;
    env_ground_da408();
    scanned(0x18fff0u);
    assert(EF_S16(0x425baeu) == -19);
    assert(EF_U16(0x425bb6u) == 0u);
    assert(EF_U16(0x425ba6u) == 1u);
    assert(calls[0].target == 0x9ca57u && calls[0].eax == 0x422348u);

    reset();
    fixture.number[0] = 200u;
    env_ceiling_da4b0();
    scanned(0x18fffcu);
    assert(EF_S16(0x425ba8u) == -200);
    assert(EF_U16(0x425bbeu) == 1u);

    reset();
    fixture.number[0] = 255u;
    env_planet_number_da518();
    scanned(0x18fffcu);
    assert(EF_S16(0x425bacu) == -255);
    assert(EF_U16(0x425bc0u) == 1u);

    reset();
    fixture.number[0] = 6u;
    env_game_type_da580();
    scanned(0x18fffcu);
    assert(EF_U16(0x425be4u) == 6u);

    /* The guest also strips the preceding byte when the terminator is
     * already NUL, not just on CR-LF input. */
    reset();
    memcpy(&ram[0x100], "valueX", 7u);
    fixture.number[0] = 0x8000u;
    env_ceiling_da4b0();
    scanned(0x18fffcu);
    assert(EF_U16(0x425ba8u) == 0x8000u);
}

static void test_world_and_names(void)
{
    reset();
    EF_U16(0x425ba0u) = 3u;
    env_load_missile_palette_da7fc();
    scanned(0x190000u);
    assert(EF_U16(0x425ba0u) == 4u);
    assert(call_count == 1u);
    assert(calls[0].target == 0xdbf3cu);
    assert(calls[0].eax == 0x4254e0u && calls[0].ebx == 0x43ff3cu);

    reset();
    EF_U16(0x425ba0u) = 5u;
    engine_return = 0u;
    env_load_missile_palette_da7fc();
    assert(EF_U16(0x425ba0u) == 6u);
    assert(call_count == 0u);

    reset();
    fixture.number[0] = 9u;
    fixture.number[1] = 10u;
    fixture.number[2] = 11u;
    fixture.number[3] = 12u;
    env_world_name_da964();
    scanned(0x1900dcu);
    assert(EF_U32(0x425b94u) == 9u);
    assert(EF_U32(0x425b90u) == 10u);
    assert(EF_U16(0x425bc6u) == 11u);
    assert(EF_U16(0x425bccu) == 12u);

    reset();
    memcpy(fixture.text, "terra", 6u);
    env_xmap_name_da9d4();
    scanned(0x190000u);
    assert(strcmp((char *)&ram[0x425bf0], "terraat.lbm") == 0);

    reset();
    fixture.number[0] = 2u;
    memcpy(fixture.text, "map", 4u);
    env_mmap_name_daa5c();
    scanned(0x190120u);
    assert(strcmp((char *)&ram[0x425bf0 + 50], "mapat.lbm") == 0);

    reset();
    memcpy(fixture.text, "anchor", 7u);
    env_ref_point_dab40();
    scanned(0x190000u);
    assert(strcmp((char *)&ram[0x425c6d], "anchorat.lbm") == 0);

    reset();
    memcpy(fixture.text, "radar", 6u);
    env_map_name_dabc8();
    scanned(0x190000u);
    assert(strcmp((char *)&ram[0x425b28], "RADAR") == 0);
    assert(call_count == 2u && calls[0].target == 0xa4421u);
}

static void test_player_and_clouds(void)
{
    reset();
    env_load_player_virtual_db0b4();
    scanned(0x190000u);
    assert(EF_U8(0x15bb5cu) == 0xffu);
    assert(call_count == 1u && calls[0].target == 0x80d8cu);

    reset();
    env_load_player_sort_db114();
    scanned(0x190000u);
    assert(call_count == 1u && calls[0].target == 0x392fcu);

    reset();
    env_load_player_db16c();
    scanned(0x190000u);
    assert(call_count == 1u && calls[0].target == 0x39300u);

    reset();
    env_set_flak_count_db1c4();
    scanned(0x190000u);
    assert(call_count == 1u && calls[0].target == 0x3930cu);

    reset();
    fixture.number[0] = 3u;
    EF_U32(0x425ae4u) = 21u;
    env_load_flak_db21c();
    scanned(0x1902d8u);
    assert(EF_U32(0x425afcu) == 3u);
    assert(call_count == 2u);
    assert(calls[0].target == 0x132a74u && calls[0].eax == 0x1902dcu);
    assert(calls[1].target == 0x2a794u);

    reset();
    EF_U32(0x15bb60u) = 1u;
    for (unsigned i = 0u; i < 5u; ++i)
        fixture.number[i] = i + 5u;
    env_cloud_fade_db420();
    scanned(0x19034cu);
    assert(EF_U32(0x15bb60u) == 2u);
    for (unsigned i = 0u; i < 4u; ++i)
        assert(EF_U32(0x421590u + 20u + 4u * i) == i + 6u);
    assert(EF_U32(0x421590u + 36u) == 4u);

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
