/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

/* First real game-owned startup call with synthetic image and service data. */
#define _POSIX_C_SOURCE 200809L

#include "decomp_files.h"
#include "decomp_image.h"
#include "decomp_startup.h"
#include "ef_abi.h"
#include "ef_mem.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Keep all the fixture's configuration and memory data synthetic. Recreate
 * the same ReadGenSeqs entry state for every file-format branch. */
static void setup_views_probe(void)
{
    uint32_t ch;
    /* Original atoi uses its resident ctype table, not libc's isdigit. */
    for (ch = '0'; ch <= '9'; ++ch)
        EF_U8(0x00195af0u + ch + 1u) = 0x20u;
    EF_U32(0x001264d4u) = 0x400u;
    memcpy(ef_decomp_memory_span(0x0018a810u, 12), "Memory: %sK", 12);
    memcpy(ef_decomp_memory_span(0x0018a848u, 9), "File.Dat", 9);
    memcpy(ef_decomp_memory_span(0x0018b154u, 4), "DID", 4);
    memcpy(ef_decomp_memory_span(0x00182708u, 5), "NONE", 5);
    memcpy(ef_decomp_memory_span(0x0017f3bcu, 5), "NONE", 5);
    memcpy(ef_decomp_memory_span(0x0017f3c4u, sizeof("\\views\\aid2.dat")),
           "\\views\\aid2.dat", sizeof("\\views\\aid2.dat"));
    memcpy(ef_decomp_memory_span(0x0017f3fcu, 7u), "PARENT", 7u);
    memcpy(ef_decomp_memory_span(0x0017f404u, 6u), "CHILD", 6u);
    memcpy(ef_decomp_memory_span(0x0017f40cu, 7u), "TARGET", 7u);
    memcpy(ef_decomp_memory_span(0x0017f414u, 8u), "GTARGET", 8u);
    memcpy(ef_decomp_memory_span(0x0017f41cu, 7u), "THREAT", 7u);
    memcpy(ef_decomp_memory_span(0x0017f424u, 8u), "MTHREAT", 8u);
    memcpy(ef_decomp_memory_span(0x0017f42cu, 8u), "WINGMAN", 8u);
    memcpy(ef_decomp_memory_span(0x0017f4d4u, sizeof("Seqs")),
           "Seqs", sizeof("Seqs"));
    memcpy(ef_decomp_memory_span(0x0017f4dcu, sizeof("Option")),
           "Option", sizeof("Option"));
    memcpy(ef_decomp_memory_span(0x00120d31u, sizeof("CRUISING")),
           "CRUISING", sizeof("CRUISING"));
    memcpy(ef_decomp_memory_span(0x00120d31u + 0x1eu, sizeof("DOGFIGHTING")),
           "DOGFIGHTING", sizeof("DOGFIGHTING"));
    EF_U32(0x0017f5b9u) = 0x42000000u; /* source float 32.0 */
    EF_U32(0x0017f5bdu) = 0x42480000u; /* source float 50.0 */
    EF_U16(0x00182710u) = 0x5fu;
    memcpy(ef_decomp_memory_span(0x00182714u, sizeof("\\views\\seq2.dat")),
           "\\views\\seq2.dat", sizeof("\\views\\seq2.dat"));
    memcpy(ef_decomp_memory_span(0x0018274cu, 2), "#", 2);
    EF_U32(0x001827d0u) = 0x76543210u;
    EF_U32(0x001827d4u) = 0xfedcba98u;
    memcpy(ef_decomp_memory_span(0x001827d8u, sizeof("REL_CAM")),
           "REL_CAM", sizeof("REL_CAM"));
    memcpy(ef_decomp_memory_span(0x001827e0u, sizeof("FIXED_REL_CAM")),
           "FIXED_REL_CAM", sizeof("FIXED_REL_CAM"));
    memcpy(ef_decomp_memory_span(0x001827f0u, sizeof("DYNAMIC_CAM")),
           "DYNAMIC_CAM", sizeof("DYNAMIC_CAM"));
    memcpy(ef_decomp_memory_span(0x001827fcu, sizeof("FIXED_DYNAMIC_CAM")),
           "FIXED_DYNAMIC_CAM", sizeof("FIXED_DYNAMIC_CAM"));
    memcpy(ef_decomp_memory_span(0x00182810u, sizeof("TRACK_CAM")),
           "TRACK_CAM", sizeof("TRACK_CAM"));
    memcpy(ef_decomp_memory_span(0x0018281cu, sizeof("CLOSURE_CAM")),
           "CLOSURE_CAM", sizeof("CLOSURE_CAM"));
    memcpy(ef_decomp_memory_span(0x00182828u, sizeof("Name")),
           "Name", sizeof("Name"));
    memcpy(ef_decomp_memory_span(0x00182830u, sizeof("Key")),
           "Key", sizeof("Key"));
    memcpy(ef_decomp_memory_span(0x00182834u, sizeof("Location")),
           "Location", sizeof("Location"));
    memcpy(ef_decomp_memory_span(0x00182840u, sizeof("INSIDE")),
           "INSIDE", sizeof("INSIDE"));
    memcpy(ef_decomp_memory_span(0x00182848u, sizeof("PosDamping")),
           "PosDamping", sizeof("PosDamping"));
    memcpy(ef_decomp_memory_span(0x00182854u, sizeof("AngDamping")),
           "AngDamping", sizeof("AngDamping"));
    memcpy(ef_decomp_memory_span(0x00182860u, sizeof("MinAlt")),
           "MinAlt", sizeof("MinAlt"));
    memcpy(ef_decomp_memory_span(0x00182868u, sizeof("RollLock")),
           "RollLock", sizeof("RollLock"));
    memcpy(ef_decomp_memory_span(0x00182874u, sizeof("HORIZON")),
           "HORIZON", sizeof("HORIZON"));
    memcpy(ef_decomp_memory_span(0x0018287cu, sizeof("BANKING_HORIZON")),
           "BANKING_HORIZON", sizeof("BANKING_HORIZON"));
    memcpy(ef_decomp_memory_span(0x0018288cu, sizeof("DAMPED")),
           "DAMPED", sizeof("DAMPED"));
    memcpy(ef_decomp_memory_span(0x00182894u, sizeof("RPLANE")),
           "RPLANE", sizeof("RPLANE"));
    memcpy(ef_decomp_memory_span(0x0018289cu, sizeof("OffSet")),
           "OffSet", sizeof("OffSet"));
    memcpy(ef_decomp_memory_span(0x001828a4u, sizeof("RandomOffset")),
           "RandomOffset", sizeof("RandomOffset"));
    memcpy(ef_decomp_memory_span(0x001828b4u, sizeof("InitialOffSet")),
           "InitialOffSet", sizeof("InitialOffSet"));
    memcpy(ef_decomp_memory_span(0x001828c4u, sizeof("LookAtObj")),
           "LookAtObj", sizeof("LookAtObj"));
    memcpy(ef_decomp_memory_span(0x001828d0u, sizeof("SOURCE")),
           "SOURCE", sizeof("SOURCE"));
    memcpy(ef_decomp_memory_span(0x001828d8u, sizeof("LookAt")),
           "LookAt", sizeof("LookAt"));
    memcpy(ef_decomp_memory_span(0x001828e0u, sizeof("Dist")),
           "Dist", sizeof("Dist"));
    memcpy(ef_decomp_memory_span(0x001828e8u, sizeof("XClamp")),
           "XClamp", sizeof("XClamp"));
    memcpy(ef_decomp_memory_span(0x001828f0u, sizeof("YClamp")),
           "YClamp", sizeof("YClamp"));
    memcpy(ef_decomp_memory_span(0x001828f8u, sizeof("HUDCenter")),
           "HUDCenter", sizeof("HUDCenter"));
    memcpy(ef_decomp_memory_span(0x00182904u, sizeof("HUDHeight")),
           "HUDHeight", sizeof("HUDHeight"));
    memcpy(ef_decomp_memory_span(0x00182910u, sizeof("Insts")),
           "Insts", sizeof("Insts"));
    memcpy(ef_decomp_memory_span(0x00182918u, sizeof("FrontInternalCock")),
           "FrontInternalCock", sizeof("FrontInternalCock"));
    memcpy(ef_decomp_memory_span(0x0018292cu, sizeof("BottomInfoBar")),
           "BottomInfoBar", sizeof("BottomInfoBar"));
    memcpy(ef_decomp_memory_span(0x0018293cu, sizeof("FullViewHUD")),
           "FullViewHUD", sizeof("FullViewHUD"));
    memcpy(ef_decomp_memory_span(0x00182948u, sizeof("DisplayFullTarget")),
           "DisplayFullTarget", sizeof("DisplayFullTarget"));
    memcpy(ef_decomp_memory_span(0x0018295cu, sizeof("InternalFullHUD")),
           "InternalFullHUD", sizeof("InternalFullHUD"));
    memcpy(ef_decomp_memory_span(0x0018296cu, sizeof("None")),
           "None", sizeof("None"));
    memcpy(ef_decomp_memory_span(0x00182974u, sizeof("Cockpit")),
           "Cockpit", sizeof("Cockpit"));
    memcpy(ef_decomp_memory_span(0x00182750u, sizeof("ON")),
           "ON", sizeof("ON"));
    memcpy(ef_decomp_memory_span(0x00182754u, sizeof("END")),
           "END", sizeof("END"));
    EF_U32(0x00182a05u) = 0x43360000u; /* original float 182 */
    EF_U32(0x0018278bu) = 0x42480000u; /* original float 50 */
    EF_U32(0x0018278fu) = 0x42000000u; /* original float 32 */
    EF_U32(0x00182a09u) = 0x42000000u; /* original float 32 */
    EF_U32(0x00182a0du) = 0x42c80000u; /* original float 100 */
    memcpy(ef_decomp_memory_span(0x00182794u, sizeof("BG\\views\\views2.dat")),
           "BG\\views\\views2.dat", sizeof("BG\\views\\views2.dat"));
    memcpy(ef_decomp_memory_span(0x1900u, 14), "DID=C:\\GAMES\\", 14);
    memcpy(ef_decomp_memory_span(0x0018d52cu, 3), "rb", 3);
    memcpy(ef_decomp_memory_span(0x3400u, 7), "german", 7);
    memcpy(ef_decomp_memory_span(0x3600u, 11), "EF2000.CFG", 11);
    EF_U32(0x0012b36cu) = 0x3600u;
    EF_U32(0x0012ddb0u) = 0x3400u;
    EF_U8(0x0018b1b4u) = '\\';
    EF_U32(0x00160a18u) = 0x1800u;
    EF_U32(0x00132ab4u) = 0x0009cb20u;
    EF_U32(0x1800u) = 0x1900u;
}

