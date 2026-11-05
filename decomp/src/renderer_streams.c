/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* Draft reconstruction of seven closely related renderer stream entry points.
 * The original functions receive a
 * single callee-cleaned stack pointer, not a Watcom register argument. The
 * final splice must supply that ABI and the exact saved-ESP value here. */

/* 0xc49c4..0xc4d43: shared entry prologue, including the stack diagnostic
 * written after ENTER 0,0 and three saved registers. */
static void begin_renderer_stream(void)
{
    EF_U32(0x0015918cu) = 0xffffffffu;
    EF_U32(0x00159084u) = EF_U32(0x00164604u);
    EF_U32(0x0015907cu) = 0;
    EF_U8(0x00158ed8u) = 0;
    EF_U32(0x00159064u) = renderer_stream_saved_esp();
}

static void make_renderer_indices(uint32_t count)
{
    uint32_t i;
    for (i = 0; i < count; ++i)
        EF_U16(0x00159190u + 2u * i) = (uint16_t)(i * 0x30u);
}

/* The 16-bit source coordinates are shifted as 32-bit EAX values but only
 * their low words are stored. The final dword closes the strip by repeating
 * the first coordinate pair. */
static ef_addr32 read_renderer_texture_pairs(ef_addr32 src, uint32_t count)
{
    uint32_t shift = EF_U32(0x00158f0cu) & 31u;
    uint32_t i;
    for (i = 0; i < count; ++i) {
        EF_U16(0x0015997cu + 4u * i) = (uint16_t)((uint32_t)EF_U16(src) << shift);
        EF_U16(0x0015997eu + 4u * i) = (uint16_t)((uint32_t)EF_U16(src + 2u) << shift);
        src += 4u;
    }
    EF_U32(0x0015997cu + count * 4u) = EF_U32(0x0015997cu);
    return src;
}

/* A count of zero is not a valid stream: the original uses LOOP or a
 * decrement-and-branch with no zero guard, potentially walking all of RAM.
 * These drafts deliberately do not claim equivalence for malformed input. */

/* 0xc49c4, 140 bytes: per-vertex shade bytes, then 2D positions. */
void render_colour_stream_c49c4(ef_addr32 stream)
{
    uint32_t count = EF_U16(stream);
    uint32_t i;
    if (count == 0)
        return;
    begin_renderer_stream();
    stream += 2u;
    for (i = 0; i < count; ++i)
        EF_U32(0x0015977cu + 4u * i) = (uint32_t)EF_U8(stream++) << 8;
    load_vertices_c48dc(stream, count);
    make_renderer_indices(count);
    EF_U32(0x0015907cu) = 4u;
    build_render_object_d2584(0x00159190u, count);
    EF_U32(0x0015907cu) = 0;
}

/* Shared by four original functions; their last dispatches and one depth
 * offset are different, but the stream decoding is byte-for-byte identical. */
static void render_textured_stream(ef_addr32 stream, unsigned variant)
{
    uint32_t count = EF_U16(stream);
    if (count == 0)
        return;
    begin_renderer_stream();
    stream = read_renderer_texture_pairs(stream + 2u, count);
    load_vertices_c48dc(stream, count);
    make_renderer_indices(count);
    update_fc9f8((int32_t)EF_U32(0x00159080u));
    if (variant == 0u) {
        build_textured_object_d2634(0x00159190u, count);
    } else if (variant == 1u) {
        EF_U32(0x00159084u) = EF_U32(0x00164604u) + 0x8000u;
        build_render_list_d2704(0x00159190u, count);
    } else if (variant == 2u) {
        build_render_list_d2c84(0x00159190u, count);
    } else {
        build_render_list_d2d7c(0x00159190u, count);
    }
}

/* 0xc4a50, 155 bytes. */
void render_texture_stream_c4a50(ef_addr32 stream)
{
    render_textured_stream(stream, 0u);
}

/* 0xc4aeb, 170 bytes: extra half-unit depth bias before rendering. */
void render_texture_stream_c4aeb(ef_addr32 stream)
{
    render_textured_stream(stream, 1u);
}

/* 0xc4b95, 155 bytes. */
void render_texture_stream_c4b95(ef_addr32 stream)
{
    render_textured_stream(stream, 2u);
}

/* 0xc4c30, 155 bytes: an ENTER/RET 4 routine missing from the current
 * Ghidra-derived function inventory. Confirmed by payload disassembly. */
void render_texture_stream_c4c30(ef_addr32 stream)
{
    render_textured_stream(stream, 3u);
}

/* 0xc4ccb, 120 bytes: first two words set the shade selector and mode,
 * the third counts the vertices. Also missed by Ghidra's inventory. */
void render_index_stream_c4ccb(ef_addr32 stream)
{
    uint32_t count;
    if (EF_U16(stream + 4u) == 0)
        return;
    begin_renderer_stream();
    EF_U32(0x0015918cu) = EF_U16(stream);
    EF_U8(0x00159138u) = (uint8_t)EF_U16(stream + 2u);
    count = EF_U16(stream + 4u);
    load_vertices_c48dc(stream + 6u, count);
    make_renderer_indices(count);
    build_render_list_d2500(0x00159190u, count);
}

/* 0xc4d43, 112 bytes: one mode word followed by count and vertices. */
void render_index_stream_c4d43(ef_addr32 stream)
{
    uint32_t count;
    if (EF_U16(stream + 2u) == 0)
        return;
    begin_renderer_stream();
    EF_U8(0x00159138u) = (uint8_t)EF_U16(stream);
    count = EF_U16(stream + 2u);
    load_vertices_c48dc(stream + 4u, count);
    make_renderer_indices(count);
    build_render_list_d2500(0x00159190u, count);
}
