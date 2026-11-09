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
        memcpy(ef_decomp_memory_span(0x3000u, 9), "..\\escape", 9);
        assert(read_file_payload_size_b3b90(0x3000u) == 0u);
        memcpy(ef_decomp_memory_span(0x3000u, 8), "NOTHERE", 8);
        assert(read_file_payload_size_b3b90(0x3000u) == 0u);
        ef_decomp_files_end();
        ef_decomp_memory_release();

        /* Missing-file branch in the actual TACTCOM prefix reaches its
         * diagnostic callback; when present, the next call is 0x82cec. */
        assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
        EF_U32(0x001264d4u) = 0x400u;
        memcpy(ef_decomp_memory_span(0x0018a810u, 12), "Memory: %sK", 12);
        memcpy(ef_decomp_memory_span(0x0018a848u, 9), "File.Dat", 9);
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
            assert(stop == 0x82cecu);
            assert(EF_U32(0x001371a4u) == 0);
        }
        ef_decomp_memory_release();
        assert(remove(filename) == 0);

        assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
        EF_U32(0x001264d4u) = 0x400u;
        memcpy(ef_decomp_memory_span(0x0018a810u, 12), "Memory: %sK", 12);
        memcpy(ef_decomp_memory_span(0x0018a848u, 9), "File.Dat", 9);
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
            assert(stop == 0xea50cu);
            assert(EF_U32(0x001371a4u) == 0);
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
