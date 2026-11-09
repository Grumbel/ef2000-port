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
            {"MUSIC", 0x001fcb50u, 0x50u, 0u, 5u},
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
        assert(EF_U32(0x00464384u) == text);
        assert(EF_U8(0x001fcb50u) == 0u);

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
            EF_U32(0x001264d4u) = 0x400u;
            memcpy(ef_decomp_memory_span(0x0018a810u, 12), "Memory: %sK", 12);
            memcpy(ef_decomp_memory_span(0x0018a848u, 9), "File.Dat", 9);
            memcpy(ef_decomp_memory_span(0x0018b154u, 4), "DID", 4);
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
            assert(ef_decomp_probe_main(bss_end, 1, 0, dirname, &probe, &stop,
                                        reason, sizeof(reason)) == 0);
            assert(stop == 0x30214u);
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
