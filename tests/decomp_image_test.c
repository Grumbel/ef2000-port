/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

/* Synthetic FlashTek envelope and bounded guest-memory tests, no game bytes. */

#include "decomp_image.h"
#include "ef_abi.h"
#include "ef_mem.h"

#include <assert.h>
#include <string.h>

static void write_u16(uint8_t *dst, unsigned value)
{
    dst[0] = (uint8_t)value;
    dst[1] = (uint8_t)(value >> 8);
}

static void write_u32(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)value;
    dst[1] = (uint8_t)(value >> 8);
    dst[2] = (uint8_t)(value >> 16);
    dst[3] = (uint8_t)(value >> 24);
}

static void test_layout(void)
{
    uint8_t executable[0x140] = { 0 };
    ef_decomp_image_info info;
    memcpy(executable, "MZ", 2);
    write_u16(executable + 8, 3);
    memcpy(executable + 0x28, "B23X", 4);
    write_u16(executable + 0x30, 0x60);
    write_u16(executable + 0x32, 0x40);
    write_u16(executable + 0x38, 2);  /* code offset 0x20 */
    write_u16(executable + 0x42, 1);  /* data offset 0x10 */
    write_u32(executable + 0x60 + 0x10 + 4, 0xa0);
    write_u32(executable + 0x60 + 0x10 + 8, 0x180);
    assert(ef_decomp_decode_layout(executable, sizeof(executable), &info) == 0);
    assert(info.payload_file_offset == 0xa0);
    assert(info.initialized_size == 0xa0 && info.bss_end == 0x180);
    write_u32(executable + 0x60 + 0x10 + 4, 0xa1);
    assert(ef_decomp_decode_layout(executable, sizeof(executable), &info) != 0);
    write_u32(executable + 0x60 + 0x10 + 4, 0xa0);
    executable[0x28] = 0;
    assert(ef_decomp_decode_layout(executable, sizeof(executable), &info) != 0);
}

static void test_ram(void)
{
    const uint8_t original[5] = { 1, 2, 3, 4, 5 };
    const uint8_t alternate[2] = { 7, 9 };
    assert(ef_decomp_memory_load(original, sizeof(original), 0x100) == 0);
    assert(EF_U8(0) == 1 && EF_U8(4) == 5);
    assert(EF_U8(5) == 0 && EF_U8(0xff) == 0);
    EF_U32(0x80) = 0x87654321u;
    assert(EF_U32(0x80) == 0x87654321u);
    assert(ef_decomp_memory_load(alternate, sizeof(alternate), 0x40) == 0);
    assert(EF_U8(0) == 7 && EF_U8(1) == 9);
    assert(EF_U32(0x80) == 0);
    assert(ef_decomp_memory_load(original, sizeof(original), 4) != 0);
    ef_decomp_memory_release();
}

static void test_recovered_guest_functions(void)
{
    static const uint8_t blank_image[1] = { 0 };
    const ef_addr32 stream = 0x3000u;
    const ef_addr32 source = 0x4000u;
    const ef_addr32 framebuffer = 0x200000u;
    unsigned row;

    assert(ef_decomp_memory_load(blank_image, sizeof(blank_image), 0x210000u) == 0);

    /* The real recovered stream reader must use guest offsets, not host
     * pointers. Its terminating zero also sets bit 1 in the stream flags. */
    EF_U32(stream + 8u) = source;
    EF_U8(source) = 'A';
    EF_U8(source + 1u) = 'B';
    assert(read_stream_byte_df21b(stream) == 'A');
    assert(read_stream_byte_df21b(stream) == 'B');
    assert(read_stream_byte_df21b(stream) == -1);
    assert(EF_U32(stream + 8u) == source + 2u);
    assert((EF_U8(stream + 0x10u) & 2u) != 0);

    /* Likewise exercise the actual recovered 8x6 renderer with a synthetic
     * glyph and framebuffer, without bundling proprietary font/image data. */
    EF_U32(0x0015b754u) = framebuffer;
    EF_U32(0x001631f4u) = 16u;
    EF_U32(0x00163200u + 4u) = 0u; /* y = 1 */
    for (row = 0; row < 6; ++row)
        EF_U8(0x0015f770u + 6u + row) = (uint8_t)(0x80u >> row);
    draw_glyph_e3040(0x5au, 2u, 1u, 1u);
    for (row = 0; row < 6; ++row) {
        assert(EF_U8(framebuffer + row * 16u + 2u + row) == 0x5au);
        assert(EF_U8(framebuffer + row * 16u + 2u) == (row ? 0 : 0x5au));
    }
    ef_decomp_memory_release();
}

int main(void)
{
    test_layout();
    test_ram();
    test_recovered_guest_functions();
    return 0;
}
