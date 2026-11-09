/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

/* First real game-owned startup call with synthetic image and service data. */

#include "decomp_image.h"
#include "decomp_startup.h"
#include "ef_abi.h"
#include "ef_mem.h"

#include <assert.h>
#include <string.h>

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
    {
        uint32_t stop = 0;
        assert(ef_decomp_probe_main(bss_end, 1, 0, &probe, &stop,
                                    reason, sizeof(reason)) == 0);
        assert(stop == 0x8b39cu && probe.base == bss_end);
    }
    ef_decomp_memory_release();

    /* Case-folded 'playvideo' takes the original short branch, with no
     * arena allocation and a separate unimplemented reset boundary. */
    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
    EF_U32(0x001264d4u) = 0x400u;
    memcpy(ef_decomp_memory_span(0x0018a7e8u, 10), "playvideo", 10);
    memcpy(ef_decomp_memory_span(0x3000u, 10), "PLAYVIDEO", 10);
    EF_U32(0x2000u + 4u) = 0x3000u;
    {
        uint32_t stop = 0;
        assert(ef_decomp_probe_main(bss_end, 2, 0x2000u, &probe, &stop,
                                    reason, sizeof(reason)) == 0);
        assert(stop == 0x8b2d8u && probe.size == 0);
        assert(EF_U32(0x0015f75cu) == 0);
        assert(ascii_stricmp_9d524(0x0018a7e8u, 0x3000u) == 0);
    }
    memcpy(ef_decomp_memory_span(0x3000u, 10), "notvideo", 9);
    assert(ascii_stricmp_9d524(0x0018a7e8u, 0x3000u) != 0);
    {
        uint32_t stop = 0;
        assert(ef_decomp_probe_main(bss_end, 2, 0x2000u, &probe, &stop,
                                    reason, sizeof(reason)) == 0);
        assert(stop == 0x8b39cu && probe.size == 0x400u);
    }
    ef_decomp_memory_release();

    assert(ef_decomp_memory_load(payload, sizeof(payload), bss_end) == 0);
    assert(ef_decomp_probe_arena(bss_end, &probe, reason, sizeof(reason)) != 0);
    ef_decomp_memory_release();
    return 0;
}