int main(void)
{
    const uint8_t payload[1] = { 0 };
    const uint32_t bss_end = 0x00500000u;
    ef_decomp_x32_probe x32;
    ef_decomp_arena_probe probe;
    char reason[128];

    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
    assert(ef_decomp_probe_x32(bss_end, &x32, reason, sizeof(reason)) == 0);
    assert(x32.stop_address == 0x9c220u);
    assert(x32.request_free == 15u && x32.timer_free == 8u);
    assert(x32.first_address == 0x2b0u);
    assert(x32.first_selector == 0x1234u && x32.first_length == 0xb70u);
    assert(x32.far_stub_address == 0x400100u && x32.cleanup_hook == 0xad8u);
    assert(EF_U8(0x00400100u) == 0x9au && EF_U8(0x00400105u) == 0xcfu);
    assert(x32.registered_requests == 1u);
    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
    assert(ef_decomp_probe_x32_vectors(bss_end, &x32, reason, sizeof(reason)) == 0);
    assert(x32.stop_address == 0x9c8b4u);
    assert(x32.registered_requests == 9u);
    assert(x32.registered_traps == 0u);
    assert(x32.request_free == 7u && x32.timer_free == 8u);
    assert(x32.first_address == 0x2b0u && x32.first_length == 0xb70u);
    assert(x32.last_address == 0x120cd4u);
    assert(x32.last_selector == 0x5678u && x32.last_length == 2u);
    assert(EF_U8(0x00196022u + 7u * 10u) == 0xd4u);
    assert(EF_U8(0x00196023u + 7u * 10u) == 0x0cu);
    assert(EF_U16(0x00196026u + 7u * 10u) == 0x5678u);
    assert(EF_U8(0x00196028u + 7u * 10u) == 2u);
    assert(EF_U32(0x00132a9cu) == 0u);
    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
    assert(ef_decomp_probe_x32_traps(bss_end, &x32, reason, sizeof(reason)) == 0);
    assert(x32.stop_address == 0xf43u);
    assert(x32.request_free == 7u && x32.timer_free == 8u);
    assert(x32.registered_requests == 9u && x32.registered_traps == 8u);
    assert(x32.first_trap_vector == 3u && x32.first_trap_handler == 0x9c908u);
    assert(x32.last_trap_vector == 14u && x32.last_trap_handler == 0x9c909u);
    assert(x32.saved_trap_callback == 0xd68u);
    assert(x32.saved_trap_esp == EF_DECOMP_RAM_SIZE - 0x1000u);
    assert(x32.saved_trap_ss == 0x9abcu && x32.saved_trap_ds == 0x5678u);
    assert(EF_U32(0x00132a9cu) == x32.saved_trap_callback);
    assert(EF_U32(0x00132aa0u) == x32.saved_trap_esp);
    assert(EF_U16(0x00132aa4u) == x32.saved_trap_ss);
    assert(EF_U16(0x00132aa6u) == x32.saved_trap_ds);
    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
    assert(ef_decomp_probe_x32_input(bss_end, &x32, reason, sizeof(reason)) == 0);
    assert(x32.stop_address == 0x324u);
    assert(x32.registered_requests == 11u && x32.registered_traps == 8u);
    assert(x32.request_free == 5u && x32.timer_free == 7u);
    assert(x32.last_address == 0x41f833u);
    assert(x32.last_selector == 0x5678u && x32.last_length == 0x118u);
    assert(x32.keyboard_led_bytes == 2u);
    assert(x32.keyboard_code_slot == 6u && x32.keyboard_data_slot == 5u);
    assert(x32.stored_data_selector == 0x5678u);
    assert(x32.pending_timer_source == 9u);
    assert(EF_U32(0x0041f828u) == 6u && EF_U32(0x0041f82cu) == 5u);
    assert(EF_U16(0x000fe624u) == 0x5678u);
    assert(EF_U8(0x0041f72cu) == 0x1cu);
    assert(EF_U8(0x0041f72du) == 0x1du);
    assert(EF_U8(0x0041f748u) == 0x38u);
    assert(EF_U32(0x00196140u) == 7u);
    assert(EF_U8(0x001960c4u + 7u * 14u) == 9u);
    assert(EF_U32(0x00132a9cu) == 0xd68u);
    assert(EF_U8(0x0015b72eu) == 0u);
    assert(EF_U32(0x0041f824u) == 0u);
    assert(x32.timer_registrations == 0u && x32.input_cleanup_hook == 0u);
    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
    assert(ef_decomp_probe_x32_timer(bss_end, &x32, reason, sizeof(reason)) == 0);
    assert(x32.stop_address == 0xcd550u);
    assert(x32.registered_requests == 11u && x32.registered_traps == 8u);
    assert(x32.request_free == 5u && x32.timer_free == 7u);
    assert(x32.keyboard_led_bytes == 2u);
    assert(x32.keyboard_code_slot == 6u && x32.keyboard_data_slot == 5u);
    assert(x32.pending_timer_source == 9u);
    assert(x32.timer_2503_result == 0x2468u);
    assert(x32.timer_2502_result_low == 0x12345678u);
    assert(x32.timer_2502_result_high == 0xabcdu);
    assert(x32.timer_registrations == 1u && x32.input_cleanup_hook == 0xcd554u);
    assert(x32.input_initialized == 1u);
    assert(EF_U32(0x0041f824u) == 7u);
    assert(EF_U8(0x0015b72eu) == 1u);
    assert(EF_U32(0x00132a9cu) == 0xd68u);
    assert(EF_U32(0x00196140u) == 7u);
    assert(EF_U32(0x00196144u) == 5u);
    assert(EF_U16(0x001960d0u + 7u * 14u) == 0xabcdu);
    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
    assert(ef_decomp_probe_x32_tactcom(bss_end, &x32, reason, sizeof(reason)) == 0);
    assert(x32.stop_address == 0xe28e8u);
    assert(x32.registered_requests == 11u && x32.registered_traps == 8u);
    assert(x32.request_free == 5u && x32.timer_free == 7u);
    assert(x32.keyboard_led_bytes == 2u && x32.keyboard_code_slot == 6u);
    assert(x32.keyboard_data_slot == 5u && x32.pending_timer_source == 9u);
    assert(x32.timer_registrations == 1u && x32.input_cleanup_hook == 0xcd554u);
    assert(x32.timer_2503_result == 0x2468u);
    assert(x32.timer_2502_result_low == 0x12345678u);
    assert(x32.timer_2502_result_high == 0xabcdu);
    assert(x32.input_initialized == 1u);
    assert(EF_U8(0x0015f751u) == 0u);
    assert(EF_U8(0x0015bac6u) == 0u && EF_U8(0x0015bac7u) == 0u);
    assert(EF_U8(0x00420a72u) == 0u);
    assert(EF_U32(0x0041f824u) == 7u);
    /* Cold X-32 probing is isolated; restore fresh BSS for the ordinary
     * game-main/TACTCOM startup probes below. */
    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
    EF_U32(0x001264d4u) = 0x400u;
    EF_U8(bss_end) = 0xa5u;
    assert(ef_decomp_probe_arena(bss_end, &probe, reason, sizeof(reason)) == 0);
    assert(probe.base == bss_end && probe.size == 0x400u);
    assert(probe.release_hook == 0xe2a50u && probe.hook_registrations == 1);
    assert(EF_U8(bss_end) == 0 && EF_U8(bss_end + 0x3ffu) == 0);
    assert(EF_U32(0x0015f754u) == bss_end);
    assert(EF_U32(0x0015f758u) == 0x400u);
    assert(EF_U32(0x0015f75cu) == bss_end);
    assert(EF_U32(0x0015f760u) == 0x400u);
    assert(EF_U32(0x0015f764u) == 0);
    assert(EF_U32(0x0015f768u) == 0);
    assert(ef_decomp_probe_arena(bss_end, &probe, reason, sizeof(reason)) != 0);
    assert(ef_decomp_memory_span(EF_DECOMP_RAM_SIZE - 1, 2) == 0);
    ef_decomp_memory_release();

    /* Views.c ReadGenSeqs initializes all forty 0xa1-byte slots before
     * loading the external generic-sequence resource. It copies the name
     * as one dword plus one byte, and stores the original default word. */
    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
    {
        const uint8_t name[] = {'N', 'O', 'N', 'E', 0};
        uint32_t i;
        memset(ef_decomp_memory_span(0x0019cbb0u, 40u * 0xa1u),
               0xcc, 40u * 0xa1u);
        memcpy(ef_decomp_memory_span(0x00182708u, sizeof(name)),
               name, sizeof(name));
        EF_U16(0x00182710u) = 0x5f5fu;
        init_gen_sequence_slots_30214();
        for (i = 0; i < 40u; ++i) {
            uint32_t base = 0x0019cbb0u + i * 0xa1u;
            assert(EF_U16(base) == 0x5f5fu);
            assert(EF_U8(base + 2u) == 0xccu);
            assert(EF_U8(base + 0x3fu) == 0xccu);
            assert(memcmp(ef_decomp_memory_span(base + 0x40u, 5u),
                          name, sizeof(name)) == 0);
            assert(EF_U8(base + 0x45u) == 0xccu);
            assert(EF_U8(base + 0xa0u) == 0xccu);
        }
        assert(EF_U8(0x0019cbb0u + 40u * 0xa1u) == 0u);
    }
    ef_decomp_memory_release();

    /* Recovered type-3 parser reads real guest descriptor layout (18-byte
     * stride); use synthetic keys, not original executable bytes. Test
     * byte/word/dword stores, case folding and the 0x82710 CONTROL hook. */
    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
    {
        struct descriptor {
            const char *key;
            uint32_t destination;
            uint8_t width;
            uint32_t callback;
            uint8_t type;
        } descriptors[] = {
            {"DETAIL", 0x0015b7e4u, 1u, 0u, 3u},
            {"REZ", 0x0012b369u, 1u, 0u, 3u},
            {"CONTROL", 0x001a5c0bu, 1u, 0x00082710u, 3u},
            {"HUDCOL", 0x0012b366u, 2u, 0u, 3u},
            {"JOY_FACTOR", 0x00122dc4u, 4u, 0u, 3u},
            {"MUSIC", 0x001fcb50u, 0x50u, 0x0008289cu, 5u},
            {"SOUNDFX", 0x001fcb50u, 0x50u, 0x00082814u, 5u},
            {"JOYSTICK", 0x001fcb50u, 0x50u, 0x000828f8u, 5u},
            {"SPEECH", 0x001fcb50u, 0x50u, 0x000826f0u, 5u},
            {"VRHELMET", 0x001fcb50u, 0x50u, 0x00082c34u, 5u},
        };
        size_t i;
        const uint32_t table = 0x0012b370u;
        const uint32_t key_base = 0x00017000u;
        const uint32_t text = 0x00018000u;
        EF_U32(0x00464380u) = table;
        for (i = 0; i < sizeof(descriptors) / sizeof(descriptors[0]); i++) {
            uint32_t entry = table + (uint32_t)i * 18u;
            uint32_t name = key_base + (uint32_t)i * 32u;
            memcpy(ef_decomp_memory_span(name, strlen(descriptors[i].key) + 1u),
                   descriptors[i].key, strlen(descriptors[i].key) + 1u);
            EF_U32(entry) = name;
            EF_U8(entry + 4u) = descriptors[i].type;
            EF_U32(entry + 5u) = descriptors[i].destination;
            EF_U8(entry + 9u) = descriptors[i].width;
            EF_U32(entry + 14u) = descriptors[i].callback;
        }

        memcpy(ef_decomp_memory_span(text, 11), "dEtAiL=2\r\n", 11);
        EF_U32(0x00464384u) = text;
        EF_U32(0x0046438cu) = 0xdeadbeefu;
        assert(parse_scalar_setting_e882c() == 1u);
        assert(EF_U8(0x0015b7e4u) == 2u);
        assert(strcmp(ef_decomp_memory_span(0x00464396u, 8), "dEtAiL") == 0);
        assert(EF_U32(0x00464384u) == text + 7u);
        assert(EF_U32(0x0046438cu) == 2u);

        memcpy(ef_decomp_memory_span(text, 8), "REZ=-1\r\n", 8);
        EF_U32(0x00464384u) = text;
        assert(parse_scalar_setting_e882c() == 1u);
        assert(EF_U8(0x0012b369u) == 0xffu);
        assert(EF_U32(0x0046438cu) == 0xffffffffu);

        memcpy(ef_decomp_memory_span(text, 12), "CONTROL=1\r\n", 12);
        EF_U32(0x00464384u) = text;
        assert(parse_scalar_setting_e882c() == 1u);
        assert(EF_U8(0x001a5c0bu) == 1u);
        assert(EF_U8(0x001fcb30u) == 1u);
        assert(EF_U8(0x00420960u) == 1u);

        memcpy(ef_decomp_memory_span(text, 18), "HUDCOL = 65535\r\n", 17);
        EF_U32(0x00464384u) = text;
        assert(parse_scalar_setting_e882c() == 1u);
        assert(EF_U16(0x0012b366u) == 65535u);

        memcpy(ef_decomp_memory_span(text, 24), "JOY_FACTOR=-2147483648\r\n", 24);
        EF_U32(0x00464384u) = text;
        assert(parse_scalar_setting_e882c() == 1u);
        assert(EF_U32(0x00122dc4u) == 0x80000000u);

        memcpy(ef_decomp_memory_span(text, 14), "MUSIC=1,127\r\n", 13);
        EF_U32(0x00464384u) = text;
        assert(parse_scalar_setting_e882c() == 0u);
        assert(parse_text_setting_e882c() == 1u);
        assert(EF_U8(0x00420a72u) == 1u);
        assert(EF_U8(0x00420a71u) == 127u);
        assert(EF_U16(0x00464394u) == 5u);
        assert(strcmp(ef_decomp_memory_span(0x001fcb50u, 6), "1,127") == 0);
        assert(strcmp(ef_decomp_memory_span(0x004643b7u, 6), "1,127") == 0);
        assert(EF_U32(0x00464384u) == text + 6u);

        memcpy(ef_decomp_memory_span(text, sizeof("SOUNDFX=0,200\r\n")),
               "SOUNDFX=0,200\r\n", sizeof("SOUNDFX=0,200\r\n"));
        EF_U32(0x00464384u) = text;
        assert(parse_text_setting_e882c() == 1u);
        assert(EF_U8(0x00420e10u) == 127u);
        assert(EF_U8(0x0015bac6u) == 0u);
        assert(EF_U8(0x001a8581u) == 0u);
        assert(EF_U8(0x0015b7e5u) == 0xffu);

        memcpy(ef_decomp_memory_span(text, sizeof("SOUNDFX=2,-10\r\n")),
               "SOUNDFX=2,-10\r\n", sizeof("SOUNDFX=2,-10\r\n"));
        EF_U32(0x00464384u) = text;
        assert(parse_text_setting_e882c() == 1u);
        assert(EF_U8(0x00420e10u) == 0u);
        assert(EF_U8(0x0015bac6u) == 2u);
        assert(EF_U8(0x001a8581u) == 2u);
        assert(EF_U8(0x0015b7e5u) == 1u);

        memcpy(ef_decomp_memory_span(text, sizeof("MUSIC=1,32768\r\n")),
               "MUSIC=1,32768\r\n", sizeof("MUSIC=1,32768\r\n"));
        EF_U32(0x00464384u) = text;
        assert(parse_text_setting_e882c() == 0u);
        assert(EF_U8(0x00420a71u) == 127u);

        memcpy(ef_decomp_memory_span(text, sizeof("MUSIC=invalid\r\n")),
               "MUSIC=invalid\r\n", sizeof("MUSIC=invalid\r\n"));
        EF_U32(0x00464384u) = text;
        assert(parse_text_setting_e882c() == 0u);

        /* Full shipped calibration list. Last field is scanf %hd; the
         * indirect callback resolves to 0xd6960 in the pinned image. */
        EF_U32(0x0015bab8u) = 0x000d6960u;
        memcpy(ef_decomp_memory_span(text, sizeof("JOYSTICK=4000,63,63,63,63,0,126,0,126,63,0\r\n")),
               "JOYSTICK=4000,63,63,63,63,0,126,0,126,63,0\r\n",
               sizeof("JOYSTICK=4000,63,63,63,63,0,126,0,126,63,0\r\n"));
        EF_U32(0x00464384u) = text;
        assert(parse_text_setting_e882c() == 1u);
        assert(EF_U32(0x0015baacu) == 126u);
        assert(EF_U32(0x001fcba8u) == 126u);
        assert(EF_U32(0x001fcba4u) == 126u);
        assert(EF_U32(0x001fcba0u) == 4000u);
        assert(EF_U16(0x00420d9au) == 126u);
        assert(EF_U16(0x00420d9cu) == 126u);
        assert(EF_U16(0x00420d9eu) == 1u);
        assert(EF_U16(0x00420d96u) == 1u);
        assert(EF_U16(0x00420d94u) == 1u);
        assert(EF_U16(0x00420d90u) == 1u);
        assert(EF_U16(0x00198450u) == 63u);
        assert(EF_U16(0x00198448u) == 63u);
        assert(EF_U16(0x00198446u) == (uint16_t)-3937);
        assert(EF_U16(0x0019844cu) == 4000u);
        assert(EF_U16(0x00198432u) == 0u);
        assert(EF_U16(0x00464394u) == strlen("4000,63,63,63,63,0,126,0,126,63,0"));

        memcpy(ef_decomp_memory_span(text, sizeof("SPEECH=1\r\n")),
               "SPEECH=1\r\n", sizeof("SPEECH=1\r\n"));
        EF_U32(0x00464384u) = text;
        assert(parse_text_setting_e882c() == 1u);
        assert(EF_U8(0x0015bac7u) == 1u);
        assert(EF_U8(0x001a8580u) == 1u);

        memcpy(ef_decomp_memory_span(text, sizeof("VRHELMET=0\r\n")),
               "VRHELMET=0\r\n", sizeof("VRHELMET=0\r\n"));
        EF_U32(0x00464384u) = text;
        assert(parse_text_setting_e882c() == 1u);
        assert(EF_U16(0x00464470u) == 0u);
        assert(EF_U16(0x00464472u) == 0u);

        memcpy(ef_decomp_memory_span(text, sizeof("VRHELMET=7\r\n")),
               "VRHELMET=7\r\n", sizeof("VRHELMET=7\r\n"));
        EF_U32(0x00464384u) = text;
        assert(parse_text_setting_e882c() == 1u);
        assert(EF_U16(0x00464470u) == 2u);
        assert(EF_U16(0x00464472u) == 3u);

        memcpy(ef_decomp_memory_span(text, sizeof("JOYSTICK=4000,63\r\n")),
               "JOYSTICK=4000,63\r\n", sizeof("JOYSTICK=4000,63\r\n"));
        EF_U32(0x00464384u) = text;
        assert(parse_text_setting_e882c() == 0u);
        assert(EF_U32(0x00464384u) == text);

        memcpy(ef_decomp_memory_span(text, 20), "DETAIL=2147483648\r\n", 20);
        EF_U32(0x00464384u) = text;
        assert(parse_scalar_setting_e882c() == 0u);
        assert(EF_U8(0x0015b7e4u) == 2u);
        memcpy(ef_decomp_memory_span(text, 16), "DETAIL=42junk\r\n", 16);
        EF_U32(0x00464384u) = text;
        assert(parse_scalar_setting_e882c() == 0u);
        assert(EF_U8(0x0015b7e4u) == 2u);
    }
    ef_decomp_memory_release();

    /* Normal game main enters the real recovered arena setup, then the
     * missing TACTCOM initialization boundary. */
    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
    EF_U32(0x001264d4u) = 0x400u;
    memcpy(ef_decomp_memory_span(0x0018a810u, 12), "Memory: %sK", 12);
    memcpy(ef_decomp_memory_span(0x0018a848u, 9), "File.Dat", 9);
    memcpy(ef_decomp_memory_span(0x0018b154u, 4), "DID", 4);
    memcpy(ef_decomp_memory_span(0x1900u, 14), "DID=C:\\GAMES\\", 14);
    EF_U32(0x00160a18u) = 0x1800u;
    EF_U32(0x00132ab4u) = 0x0009cb20u;
    EF_U32(0x1800u) = 0x1900u;
    EF_U32(0x1804u) = 0;
    {
        uint32_t stop = 0;
        assert(ef_decomp_probe_main(bss_end, 1, 0, NULL, &probe, &stop,
                                    reason, sizeof(reason)) == 0);
        assert(stop == 0xb3b90u && probe.base == bss_end);
        assert(strcmp(ef_decomp_memory_span(EF_DECOMP_RAM_SIZE - 0x1f60u, 80),
                      "c:\\games\\File.Dat") == 0);
        assert(EF_U32(0x00196148u) == EF_DECOMP_RAM_SIZE - bss_end - 0x400u);
        assert(EF_U32(0x00196160u) == 0);
        assert(EF_U32(0x00132a74u) == 0x000ea50cu);
        assert(EF_U32(0x00132a78u) == 0x0018a838u);
    }
    ef_decomp_memory_release();

    /* Header reader: exercise the same recovered 0xb3b90 body for JR, RA,
     * unrecognized magic and absent files through the read-only Linux file
     * adapter. The temporary fixture is created outside game assets. */
    {
        char dirname[] = "/tmp/ef2000-decomp-XXXXXX";
        char game_dir[128];
        char filename[128];
        unsigned char header[8] = {'J', 'R', 0, 0, 7, 0, 0, 0};
        FILE *file;
        assert(mkdtemp(dirname));
        assert(snprintf(game_dir, sizeof(game_dir), "%s/GAMES", dirname) <
               (int)sizeof(game_dir));
        assert(mkdir(game_dir, 0700) == 0);
        assert(snprintf(filename, sizeof(filename), "%s/FILE.DAT", game_dir) <
               (int)sizeof(filename));
        file = fopen(filename, "wb");
        assert(file && fwrite(header, 1, sizeof(header), file) == sizeof(header));
        assert(fclose(file) == 0);

        assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
        memcpy(ef_decomp_memory_span(0x0018d52cu, 3), "rb", 3);
        memcpy(ef_decomp_memory_span(0x3000u, 18), "c:\\games\\File.Dat", 18);
        assert(ef_decomp_files_begin(dirname) == 0);
        assert(read_file_payload_size_b3b90(0x3000u) == 23u);
        assert(ef_decomp_files_read_plain(0x3000u, 0x4100u, 8u) == -1);
        assert(EF_U32(0x001371a4u) == 0);
        header[0] = 'R'; header[1] = 'A';
        file = fopen(filename, "wb");
        assert(file && fwrite(header, 1, sizeof(header), file) == sizeof(header));
        assert(fclose(file) == 0);
        {
            uint32_t observed = read_file_payload_size_b3b90(0x3000u);
            if (observed != 7u)
                fprintf(stderr, "RA size=%u, magic=0x%x, header=%u\n",
                        observed, EF_U16(0x0041f360u), EF_U32(0x0041f364u));
            assert(observed == 7u);
        }
        header[0] = 'X';
        file = fopen(filename, "wb");
        assert(file && fwrite(header, 1, sizeof(header), file) == sizeof(header));
        assert(fclose(file) == 0);
        assert(read_file_payload_size_b3b90(0x3000u) == 8u);
        assert(ef_decomp_files_read_plain(0x3000u, 0x4100u, 8u) == 1);
        assert(memcmp(ef_decomp_memory_span(0x4100u, 8), header, 8) == 0);
        memcpy(ef_decomp_memory_span(0x3000u, 9), "..\\escape", 9);
        assert(read_file_payload_size_b3b90(0x3000u) == 0u);
        memcpy(ef_decomp_memory_span(0x3000u, 8), "NOTHERE", 8);
        assert(read_file_payload_size_b3b90(0x3000u) == 0u);
        ef_decomp_files_end();
        ef_decomp_memory_release();

        /* The original callback concatenates the selected language prefix
         * directly with NAME. For EF2000.CFG (no leading slash), its lookup
         * misses german/EF2000.CFG and falls back to the base config. */
        {
            char localized_dir[128];
            char localized_path[160];
            char base_path[160];
            const char localized_cfg[] = "LANGUAGE=DE";
            const char base_cfg[] = "LANGUAGE=EN";
            assert(snprintf(localized_dir, sizeof(localized_dir), "%s/german", dirname) <
                   (int)sizeof(localized_dir));
            assert(mkdir(localized_dir, 0700) == 0);
            assert(snprintf(localized_path, sizeof(localized_path), "%s/EF2000.CFG",
                            localized_dir) < (int)sizeof(localized_path));
            file = fopen(localized_path, "wb");
            assert(file && fwrite(localized_cfg, 1, sizeof(localized_cfg) - 1, file) ==
                   sizeof(localized_cfg) - 1);
            assert(fclose(file) == 0);
            assert(snprintf(base_path, sizeof(base_path), "%s/EF2000.CFG", dirname) <
                   (int)sizeof(base_path));
            file = fopen(base_path, "wb");
            assert(file && fwrite(base_cfg, 1, sizeof(base_cfg) - 1, file) ==
                   sizeof(base_cfg) - 1);
            assert(fclose(file) == 0);
        }

        /* Missing resource enters the diagnostic callback; with an initial
         * resource the original configuration loader finds the base file. */
        assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
        EF_U32(0x001264d4u) = 0x400u;
        memcpy(ef_decomp_memory_span(0x0018a810u, 12), "Memory: %sK", 12);
        memcpy(ef_decomp_memory_span(0x0018a848u, 9), "File.Dat", 9);
        memcpy(ef_decomp_memory_span(0x0018b154u, 4), "DID", 4);
        memcpy(ef_decomp_memory_span(0x00182708u, 5), "NONE", 5);
        EF_U16(0x00182710u) = 0x5fu;
        memcpy(ef_decomp_memory_span(0x00182794u, sizeof("BG\\views\\views2.dat")),
               "BG\\views\\views2.dat", sizeof("BG\\views\\views2.dat"));
        memcpy(ef_decomp_memory_span(0x1900u, 14), "DID=C:\\GAMES\\", 14);
        memcpy(ef_decomp_memory_span(0x0018d52cu, 3), "rb", 3);
        memcpy(ef_decomp_memory_span(0x3400u, 7), "german", 7);
        memcpy(ef_decomp_memory_span(0x3600u, 11), "EF2000.CFG", 11);
        EF_U32(0x0012b36cu) = 0x3600u;
        EF_U32(0x0012ddb0u) = 0x3400u;
        EF_U8(0x0012ddb4u) = 0x5au;
        EF_U8(0x0018b1b4u) = '\\';
        memset(ef_decomp_memory_span(0x0015ba2cu, 4), 0xa5, 4);
        EF_U32(0x00160a18u) = 0x1800u;
        EF_U32(0x00132ab4u) = 0x0009cb20u;
        EF_U32(0x1800u) = 0x1900u;
        {
            uint32_t stop = 0;
            assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe, &stop,
                                        reason, sizeof(reason)) == 0);
            assert(stop == 0xe882cu);
            assert(EF_U32(0x001371a4u) == 0);
            assert(EF_U32(0x00132ab4u) == 0x0009cf74u);
            assert(EF_U32(0x00132ab8u) == 0x0009cfa4u);
            assert(strcmp(ef_decomp_memory_span(0x0040d150u, 16),
                          "\\german") == 0);
            assert(EF_U8(0x001371efu) == 0x5au);
            assert(EF_U32(0x0015ba2cu) == 0);
            assert(EF_U32(0x00464390u) == 0x3600u);
            assert(EF_U32(0x001600a0u) != 0);
            assert(EF_U8(0x001600a4u) == 1);
            assert(strcmp(ef_decomp_memory_span(EF_U32(0x001600a0u), 12),
                          "LANGUAGE=EN") == 0);
            assert(EF_U32(0x00464380u) == 0x0012b370u);
            assert(EF_U32(0x00464384u) == EF_U32(0x001600a0u));
            assert(EF_U16(0x0016007cu) == 1u);
            assert(EF_U8(0x001600a6u) == 0u);
        }
        ef_decomp_memory_release();
        /* Comment and whitespace lines exercise the complete outer parser,
         * the bounded comment-only branch of 0xe882c, and 0xe9574 release.
         * Three CRLF lines advance the guest line counter. */
        {
            char cfg_path[160];
            const char whitespace[] = ";ignored\r\n \t\r\n\r\n";
            uint32_t stop = 0;
            uint32_t buffer;
            assert(snprintf(cfg_path, sizeof(cfg_path), "%s/EF2000.CFG", dirname) <
                   (int)sizeof(cfg_path));
            file = fopen(cfg_path, "wb");
            assert(file && fwrite(whitespace, 1, sizeof(whitespace) - 1, file) ==
                   sizeof(whitespace) - 1);
            assert(fclose(file) == 0);
            assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
            setup_views_probe();
            assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe, &stop,
                                        reason, sizeof(reason)) == 0);
            assert(stop == 0xea50cu);
            assert(EF_U32(EF_DECOMP_RAM_SIZE - 0x2000u + 0x600u + 0x84u) ==
                   UINT32_MAX);
            assert(EF_U16(0x0019cbb0u) == 0x5fu);
            assert(EF_U16(0x0019cbb0u + 39u * 0xa1u) == 0x5fu);
            assert(strcmp(ef_decomp_memory_span(0x0019cbf0u, 5u), "NONE") == 0);
            assert(strcmp(ef_decomp_memory_span(0x0019cbf0u + 39u * 0xa1u, 5u),
                          "NONE") == 0);
            buffer = EF_U32(0x001600a0u);
            assert(buffer != 0u);
            assert(EF_U32(0x00464380u) == 0x0012b370u);
            assert(EF_U32(0x00464384u) == buffer + sizeof(whitespace) - 1u);
            assert(EF_U16(0x0016007cu) == 4u);
            assert(EF_U8(0x001600a4u) == 2u);
            assert(EF_U8(0x001600a6u) == 0u);
            assert(EF_U32(0x0046438cu) == 0u);
            assert(EF_U8(0x004643b7u) == 0u);
            assert(EF_U16(0x00464394u) == 0u);
            assert(EF_U32(0x0015f764u) == 0u);
            assert(EF_U32(0x0015f768u) == 0u);
            assert(EF_U32(0x0015f754u) == bss_end);
            ef_decomp_memory_release();
        }
        /* With a synthetic plain sequence resource the recovered opener
         * returns game slot 100, with the file position rewound to zero.
         * Recovered 0x9d114 allocates its stream record, and 0x9d170,
         * 0x9d1e8 and 0x9d278 read the first physical line/token. The
         * original '#' case initializes slot zero, reads the name and the
         * second token of the next line as its camera mode, then enters
         * its line-oriented option parser. */
        {
            char bg_dir[160], views_dir[180], views_path[220];
            const uint8_t plain[] = "# x\nMODE REL_CAM\nEND\n";
            const uint8_t packed[] = {'J', 'R', 0, 0, 8, 0, 0, 0};
            const uint8_t packed_ra[] = {'R', 'A', 0, 0, 8, 0, 0, 0};
            uint32_t stop;
            assert(snprintf(bg_dir, sizeof(bg_dir), "%s/BG", game_dir) <
                   (int)sizeof(bg_dir));
            assert(snprintf(views_dir, sizeof(views_dir), "%s/views", bg_dir) <
                   (int)sizeof(views_dir));
            assert(snprintf(views_path, sizeof(views_path), "%s/views2.dat", views_dir) <
                   (int)sizeof(views_path));
            assert(mkdir(bg_dir, 0700) == 0);
            assert(mkdir(views_dir, 0700) == 0);
            file = fopen(views_path, "wb");
            assert(file && fwrite(plain, 1, sizeof(plain) - 1u, file) ==
                   sizeof(plain) - 1u);
            assert(fclose(file) == 0);
            assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
            setup_views_probe();
            stop = 0;
            assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe, &stop,
                                        reason, sizeof(reason)) == 0);
            assert(stop == 0xea50cu);
            /* EOF passes through both original release/close call sites. */
            assert(EF_U32(0x001371a4u) == 0u);
            assert(EF_U32(EF_DECOMP_RAM_SIZE - 0x2000u + 0x600u + 0x84u) == 100u);
            {
                uint32_t stream = probe.base + probe.size;
                assert(EF_U8(stream) == 0u);
                assert(EF_U8(stream + 0x400u) == 0u);
                assert(EF_U32(stream + 0x552u) == 4u);
                assert(EF_U16(stream + 0x556u) == 0u);
                /* The last probe read refills an empty buffer at EOF. */
                assert(EF_U32(stream + 0x558u) == 0u);
                assert(EF_U32(stream + 0x55cu) == 1u);
                assert(EF_U32(stream + 0x560u) == 100u);
                assert(EF_U32(stream + 0x564u) == 0u);
                assert(EF_U32(stream + 0x568u) == 0x0009d084u);
            }
            {
                const uint32_t rec = 0x0019cbb0u;
                assert(strcmp(ef_decomp_memory_span(rec, 2u), "x") == 0);
                assert(EF_U32(rec + 0x1eu) == 0x76543210u);
                assert(EF_U32(rec + 0x22u) == 0xfedcba98u);
                assert(EF_U8(rec + 0x3du) == 0u);
                assert(EF_U8(rec + 0x3cu) == 0u);
                assert(EF_U8(rec + 0x3eu) == 0x64u);
                assert(EF_U8(rec + 0x3fu) == 0x64u);
                assert(strcmp(ef_decomp_memory_span(rec + 0x40u, 5u), "NONE") == 0);
                assert(EF_U32(rec + 0x4fu) == 0xfffffd00u);
                assert(EF_U8(rec + 0x53u) == 3u);
                assert(EF_U32(rec + 0x54u) == 0u);
                assert(EF_U32(rec + 0x58u) == 0u);
                assert(EF_U32(rec + 0x5cu) == 0u);
                assert(EF_U32(rec + 0x60u) == 0u);
                assert(EF_U32(rec + 0x64u) == 0u);
                assert(EF_U32(rec + 0x68u) == 0u);
                assert(EF_U32(rec + 0x78u) == 0u);
                assert(EF_U8(rec + 0x7cu) == 0u);
                assert(EF_U16(rec + 0x7du) == 0u);
                assert(EF_U16(rec + 0x7fu) == 0u);
                assert(EF_U16(rec + 0x81u) == 0u);
                assert(EF_U16(rec + 0x83u) == 0u);
                assert(EF_U16(rec + 0x85u) == 0u);
                assert(EF_U16(rec + 0x87u) == 0x280u);
                assert(EF_U16(rec + 0x89u) == 0x140u);
                assert(EF_U16(rec + 0x8bu) == 0x640u);
                assert(EF_U16(rec + 0x8du) == 0x40u);
                assert(EF_U16(rec + 0x8fu) == 0u);
                assert(EF_U16(rec + 0x95u) == 0u);
                assert(EF_U8(rec + 0x9bu) == 0u);
                assert(EF_U16(rec + 0x9cu) == 0x63u);
                assert(EF_U16(rec + 0x9eu) == 0xc7u);
                assert(EF_U8(rec + 0xa0u) == 1u);
                assert(EF_U16(rec + 0xa1u) == 0x5fu);
                assert(strcmp(ef_decomp_memory_span(rec + 0xa1u + 0x40u, 5u),
                              "NONE") == 0);
                assert(EF_U8(rec + 0xa1u + 0xa0u) == 0u);
            }
            ef_decomp_memory_release();

            /* A long ignored comment crosses the 0x100-byte refill, with
             * the first CR and LF on opposite sides of that boundary.
             * Whitespace-only lines still count toward the physical line
             * number. The next record selects a camera mode and processes
             * Name, Key and Location options across multiple lines. */
            {
                const char rest[] =
                    "\r\n \t\r\n  # alpha,beta\r\nMODE FIXED_REL_CAM\r\n"
                    "Name \"Overhead View\"\r\nKey V1\r\nLocation INSIDE\r\nEND\r\n";
                uint8_t lines[255u + sizeof(rest) - 1u];
                uint32_t stream;
                lines[0] = ';';
                memset(lines + 1u, 'x', 254u);
                memcpy(lines + 255u, rest, sizeof(rest) - 1u);
                file = fopen(views_path, "wb");
                assert(file && fwrite(lines, 1, sizeof(lines), file) == sizeof(lines));
                assert(fclose(file) == 0);
                assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
                setup_views_probe();
                stop = 0;
                assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe, &stop,
                                            reason, sizeof(reason)) == 0);
                assert(stop == 0xea50cu);
                stream = probe.base + probe.size;
                assert(EF_U8(stream) == 0u);
                assert(EF_U8(stream + 0x400u) == 0u);
                assert(EF_U32(stream + 0x552u) == 9u);
                assert(EF_U16(stream + 0x556u) == 0u);
                assert(EF_U32(stream + 0x558u) == 0u);
                assert(EF_U32(stream + 0x55cu) == 1u);
                assert(strcmp(ef_decomp_memory_span(0x0019cbb0u, 6u),
                              "alpha") == 0);
                assert(EF_U8(0x0019cbb0u + 0x3cu) == 1u);
                assert(strcmp(ef_decomp_memory_span(0x0019cbb0u + 0x1eu, 14u),
                              "Overhead View") == 0);
                assert(strcmp(ef_decomp_memory_span(0x0019cbb0u + 0x40u, 3u),
                              "V1") == 0);
                assert(EF_U8(0x0019cbb0u + 0x3du) == 0u);
                ef_decomp_memory_release();
            }

            /* All six original camera labels dispatch to their numbered
             * byte, including case folding. An unknown label retains the
             * existing field, as none of the six comparisons matches. */
            {
                const struct {
                    const char *label;
                    uint8_t expected;
                } modes[] = {
                    {"REL_CAM", 0u},
                    {"FIXED_REL_CAM", 1u},
                    {"DYNAMIC_CAM", 2u},
                    {"fixed_dynamic_cam", 3u},
                    {"TRACK_CAM", 4u},
                    {"CLOSURE_CAM", 5u},
                    {"unrecognized", 0x72u},
                };
                size_t i;
                for (i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
                    char config[96];
                    uint32_t stream;
                    int written = snprintf(config, sizeof(config),
                                           "# Test\nMODE %s\nEND\n", modes[i].label);
                    assert(written > 0 && written < (int)sizeof(config));
                    file = fopen(views_path, "wb");
                    assert(file && fwrite(config, 1, (size_t)written, file) ==
                           (size_t)written);
                    assert(fclose(file) == 0);
                    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
                    setup_views_probe();
                    EF_U8(0x0019cbb0u + 0x3cu) = 0x72u;
                    stop = 0;
                    assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe,
                                                &stop, reason, sizeof(reason)) == 0);
                    assert(stop == 0xea50cu);
                    stream = probe.base + probe.size;
                    assert(strcmp(ef_decomp_memory_span(0x0019cbb0u, 5u),
                                  "Test") == 0);
                    assert(EF_U8(stream + 0x400u) == 0u);
                    assert(EF_U8(0x0019cbb0u + 0x3cu) == modes[i].expected);
                    assert(EF_U32(stream + 0x552u) == 4u);
                    if (modes[i].expected == 2u) {
                        assert(EF_U16(0x0019cbb0u + 0x83u) == 0u);
                        assert(EF_U16(0x0019cbb0u + 0x85u) == 0u);
                        assert(EF_U16(0x0019cbb0u + 0x87u) == 0u);
                    }
                    ef_decomp_memory_release();
                }
            }

            /* Original 0x30407 uses an unbounded strcpy into the record.
             * The host probe stops before copying an oversized token. */
            {
                uint8_t long_name[38u];
                long_name[0] = '#';
                long_name[1] = ' ';
                memset(long_name + 2u, 'n', 35u);
                long_name[37u] = '\n';
                file = fopen(views_path, "wb");
                assert(file && fwrite(long_name, 1, sizeof(long_name), file) ==
                       sizeof(long_name));
                assert(fclose(file) == 0);
                assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
                setup_views_probe();
                stop = 0;
                assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe,
                                            &stop, reason, sizeof(reason)) == 0);
                assert(stop == 0x30407u);
                assert(EF_U16(0x0019cbb0u) == 0x5fu);
                assert(EF_U32(0x0019cbb0u + 0x1eu) == 0x76543210u);
                ef_decomp_memory_release();
            }

            /* A Location value other than INSIDE writes 1, regardless of
             * whether it names a known location. Matching is ASCII folded. */
            {
                const char outside[] = "# x\nMODE REL_CAM\nlocation OUTSIDE\nEND\n";
                file = fopen(views_path, "wb");
                assert(file && fwrite(outside, 1, sizeof(outside) - 1u, file) ==
                       sizeof(outside) - 1u);
                assert(fclose(file) == 0);
                assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
                setup_views_probe();
                stop = 0;
                assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe,
                                            &stop, reason, sizeof(reason)) == 0);
                assert(stop == 0xea50cu);
                assert(EF_U8(0x0019cbb0u + 0x3du) == 1u);
                assert(EF_U8(0x0019cbb0u + 0x3cu) == 0u);
                ef_decomp_memory_release();
            }

            /* The original first three scalar options round toward zero
             * after x87 arithmetic: 100 - damping and -32 * minimum
             * altitude. Negative and exponent inputs exercise signs and
             * the low-byte truncation of the two damping results. */
            {
                const struct {
                    const char *options;
                    uint8_t pos, ang;
                    int32_t altitude;
                    uint8_t roll;
                } cases[] = {
                    {"posdamping 12.5\nAngDamping 80.125\n"
                     "MinAlt 2.25\nRollLock horizon\n", 87u, 19u, -72, 3u},
                    {"PosDamping -1.25e1\nAngDamping 101.8\n"
                     "MinAlt -0.25\nRollLock unknown\n", 112u, 255u, 8, 3u},
                };
                size_t i;
                for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
                    char config[256];
                    int written = snprintf(config, sizeof(config),
                                           "# x\nMODE REL_CAM\n%sEND\n",
                                           cases[i].options);
                    assert(written > 0 && written < (int)sizeof(config));
                    file = fopen(views_path, "wb");
                    assert(file && fwrite(config, 1, (size_t)written, file) ==
                           (size_t)written);
                    assert(fclose(file) == 0);
                    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
                    setup_views_probe();
                    stop = 0;
                    assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe,
                                                &stop, reason, sizeof(reason)) == 0);
                    assert(stop == 0xea50cu);
                    assert(EF_U8(0x0019cbb0u + 0x3eu) == cases[i].pos);
                    assert(EF_U8(0x0019cbb0u + 0x3fu) == cases[i].ang);
                    assert(EF_S32(0x0019cbb0u + 0x4fu) == cases[i].altitude);
                    assert(EF_U8(0x0019cbb0u + 0x53u) == cases[i].roll);
                    ef_decomp_memory_release();
                }
            }

            /* RollLock has six independent, case-insensitive comparisons;
             * unrecognized values retain the default byte 3. */
            {
                const struct { const char *label; uint8_t expected; } rolls[] = {
                    {"NONE", 1u}, {"HORIZON", 3u}, {"BANKING_HORIZON", 2u},
                    {"DAMPED", 4u}, {"RPLANE", 5u}, {"inside", 0u},
                    {"invalid", 3u},
                };
                size_t i;
                for (i = 0; i < sizeof(rolls) / sizeof(rolls[0]); ++i) {
                    char config[96];
                    int written = snprintf(config, sizeof(config),
                                           "# x\nMODE REL_CAM\nRollLock %s\nEND\n",
                                           rolls[i].label);
                    assert(written > 0 && written < (int)sizeof(config));
                    file = fopen(views_path, "wb");
                    assert(file && fwrite(config, 1, (size_t)written, file) ==
                           (size_t)written);
                    assert(fclose(file) == 0);
                    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
                    setup_views_probe();
                    stop = 0;
                    assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe,
                                                &stop, reason, sizeof(reason)) == 0);
                    assert(stop == 0xea50cu);
                    assert(EF_U8(0x0019cbb0u + 0x53u) == rolls[i].expected);
                    ef_decomp_memory_release();
                }
            }

            /* The three vector forms use the same source float multiplier
             * (32.0f) and truncation, but write different record regions.
             * InitialOffSet also increments the +0x78 coordinate count. */
            {
                const char vectors[] =
                    "# x\nMODE REL_CAM\n"
                    "offset 1.25 -2.5 0\n"
                    "RandomOffset 3 +0.5 -1.125\n"
                    "InitialOffSet -1 2.5 3.125\n"
                    "END\n";
                file = fopen(views_path, "wb");
                assert(file && fwrite(vectors, 1, sizeof(vectors) - 1u, file) ==
                       sizeof(vectors) - 1u);
                assert(fclose(file) == 0);
                assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
                setup_views_probe();
                stop = 0;
                assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe,
                                            &stop, reason, sizeof(reason)) == 0);
                assert(stop == 0xea50cu);
                assert(EF_S32(0x0019cbb0u + 0x60u) == 40);
                assert(EF_S32(0x0019cbb0u + 0x64u) == -80);
                assert(EF_S32(0x0019cbb0u + 0x68u) == 0);
                assert(EF_S32(0x0019cbb0u + 0x54u) == 96);
                assert(EF_S32(0x0019cbb0u + 0x58u) == 16);
                assert(EF_S32(0x0019cbb0u + 0x5cu) == -36);
                assert(EF_S32(0x0019cbb0u + 0x6cu) == -32);
                assert(EF_S32(0x0019cbb0u + 0x70u) == 80);
                assert(EF_S32(0x0019cbb0u + 0x74u) == 100);
                assert(EF_U32(0x0019cbb0u + 0x78u) == 3u);
                ef_decomp_memory_release();
            }

            /* LookAtObj is a two-state selector: only SOURCE writes 0,
             * including case-folded spelling; anything else writes 1.
             * LookAt and Dist scale 3 and 4 values by 32 respectively,
             * preserving only their low signed-word representation. */
            {
                const struct { const char *look_obj; uint8_t expected; } modes[] = {
                    {"source", 0u}, {"RESOURCE", 1u}, {"EXTERNAL", 1u},
                };
                size_t i;
                for (i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
                    char config[180];
                    int written = snprintf(config, sizeof(config),
                                           "# x\nMODE REL_CAM\nLookAtObj %s\n"
                                           "LookAt 1.25 -2.5 0.5\n"
                                           "Dist 0.25 1.5 10 2\nEND\n",
                                           modes[i].look_obj);
                    assert(written > 0 && written < (int)sizeof(config));
                    file = fopen(views_path, "wb");
                    assert(file && fwrite(config, 1, (size_t)written, file) ==
                           (size_t)written);
                    assert(fclose(file) == 0);
                    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
                    setup_views_probe();
                    stop = 0;
                    assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe,
                                                &stop, reason, sizeof(reason)) == 0);
                    assert(stop == 0xea50cu);
                    assert(EF_U8(0x0019cbb0u + 0x7cu) == modes[i].expected);
                    assert(EF_S16(0x0019cbb0u + 0x7du) == 40);
                    assert(EF_S16(0x0019cbb0u + 0x7fu) == -80);
                    assert(EF_S16(0x0019cbb0u + 0x81u) == 16);
                    assert(EF_S16(0x0019cbb0u + 0x87u) == 8);
                    assert(EF_S16(0x0019cbb0u + 0x89u) == 48);
                    assert(EF_S16(0x0019cbb0u + 0x8bu) == 320);
                    assert(EF_S16(0x0019cbb0u + 0x8du) == 64);
                    ef_decomp_memory_release();
                }
            }

            /* XClamp and YClamp each set an enable word and store two
             * coordinates scaled by the original 182.0f. HUDCenter and
             * HUDHeight store unscaled, truncated signed words. */
            {
                const char config[] =
                    "# x\nMODE REL_CAM\n"
                    "xclamp 1.5 -2.25\nYClamp 0.5 3.25\n"
                    "HUDCenter -17.9\nHUDHeight 240.9\nEND\n";
                file = fopen(views_path, "wb");
                assert(file && fwrite(config, 1, sizeof(config) - 1u, file) ==
                       sizeof(config) - 1u);
                assert(fclose(file) == 0);
                assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
                setup_views_probe();
                stop = 0;
                assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe,
                                            &stop, reason, sizeof(reason)) == 0);
                assert(stop == 0xea50cu);
                assert(EF_U16(0x0019cbb0u + 0x8fu) == 1u);
                assert(EF_S16(0x0019cbb0u + 0x91u) == 273);
                assert(EF_S16(0x0019cbb0u + 0x93u) == -409);
                assert(EF_U16(0x0019cbb0u + 0x95u) == 1u);
                assert(EF_S16(0x0019cbb0u + 0x97u) == 91);
                assert(EF_S16(0x0019cbb0u + 0x99u) == 591);
                assert(EF_S16(0x0019cbb0u + 0x9cu) == -17);
                assert(EF_S16(0x0019cbb0u + 0x9eu) == 240);
                ef_decomp_memory_release();
            }

            /* Insts recognizes six literal layouts, including pointer-
             * into-string labels at 0x18292c and 0x18293c, and leaves the
             * default field alone on an unrecognized value. Cockpit sets
             * its byte for ON (case-insensitively) and clears it otherwise. */
            {
                const struct {
                    const char *layout;
                    uint8_t expected_layout;
                    const char *cockpit;
                    uint8_t expected_cockpit;
                } cases[] = {
                    {"FrontInternalCock", 5u, "ON", 1u},
                    {"BottomInfoBar", 2u, "off", 0u},
                    {"FullViewHUD", 3u, "oN", 1u},
                    {"DisplayFullTarget", 4u, "OFF", 0u},
                    {"InternalFullHUD", 1u, "ON", 1u},
                    {"None", 0u, "bad", 0u},
                    {"unknown", 0u, "ON", 1u},
                    {"frontinternalcock", 5u, "OFF", 0u},
                };
                size_t i;
                for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
                    char config[160];
                    int written = snprintf(config, sizeof(config),
                                           "# x\nMODE REL_CAM\nInsts %s\nCockpit %s\nEND\n",
                                           cases[i].layout, cases[i].cockpit);
                    assert(written > 0 && written < (int)sizeof(config));
                    file = fopen(views_path, "wb");
                    assert(file && fwrite(config, 1, (size_t)written, file) ==
                           (size_t)written);
                    assert(fclose(file) == 0);
                    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
                    setup_views_probe();
                    stop = 0;
                    assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe,
                                                &stop, reason, sizeof(reason)) == 0);
                    assert(stop == 0xea50cu);
                    assert(EF_U8(0x0019cbb0u + 0x9bu) == cases[i].expected_layout);
                    assert(EF_U8(0x0019cbb0u + 0xa0u) == cases[i].expected_cockpit);
                    ef_decomp_memory_release();
                }
                /* An unknown layout after a recognized one is a no-op. */
                {
                    const char config[] =
                        "# x\nMODE REL_CAM\nInsts FrontInternalCock\n"
                        "Insts bogus\nCockpit ON\nCockpit OFF\nEND\n";
                    file = fopen(views_path, "wb");
                    assert(file && fwrite(config, 1, sizeof(config) - 1u, file) ==
                           sizeof(config) - 1u);
                    assert(fclose(file) == 0);
                    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
                    setup_views_probe();
                    stop = 0;
                    assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe,
                                                &stop, reason, sizeof(reason)) == 0);
                    assert(stop == 0xea50cu);
                    assert(EF_U8(0x0019cbb0u + 0x9bu) == 5u);
                    assert(EF_U8(0x0019cbb0u + 0xa0u) == 0u);
                    ef_decomp_memory_release();
                }
            }

            /* The END branch increments the record index, then scans past
             * arbitrary non-marker text into a second sequence record.
             * Both fields and the original slot stride must stay intact. */
            {
                const char config[] =
                    "; leading comment\nignored outside record\n"
                    "# first\nMODE REL_CAM\nInitialOffSet 1 2 3\nEND\n"
                    "; gap\nnot a record\n"
                    "# second\nMODE FIXED_REL_CAM\n"
                    "Insts BottomInfoBar\nCockpit OFF\nEND\n";
                const ef_addr32 first = 0x0019cbb0u;
                const ef_addr32 second = first + 0xa1u;
                const ef_addr32 frame = EF_DECOMP_RAM_SIZE - 0x2000u + 0x600u;
                file = fopen(views_path, "wb");
                assert(file && fwrite(config, 1, sizeof(config) - 1u, file) ==
                       sizeof(config) - 1u);
                assert(fclose(file) == 0);
                assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
                setup_views_probe();
                stop = 0;
                assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe,
                                            &stop, reason, sizeof(reason)) == 0);
                assert(stop == 0xea50cu);
                assert(EF_U16(frame + 0x90u) == 2u);
                assert(strcmp(ef_decomp_memory_span(first, 6u), "first") == 0);
                assert(EF_U32(first + 0x78u) == 3u);
                assert(EF_S32(first + 0x6cu) == 32);
                assert(EF_S32(first + 0x70u) == 64);
                assert(EF_S32(first + 0x74u) == 96);
                assert(strcmp(ef_decomp_memory_span(second, 7u), "second") == 0);
                assert(EF_U8(second + 0x3cu) == 1u);
                assert(EF_U8(second + 0x9bu) == 2u);
                assert(EF_U8(second + 0xa0u) == 0u);
                assert(EF_U32(second + 0x78u) == 0u);
                ef_decomp_memory_release();
            }

            /* END rejects counts other than 0 or 3 at the original error
             * callback. Unknown options remain an explicit boundary. */
            {
                const struct { const char *config; uint32_t boundary; } cases[] = {
                    {"# x\nMODE REL_CAM\nInitialOffSet 1 2 3\n"
                     "InitialOffSet 4 5 6\nEND\n", 0x31116u},
                    {"# x\nMODE REL_CAM\nUnsupportedOption 3\n", 0x310d8u},
                };
                size_t i;
                for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
                    size_t n = strlen(cases[i].config);
                    file = fopen(views_path, "wb");
                    assert(file && fwrite(cases[i].config, 1, n, file) == n);
                    assert(fclose(file) == 0);
                    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
                    setup_views_probe();
                    stop = 0;
                    assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe,
                                                &stop, reason, sizeof(reason)) == 0);
                    assert(stop == cases[i].boundary);
                    assert(EF_U16(EF_DECOMP_RAM_SIZE - 0x2000u + 0x600u + 0x90u) == 0u);
                    if (i == 0u)
                        assert(EF_U32(0x0019cbb0u + 0x78u) == 6u);
                    ef_decomp_memory_release();
                }
            }

            /* The 0x31147..0x311f9 mode-two branch promotes signed LookAt
             * words to dwords and compares them with the dword OffSet. The
             * angles use the original quadrant helper and distance uses the
             * original low-word integer square root. The non-axis case
             * supplies a synthetic 45-degree table entry (quotient 512). */
            {
                const struct {
                    const char *options;
                    uint16_t pitch, heading, distance;
                } cases[] = {
                    {"LookAt 0 0 0\n", 0u, 0u, 0u},
                    {"LookAt 0 1 0\n", 0x4000u, 0u, 32u},
                    /* 45 degrees exercises the original 0x137a04
                     * angle-ratio table at quotient 512. */
                    {"LookAt 1 0 1\n", 0u, 0x2000u, 45u},
                    {"LookAt 0 0 -2\n", 0u, 0x8000u, 64u},
                    {"OffSet 1 0 0\nLookAt 0 0 0\n", 0u, 0xc000u, 32u},
                    {"OffSet -2000 0 0\nLookAt 0 0 0\n", 0u, 0x4000u, 64000u},
                    {"LookAt 2048 0 0\n", 0u, 0u, 0u},
                };
                const ef_addr32 rec = 0x0019cbb0u;
                size_t i;
                for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
                    char config[160];
                    int written = snprintf(config, sizeof(config),
                                           "# geometry\nMODE DYNAMIC_CAM\n%sEND\n",
                                           cases[i].options);
                    assert(written > 0 && written < (int)sizeof(config));
                    file = fopen(views_path, "wb");
                    assert(file && fwrite(config, 1, (size_t)written, file) ==
                           (size_t)written);
                    assert(fclose(file) == 0);
                    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
                    setup_views_probe();
                    EF_U16(0x00137a04u + 512u * 2u) = 0x2000u;
                    stop = 0;
                    assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe,
                                                &stop, reason, sizeof(reason)) == 0);
                    assert(stop == 0xea50cu);
                    assert(EF_U16(rec + 0x83u) == cases[i].pitch);
                    assert(EF_U16(rec + 0x85u) == cases[i].heading);
                    assert(EF_U16(rec + 0x87u) == cases[i].distance);
                    assert(EF_U16(EF_DECOMP_RAM_SIZE - 0x2000u + 0x600u + 0x90u) == 1u);
                    ef_decomp_memory_release();
                }
            }

            /* The bounded GCC decimal adapter does not claim support for
             * the original's malformed-token and exceptional-number paths. */
            {
                const char *invalid[] = {"oops", "1e", "nan", "0xA"};
                size_t i;
                for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
                    char config[96];
                    int written = snprintf(config, sizeof(config),
                                           "# x\nMODE REL_CAM\nPosDamping %s\n",
                                           invalid[i]);
                    assert(written > 0 && written < (int)sizeof(config));
                    file = fopen(views_path, "wb");
                    assert(file && fwrite(config, 1, (size_t)written, file) ==
                           (size_t)written);
                    assert(fclose(file) == 0);
                    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
                    setup_views_probe();
                    stop = 0;
                    assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe,
                                                &stop, reason, sizeof(reason)) == 0);
                    assert(stop == 0x9d55du);
                    assert(EF_U8(0x0019cbb0u + 0x3eu) == 0x64u);
                    ef_decomp_memory_release();
                }
            }

            /* The original copies Name and Key without bounds checks.
             * Stop at each original call site before a token can trample
             * the following record fields. */
            {
                const struct {
                    const char *key;
                    uint32_t boundary;
                } oversize[] = {
                    {"Name", 0x3053du},
                    {"Key", 0x30577u},
                };
                size_t i;
                for (i = 0; i < sizeof(oversize) / sizeof(oversize[0]); ++i) {
                    char long_value[36u], config[128u];
                    int written;
                    memset(long_value, 'n', sizeof(long_value) - 1u);
                    long_value[sizeof(long_value) - 1u] = 0;
                    written = snprintf(config, sizeof(config),
                                       "# x\nMODE REL_CAM\n%s %s\n",
                                       oversize[i].key, long_value);
                    assert(written > 0 && written < (int)sizeof(config));
                    file = fopen(views_path, "wb");
                    assert(file && fwrite(config, 1, (size_t)written, file) ==
                           (size_t)written);
                    assert(fclose(file) == 0);
                    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
                    setup_views_probe();
                    stop = 0;
                    assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe,
                                                &stop, reason, sizeof(reason)) == 0);
                    assert(stop == oversize[i].boundary);
                    assert(EF_U32(0x0019cbb0u + 0x1eu) == 0x76543210u);
                    assert(strcmp(ef_decomp_memory_span(0x0019cbb0u + 0x40u, 5u),
                                  "NONE") == 0);
                    ef_decomp_memory_release();
                }
            }

            /* A non-marker line takes the original 0x31201 skip branch,
             * then completes stream release and file close without initializing a
             * record or changing any of the 40 default slots. */
            {
                const char other[] = "  ordinary\r\n";
                uint32_t stream;
                file = fopen(views_path, "wb");
                assert(file && fwrite(other, 1, sizeof(other) - 1u, file) ==
                       sizeof(other) - 1u);
                assert(fclose(file) == 0);
                assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
                setup_views_probe();
                stop = 0;
                assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe, &stop,
                                            reason, sizeof(reason)) == 0);
                assert(stop == 0xea50cu);
                stream = probe.base + probe.size;
                assert(EF_U8(stream) == 0u);
                assert(EF_U8(stream + 0x400u) == 0u);
                assert(EF_U32(stream + 0x552u) == 2u);
                assert(EF_U32(0x0019cbb0u + 0x1eu) == 0u);
                ef_decomp_memory_release();
            }

            /* The original rejects a line over 0x400 bytes through its
             * configured callback before touching the token buffer. */
            {
                uint8_t overlong[0x402u];
                memset(overlong, 'a', sizeof(overlong));
                file = fopen(views_path, "wb");
                assert(file && fwrite(overlong, 1, sizeof(overlong), file) ==
                       sizeof(overlong));
                assert(fclose(file) == 0);
                assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
                setup_views_probe();
                stop = 0;
                assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe, &stop,
                                            reason, sizeof(reason)) == 0);
                assert(stop == 0xea50cu);
                assert(EF_U8(probe.base + probe.size + 0x400u) == 0u);
                ef_decomp_memory_release();
            }

            /* ReadGenSeqs returns through apply_options_82cec into the
             * second Views reader at 0x2ff44. Its plain seq2.dat record
             * names a camera already declared by views2.dat, then stores
             * the camera index, scaled duration and distance in the
             * twenty-entry 0x9c-byte sequence-list table. */
            {
                char seq_dir[180], seq_path[220];
                const char camera[] = "# x\nMODE REL_CAM\nEND\n";
                const char list[] =
                    "# listA\n"
                    "Name alpha\n"
                    "Name beta\n"
                    "ON\n"
                    "x 1.5 2.25\n"
                    "END\n"
                    "# listB\n"
                    "Name gamma\n"
                    "Name delta\n"
                    "OFF\n"
                    "END\n";
                const ef_addr32 rec = 0x0019bf80u;
                /* Unlike BG/views/views2.dat, the original seq2.dat
                 * suffix begins at the game root: \\views\\seq2.dat. */
                assert(snprintf(seq_dir, sizeof(seq_dir), "%s/views", game_dir) <
                       (int)sizeof(seq_dir));
                assert(mkdir(seq_dir, 0700) == 0);
                assert(snprintf(seq_path, sizeof(seq_path), "%s/seq2.dat", seq_dir) <
                       (int)sizeof(seq_path));
                file = fopen(views_path, "wb");
                assert(file && fwrite(camera, 1, sizeof(camera) - 1u, file) ==
                       sizeof(camera) - 1u);
                assert(fclose(file) == 0);
                file = fopen(seq_path, "wb");
                assert(file && fwrite(list, 1, sizeof(list) - 1u, file) ==
                       sizeof(list) - 1u);
                assert(fclose(file) == 0);
                assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
                setup_views_probe();
                EF_U16(0x001f71cau) = 0x34a5u;
                stop = 0;
                assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe,
                                            &stop, reason, sizeof(reason)) == 0);
                /* The next original function is ReadAidData() in Aid.c.
                 * Its defaults precede opening views/aid2.dat, which is
                 * missing from this synthetic setup. */
                assert(stop == 0xea50cu);
                assert(EF_U8(0x00196fcau) == 0u);
                assert(EF_U8(0x00196fcau + 26u) == 0u);
                assert(EF_U8(0x00196390u + 7u) == 0u);
                assert(strcmp(ef_decomp_memory_span(0x00196398u, 5u), "NONE") == 0);
                assert(strcmp(ef_decomp_memory_span(0x00196398u + 49u * 0x3eu, 5u),
                              "NONE") == 0);
                assert(EF_U16(0x00196398u + 0x34u) == 0u);
                assert(EF_U32(0x00196398u + 49u * 0x3eu + 0x3au) == 0u);
                assert(EF_U8(0x001961f8u) == 0u);
                assert(EF_U8(0x001961f9u) == 0xffu);
                assert(EF_U8(0x001961f9u + 7u * 0x33u + 49u) == 0xffu);
                assert(EF_U8(0x00196190u) == 0xffu);
                assert(EF_U8(0x00196191u) == 0u);
                assert(EF_U8(0x00196192u + 0x34u + 49u) == 0xffu);
                assert(EF_U32(0x001371a4u) == 0u);
                assert(strcmp(ef_decomp_memory_span(rec, 6u), "listA") == 0);
                assert(strcmp(ef_decomp_memory_span(rec + 0x1eu, 6u), "alpha") == 0);
                assert(strcmp(ef_decomp_memory_span(rec + 0x3cu, 5u), "beta") == 0);
                assert(EF_U8(rec + 0x4bu) == 1u);
                assert(EF_U16(rec + 0x4cu) == 0u);
                assert(EF_U16(rec + 0x4eu) == UINT16_MAX);
                assert(EF_U16(rec + 0x60u) == 75u);
                assert(EF_U32(rec + 0x74u) == 72u);
                assert(strcmp(ef_decomp_memory_span(rec + 0x9cu, 6u), "listB") == 0);
                assert(strcmp(ef_decomp_memory_span(rec + 0x9cu + 0x1eu, 6u),
                              "gamma") == 0);
                assert(EF_U8(rec + 0x9cu + 0x4bu) == 0u);
                assert(EF_U16(rec + 0x9cu + 0x4cu) == UINT16_MAX);
                assert(EF_U16(rec + 2u * 0x9cu) == 0x5fu);
                assert(strcmp(ef_decomp_memory_span(rec + 2u * 0x9cu + 0x3cu, 5u),
                              "NONE") == 0);
                assert(EF_U8(0x0019e5e0u) == 1u);
                assert(EF_U8(0x0012db9eu) == 0x49u);
                assert(EF_U16(0x0019e543u) == UINT16_MAX);
                assert(EF_U8(0x0019e555u) == 1u);
                assert(EF_U8(0x0019e5e3u) == 0u);
                assert(EF_U8(0x0019e5dfu) == 0u);
                assert(EF_U8(0x001a53dcu) == 0u);
                assert(EF_U16(0x0019e5e5u) == 0x34a5u);
                ef_decomp_memory_release();

                /* Aid.c reads 27+8 mappings, seven ordinary option grids,
                 * and two special option grids before stream cleanup. */
                {
                    char aid_path[220];
                    unsigned int j;
                    assert(snprintf(aid_path, sizeof(aid_path), "%s/aid2.dat",
                                    seq_dir) < (int)sizeof(aid_path));
                    file = fopen(aid_path, "wb");
                    assert(file);
                    assert(fputs("AID_HEADER\n", file) >= 0);
                    for (j = 0; j < 27u; ++j)
                        assert(fprintf(file, "row%u %u\n", j, j + 130u) > 0);
                    assert(fputs("AID_SECOND_HEADER\n", file) >= 0);
                    for (j = 0; j < 8u; ++j)
                        assert(fprintf(file, "row%u %u\n", j, j + 240u) > 0);
                    assert(fputs("FIRST_ASSOC_HEADER\nSECOND_ASSOC_HEADER\n"
                                 "Group 1\nOption 1\n"
                                 "Relation parent CHILD\n"
                                 "FixedPoint -1.25 ~\n"
                                 "Weight 2.5\n"
                                 "Name ALPHA\n"
                                 "Seqs\n"
                                 "Relation THREAT GTARGET\n"
                                 "FixedPoint 2 3\n"
                                 "Weight 1\n"
                                 "Name BETA\n"
                                 "Option\nOption 2\n"
                                 "Relation MTHREAT WINGMAN\n"
                                 "FixedPoint 4 5\n"
                                 "Weight 2\n"
                                 "Name CHARLIE\nEND\n", file) >= 0);
                    for (j = 2u; j < 8u; ++j) {
                        assert(fprintf(file,
                                       "Group %u\nOption 1\n"
                                       "Relation TARGET NONE\n"
                                       "FixedPoint %u ~\n"
                                       "Weight 1\nName GROUP%u\nEND\n",
                                       j, j, j) > 0);
                    }
                    assert(fputs("Special CRUISING\n"
                                 "SpecialGroup 1\nOption 1\n"
                                 "Relation PARENT NONE\n"
                                 "FixedPoint 0.5 1\n"
                                 "Weight 2\n"
                                 "Name SPECIAL_A\n"
                                 "END DOGFIGHTING\n"
                                 "SpecialGroup 2\nOption 1\n"
                                 "Relation CHILD TARGET\n"
                                 "FixedPoint -1.5 ~\n"
                                 "Weight 3\n"
                                 "Name SPECIAL_B\nEND\n", file) >= 0);
                    assert(fclose(file) == 0);
                    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
                    setup_views_probe();
                    EF_U8(0x0041f72cu) = 0x99u;
                    EF_U16(0x000fe624u) = 0xbeefu;
                    stop = 0;
                    assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe,
                                                &stop, reason, sizeof(reason)) == 0);
                    if (stop != 0x628u)
                        fprintf(stderr, "full Aid.c stop=0x%08x: %s\n", stop, reason);
                    assert(stop == 0x628u);
                    /* 0xcd4d4 has reset the key tables, programmed the
                     * virtual keyboard LEDs, and saved the selector before
                     * trying the first missing CRT-initialized request. */
                    assert(EF_U8(0x0015b72eu) == 0u);
                    assert(EF_U8(0x0041f72cu) == 0x1cu);
                    assert(EF_U8(0x0041f72du) == 0x1du);
                    assert(EF_U8(0x0041f748u) == 0x38u);
                    assert(EF_U16(0x000fe624u) == 0u);
                    assert(EF_U32(0x00196144u) == 0u);
                    assert(EF_U32(0x0041f828u) == 0u);
                    assert(EF_U8(0x00196fcau) == 130u);
                    assert(EF_U8(0x00196fcau + 26u) == 156u);
                    assert(EF_U8(0x00196390u) == 240u);
                    assert(EF_U8(0x00196390u + 7u) == 247u);
                    assert(strcmp(ef_decomp_memory_span(0x00196398u, 6u), "ALPHA") == 0);
                    assert(EF_U8(0x00196398u + 0x32u) == 1u);
                    assert(EF_U8(0x00196398u + 0x33u) == 2u);
                    assert(EF_U16(0x00196398u + 0x34u) == 125u);
                    assert(EF_S32(0x00196398u + 0x36u) == -40);
                    assert(EF_U32(0x00196398u + 0x3au) == 0u);
                    assert(EF_U8(0x001961f8u + 0x33u + 1u) == 0u);
                    assert(EF_U8(0x001961f8u + 0x33u + 2u) == 1u);
                    assert(EF_U8(0x001961f8u + 0x33u + 11u) == 2u);
                    assert(EF_U8(0x001961f8u + 0x33u) == 2u);
                    assert(EF_U8(0x001961f8u + 2u * 0x33u) == 1u);
                    assert(EF_U8(0x001961f8u + 2u * 0x33u + 1u) == 3u);
                    assert(EF_U8(0x001961f8u + 7u * 0x33u + 1u) == 8u);
                    assert(EF_U8(0x001961f8u + 7u * 0x33u + 2u) == 0xffu);
                    assert(strcmp(ef_decomp_memory_span(0x00196398u + 0x3eu, 5u),
                                  "BETA") == 0);
                    assert(EF_U8(0x00196398u + 0x3eu + 0x32u) == 6u);
                    assert(EF_U8(0x00196398u + 0x3eu + 0x33u) == 4u);
                    assert(EF_U8(0x00196398u + 2u * 0x3eu + 0x32u) == 7u);
                    assert(EF_U8(0x00196398u + 2u * 0x3eu + 0x33u) == 5u);
                    assert(EF_U8(0x00196190u) == 0u); /* CRUISING */
                    assert(EF_U8(0x00196191u) == 1u);
                    assert(EF_U8(0x00196192u) == 9u);
                    assert(EF_U8(0x00196190u + 0x34u) == 1u); /* DOGFIGHTING */
                    assert(EF_U8(0x00196191u + 0x34u) == 1u);
                    assert(EF_U8(0x00196192u + 0x34u) == 10u);
                    assert(strcmp(ef_decomp_memory_span(0x00196398u + 9u * 0x3eu,
                                                        10u), "SPECIAL_A") == 0);
                    assert(EF_S32(0x00196398u + 10u * 0x3eu + 0x36u) == -48);
                    assert(EF_U8(0x00196fe9u) == 5u); /* 0x1ad8 reset */
                    assert(EF_U32(0x001371a4u) == 0u); /* stream and file closed */
                    ef_decomp_memory_release();

                    /* On the already-initialized path, 0xcd4d4 returns
                     * without touching the keyboard/requests. The caller
                     * now executes 0xce08c and the zero-flag dispatch,
                     * stopping before the timer service at 0xe28e8. */
                    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
                    setup_views_probe();
                    EF_U8(0x0015b72eu) = 1u;
                    EF_U8(0x0041f72cu) = 0x99u;
                    EF_U16(0x000fe624u) = 0xbeefu;
                    stop = 0;
                    assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe,
                                                &stop, reason, sizeof(reason)) == 0);
                    assert(stop == 0xe28e8u);
                    assert(EF_U8(0x0041f72cu) == 0x99u);
                    assert(EF_U16(0x000fe624u) == 0xbeefu);
                    assert(EF_U32(0x00196144u) == 0u);
                    ef_decomp_memory_release();

                    /* When the timer was already started, its recovered
                     * guard returns without hardware calls. Mode 0 (and
                     * invalid modes) select no tables, execute the
                     * recovered disconnected-gameport model and return
                     * from TACTCOM at 0x8b52d. Mode 1..3 stop before
                     * their distinct hardware probes in 0xe9ac0. */
                    {
                        static const struct {
                            uint16_t mode;
                            uint32_t boundary;
                            uint16_t mode_after;
                        } cases[] = {
                            {0u, 0x8b52du, 0u},
                            {1u, 0xd9b30u, 1u},
                            {2u, 0xd9c30u, 2u},
                            {3u, 0x10d9e0u, 3u},
                            {4u, 0x8b52du, 0u},
                        };
                        size_t ci;
                        for (ci = 0; ci < sizeof(cases) / sizeof(cases[0]); ++ci) {
                            assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
                            setup_views_probe();
                            EF_U8(0x0015b72eu) = 1u;
                            EF_U8(0x0015f751u) = 1u;
                            EF_U16(0x00464470u) = 0x5a5au;
                            EF_U16(0x00464472u) = cases[ci].mode;
                            stop = 0;
                            assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe,
                                                        &stop, reason, sizeof(reason)) == 0);
                            if (stop != cases[ci].boundary)
                                fprintf(stderr, "TACTCOM mode %u stop=0x%08x: %s\n",
                                        (unsigned)cases[ci].mode, stop, reason);
                            assert(stop == cases[ci].boundary);
                            assert(EF_U16(0x00464472u) == cases[ci].mode_after);
                            assert(EF_U16(0x00464470u) == 0x5a5au);
                            assert(EF_U8(0x0015f751u) == 1u);
                            if (cases[ci].boundary == 0x8b52du) {
                                assert(EF_U32(0x0019840cu) == 0u);
                                assert(EF_U32(0x00198410u) == 0u);
                                assert(EF_U8(0x0015ba2cu) == 0u);
                                assert(EF_U8(0x0015ba2fu) == 0u);
                            }
                            ef_decomp_memory_release();
                        }
                    }

                    /* A source-enabled page branch must stop at its own
                     * handler before reaching 0xe28e8; no side effects are
                     * silently bypassed to reach the timer boundary. */
                    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
                    setup_views_probe();
                    EF_U8(0x0015b72eu) = 1u;
                    EF_U8(0x0015bac6u) = 1u;
                    stop = 0;
                    assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe,
                                                &stop, reason, sizeof(reason)) == 0);
                    assert(stop == 0x7feb0u);
                    assert(EF_U8(0x0015bac6u) == 1u);
                    ef_decomp_memory_release();

                    /* Alternate enum ordering, NONE second relationship,
                     * signed fixed-point values, and the non-tilde path. */
                    file = fopen(aid_path, "wb");
                    assert(file && fputs("AID_HEADER\n", file) >= 0);
                    for (j = 0; j < 27u; ++j)
                        assert(fprintf(file, "row%u %u\n", j, j + 130u) > 0);
                    assert(fputs("AID_SECOND_HEADER\n", file) >= 0);
                    for (j = 0; j < 8u; ++j)
                        assert(fprintf(file, "row%u %u\n", j, j + 240u) > 0);
                    assert(fputs("FIRST_ASSOC_HEADER\nSECOND_ASSOC_HEADER\n"
                                 "Group 1\nOption 1\n"
                                 "Relation WINGMAN NONE\n"
                                 "FixedPoint 1.5 -0.75\n"
                                 "Weight -1.25\n"
                                 "Name BRAVO\n", file) >= 0);
                    assert(fclose(file) == 0);
                    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
                    setup_views_probe();
                    stop = 0;
                    assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe,
                                                &stop, reason, sizeof(reason)) == 0);
                    assert(stop == 0x15cbu);
                    assert(strcmp(ef_decomp_memory_span(0x00196398u, 6u), "BRAVO") == 0);
                    assert(EF_U8(0x00196398u + 0x32u) == 5u);
                    assert(EF_U8(0x00196398u + 0x33u) == 0u);
                    assert(EF_S16(0x00196398u + 0x34u) == -62);
                    assert(EF_S32(0x00196398u + 0x36u) == 48);
                    assert(EF_S32(0x00196398u + 0x3au) == -24);
                    ef_decomp_memory_release();

                    /* A special group must resolve against the original
                     * 27-name table, not a made-up index. The first seven
                     * ordinary groups are required to reach that lookup. */
                    file = fopen(aid_path, "wb");
                    assert(file && fputs("AID_HEADER\n", file) >= 0);
                    for (j = 0; j < 27u; ++j)
                        assert(fprintf(file, "row%u %u\n", j, j + 130u) > 0);
                    assert(fputs("AID_SECOND_HEADER\n", file) >= 0);
                    for (j = 0; j < 8u; ++j)
                        assert(fprintf(file, "row%u %u\n", j, j + 240u) > 0);
                    assert(fputs("FIRST_ASSOC_HEADER\nSECOND_ASSOC_HEADER\n",
                                 file) >= 0);
                    for (j = 1u; j < 8u; ++j)
                        assert(fprintf(file, "Group %u\nOption 1\n"
                                       "Relation PARENT NONE\n"
                                       "FixedPoint 1 ~\nWeight 1\n"
                                       "Name OBJ%u\nEND\n", j, j) > 0);
                    assert(fputs("Special NOT_A_KNOWN_STATE\n", file) >= 0);
                    assert(fclose(file) == 0);
                    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
                    setup_views_probe();
                    stop = 0;
                    assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe,
                                                &stop, reason, sizeof(reason)) == 0);
                    assert(stop == 0x1687u);
                    assert(EF_U8(0x00196190u) == 0xffu);
                    assert(EF_U8(0x001961f8u + 7u * 0x33u + 1u) == 6u);
                    ef_decomp_memory_release();

                    /* Truncated mapping files must not let the host probe
                     * read past EOF into unrelated startup-table writes. */
                    file = fopen(aid_path, "wb");
                    assert(file && fputs("AID_HEADER\n", file) >= 0);
                    assert(fclose(file) == 0);
                    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
                    setup_views_probe();
                    stop = 0;
                    assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe,
                                                &stop, reason, sizeof(reason)) == 0);
                    assert(stop == 0x1171u);
                    assert(EF_U8(0x00196fcau + 26u) == 0u);
                    ef_decomp_memory_release();
                    assert(remove(aid_path) == 0);
                }

                /* Unknown camera names invoke the original fatal callback
                 * before a numeric value or camera index is stored. */
                {
                    const char unknown[] =
                        "# listB\nName alpha\nName beta\nOFF\n"
                        "does_not_exist 1 2\nEND\n";
                    file = fopen(seq_path, "wb");
                    assert(file && fwrite(unknown, 1, sizeof(unknown) - 1u, file) ==
                           sizeof(unknown) - 1u);
                    assert(fclose(file) == 0);
                    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
                    setup_views_probe();
                    stop = 0;
                    assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe,
                                                &stop, reason, sizeof(reason)) == 0);
                    assert(stop == 0x300e6u);
                    assert(EF_U8(rec + 0x4bu) == 0u);
                    ef_decomp_memory_release();
                }
                assert(remove(seq_path) == 0);
                assert(rmdir(seq_dir) == 0);
            }

            file = fopen(views_path, "wb");
            assert(file && fwrite(packed, 1, sizeof(packed), file) == sizeof(packed));
            assert(fclose(file) == 0);
            assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
            setup_views_probe();
            stop = 0;
            assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe, &stop,
                                        reason, sizeof(reason)) == 0);
            assert(stop == 0x9cd53u);
            assert(EF_U32(0x001371a4u) == 0u);
            ef_decomp_memory_release();

            file = fopen(views_path, "wb");
            assert(file && fwrite(packed_ra, 1, sizeof(packed_ra), file) ==
                   sizeof(packed_ra));
            assert(fclose(file) == 0);
            assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
            setup_views_probe();
            stop = 0;
            assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe, &stop,
                                        reason, sizeof(reason)) == 0);
            assert(stop == 0x9cd53u);
            ef_decomp_memory_release();

            file = fopen(views_path, "wb");
            assert(file && fwrite("XYZ", 1, 3u, file) == 3u);
            assert(fclose(file) == 0);
            assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
            setup_views_probe();
            stop = 0;
            assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe, &stop,
                                        reason, sizeof(reason)) == 0);
            assert(stop == 0x9ccb8u);
            ef_decomp_memory_release();

            assert(remove(views_path) == 0);
            assert(rmdir(views_dir) == 0);
            assert(rmdir(bg_dir) == 0);
        }
        assert(remove(filename) == 0);
        {
            char path[160];
            assert(snprintf(path, sizeof(path), "%s/EF2000.CFG", dirname) <
                   (int)sizeof(path));
            assert(remove(path) == 0);
            assert(snprintf(path, sizeof(path), "%s/german/EF2000.CFG", dirname) <
                   (int)sizeof(path));
            assert(remove(path) == 0);
            assert(snprintf(path, sizeof(path), "%s/german", dirname) <
                   (int)sizeof(path));
            assert(rmdir(path) == 0);
        }

        assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
        EF_U32(0x001264d4u) = 0x400u;
        memcpy(ef_decomp_memory_span(0x0018a810u, 12), "Memory: %sK", 12);
        memcpy(ef_decomp_memory_span(0x0018a848u, 9), "File.Dat", 9);
        memcpy(ef_decomp_memory_span(0x00132884u, 13), "Missing data", 13);
        memcpy(ef_decomp_memory_span(0x0018b154u, 4), "DID", 4);
        memcpy(ef_decomp_memory_span(0x1900u, 14), "DID=C:\\GAMES\\", 14);
        memcpy(ef_decomp_memory_span(0x0018d52cu, 3), "rb", 3);
        EF_U32(0x00160a18u) = 0x1800u;
        EF_U32(0x00132ab4u) = 0x0009cb20u;
        EF_U32(0x1800u) = 0x1900u;
        {
            uint32_t stop = 0;
            assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe, &stop,
                                        reason, sizeof(reason)) == 0);
            assert(stop == 0xeb25au);
            assert(EF_U32(0x001371a4u) == 0);
            assert(EF_U8(0x00160638u) == 1);
            assert(EF_U8(0x0017d404u) == 1);
            assert(EF_U32(0x00160a24u) != 0);
            assert(strcmp(ef_decomp_memory_span(EF_U32(0x00160a24u), 16),
                          "Missing data") == 0);
        }
        ef_decomp_memory_release();
        assert(rmdir(game_dir) == 0);
        assert(rmdir(dirname) == 0);
    }

    /* Case-folded 'playvideo' takes the original short branch, with no
     * arena allocation and a separate unimplemented reset boundary. */
    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
    EF_U32(0x001264d4u) = 0x400u;
    memcpy(ef_decomp_memory_span(0x0018a810u, 12), "Memory: %sK", 12);
    memcpy(ef_decomp_memory_span(0x0018a848u, 9), "File.Dat", 9);
    EF_U32(0x00132ab4u) = 0x0009cb20u;
    memcpy(ef_decomp_memory_span(0x0018a7e8u, 10), "playvideo", 10);
    memcpy(ef_decomp_memory_span(0x3000u, 10), "PLAYVIDEO", 10);
    EF_U32(0x2000u + 4u) = 0x3000u;
    {
        uint32_t stop = 0;
        assert(ef_decomp_probe_main(bss_end, 2, 0x2000u, NULL, &probe, &stop,
                                    reason, sizeof(reason)) == 0);
        assert(stop == 0x8b2d8u && probe.size == 0);
        assert(EF_U32(0x0015f75cu) == 0);
        assert(ascii_stricmp_9d524(0x0018a7e8u, 0x3000u) == 0);
    }
    memcpy(ef_decomp_memory_span(0x3000u, 10), "notvideo", 9);
    assert(ascii_stricmp_9d524(0x0018a7e8u, 0x3000u) != 0);
    {
        uint32_t stop = 0;
        assert(ef_decomp_probe_main(bss_end, 2, 0x2000u, NULL, &probe, &stop,
                                    reason, sizeof(reason)) == 0);
        assert(stop == 0xb3b90u && probe.size == 0x400u);
    }
    ef_decomp_memory_release();

    /* Deliberately low synthetic X-32 free memory takes the original
     * diagnostic callback path, which is still a bounded native boundary. */
    assert(ef_decomp_memory_load(payload, sizeof(payload), 0x02500000u) == 0);
    EF_U32(0x001264d4u) = 0x400u;
    memcpy(ef_decomp_memory_span(0x0018a810u, 12), "Memory: %sK", 12);
    memcpy(ef_decomp_memory_span(0x0018a848u, 9), "File.Dat", 9);
    {
        uint32_t stop = 0;
        assert(ef_decomp_probe_main(0x02500000u, 1, 0, NULL, &probe, &stop,
                                    reason, sizeof(reason)) == 0);
        assert(stop == 0x8b417u);
        assert(EF_U32(0x00132a74u) == 0);
    }
    ef_decomp_memory_release();

    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
    assert(ef_decomp_probe_arena(bss_end, &probe, reason, sizeof(reason)) != 0);
    ef_decomp_memory_release();
    return 0;
}
