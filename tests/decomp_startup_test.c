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
    EF_U32(0x001264d4u) = 0x400u;
    memcpy(ef_decomp_memory_span(0x0018a810u, 12), "Memory: %sK", 12);
    memcpy(ef_decomp_memory_span(0x0018a848u, 9), "File.Dat", 9);
    memcpy(ef_decomp_memory_span(0x0018b154u, 4), "DID", 4);
    memcpy(ef_decomp_memory_span(0x00182708u, 5), "NONE", 5);
    EF_U16(0x00182710u) = 0x5fu;
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
    ef_decomp_arena_probe probe;
    char reason[128];

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
         * second token of the next line as its camera mode, then stops
         * before decoding the remaining options. */
        {
            char bg_dir[160], views_dir[180], views_path[220];
            const uint8_t plain[] = "# x\nMODE REL_CAM\n";
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
            assert(stop == 0x304fcu);
            assert(EF_U32(0x001371a4u) == 1u);
            assert(EF_U32(EF_DECOMP_RAM_SIZE - 0x2000u + 0x600u + 0x84u) == 100u);
            {
                uint32_t stream = probe.base + probe.size;
                assert(strcmp(ef_decomp_memory_span(stream, 14u), "MODE REL_CAM") == 0);
                assert(strcmp(ef_decomp_memory_span(stream + 0x400u, 8u),
                              "REL_CAM") == 0);
                assert(EF_U32(stream + 0x552u) == 2u);
                assert(EF_U16(stream + 0x556u) == 12u);
                assert(EF_U32(stream + 0x558u) == sizeof(plain) - 1u);
                assert(EF_U32(stream + 0x55cu) == sizeof(plain) - 1u);
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
             * number. The next record line starts with a '#' marker and its
             * following line selects the camera mode. */
            {
                const char rest[] =
                    "\r\n \t\r\n  # alpha,beta\r\nMODE FIXED_REL_CAM\r\n";
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
                assert(stop == 0x304fcu);
                stream = probe.base + probe.size;
                assert(strcmp(ef_decomp_memory_span(stream, 20u),
                              "MODE FIXED_REL_CAM") == 0);
                assert(strcmp(ef_decomp_memory_span(stream + 0x400u, 14u),
                              "FIXED_REL_CAM") == 0);
                assert(EF_U32(stream + 0x552u) == 4u);
                assert(EF_U16(stream + 0x556u) == 18u);
                assert(EF_U32(stream + 0x558u) == sizeof(lines) - 0x100u);
                assert(EF_U32(stream + 0x55cu) == sizeof(lines) - 0x100u);
                assert(EF_U32(0x0019cbb0u + 0x1eu) == 0x76543210u);
                assert(strcmp(ef_decomp_memory_span(0x0019cbb0u, 6u),
                              "alpha") == 0);
                assert(EF_U8(0x0019cbb0u + 0x3cu) == 1u);
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
                                           "# Test\nMODE %s\n", modes[i].label);
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
                    assert(stop == 0x304fcu);
                    stream = probe.base + probe.size;
                    assert(strcmp(ef_decomp_memory_span(0x0019cbb0u, 5u),
                                  "Test") == 0);
                    assert(strcmp(ef_decomp_memory_span(stream + 0x400u, 0x51u),
                                  modes[i].label) == 0);
                    assert(EF_U8(0x0019cbb0u + 0x3cu) == modes[i].expected);
                    assert(EF_U32(stream + 0x552u) == 2u);
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

            /* A non-marker line takes the original 0x31201 skip branch.
             * It must not initialize the record-specific defaults. */
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
                assert(stop == 0x31201u);
                stream = probe.base + probe.size;
                assert(strcmp(ef_decomp_memory_span(stream, 10u), "ordinary") == 0);
                assert(strcmp(ef_decomp_memory_span(stream + 0x400u, 9u),
                              "ordinary") == 0);
                assert(EF_U32(stream + 0x552u) == 1u);
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
