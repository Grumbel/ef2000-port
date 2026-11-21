/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"
#include "ef_platform.h"

/* Seven renderer stream entry points 0xc49c4..0xc4d43. Each takes one
 * callee-popped stack argument, the address of a word stream, decodes it
 * into the vertex/index scratch tables and hands the index list to a
 * polygon builder. The builders and the vertex loader take their inputs in
 * ESI (index list or source) and ECX (count), so they are called through
 * the guest_fn_* declarations in ef_abi.h. */

#define STREAM_SHADE_SELECTOR  0x0015918cu /* -1 = none */
#define STREAM_DEPTH           0x00159084u
#define STREAM_DEPTH_BASE      0x00164604u
#define STREAM_SHADE_MODE      0x0015907cu
#define STREAM_CLIP_FLAG       0x00158ed8u
#define STREAM_ABORT_ESP       0x00159064u /* overflow escape, see 0xc454a */
#define STREAM_COLOUR_MODE     0x00159138u
#define STREAM_INDICES         0x00159190u
#define STREAM_SHADES          0x0015977cu
#define STREAM_TEXTURE_UV      0x0015997cu
#define STREAM_TEXTURE_SHIFT   0x00158f0cu
#define STREAM_TEXTURE_ORIGIN  0x00159080u
#define TEXTURE_WINDOW_SEL     0x00485fa8u
#define VERTEX_STRIDE          0x30u

/* Every entry builds ENTER 0,0 and pushes ESI, EDI, EBP; ESP then lies 12
 * bytes below the frame. The entries carry a "frame" pragma so EBP is the
 * same frame here, and the escape slot gets the original value. */
#if defined(__WATCOMC__) && !defined(__GNUC__) && !defined(__clang__)
#pragma aux renderer_stream_saved_esp = "lea eax,[ebp-0ch]" value [eax] modify exact [eax]
#endif

static void begin_renderer_stream(uint32_t saved_esp)
{
	EF_U32(STREAM_SHADE_SELECTOR) = 0xffffffffu;
	EF_U32(STREAM_DEPTH) = EF_U32(STREAM_DEPTH_BASE);
	EF_U32(STREAM_SHADE_MODE) = 0;
	EF_U8(STREAM_CLIP_FLAG) = 0;
	EF_U32(STREAM_ABORT_ESP) = saved_esp;
}

/* Index list 0, 0x30, 0x60, ...: the byte offsets of the vertices that the
 * loader 0xc48dc just wrote. */
static void make_renderer_indices(uint32_t count)
{
	uint32_t i;
	for (i = 0; i < count; ++i)
		EF_U16(STREAM_INDICES + 2u * i) = (uint16_t)(i * VERTEX_STRIDE);
}

/* Texture coordinate pairs, shifted left by the texture scale; only the low
 * words are kept. The final dword closes the strip by repeating the first
 * pair. Returns the stream position after the pairs. */
static ef_addr32 read_renderer_texture_pairs(ef_addr32 src, uint32_t count)
{
	uint32_t shift = EF_U32(STREAM_TEXTURE_SHIFT) & 31u;
	uint32_t i;
	for (i = 0; i < count; ++i) {
		EF_U16(STREAM_TEXTURE_UV + 4u * i) = (uint16_t)((uint32_t)EF_U16(src) << shift);
		EF_U16(STREAM_TEXTURE_UV + 4u * i + 2u) = (uint16_t)((uint32_t)EF_U16(src + 2u) << shift);
		src += 4u;
	}
	EF_U32(STREAM_TEXTURE_UV + count * 4u) = EF_U32(STREAM_TEXTURE_UV);
	return src;
}

/* A count of zero is not a valid stream: the original counts down with
 * LOOP or DEC/JNZ and no zero guard, which would walk all of memory. These
 * entries return instead and do not claim equivalence for that input. */

/* 0xc49c4, 140 bytes: COUNT, one shade byte per vertex, then the 2D
 * positions. The shades become 8.8 values; the builder runs in shade mode 4. */
void render_colour_stream_c49c4(ef_addr32 stream)
{
	uint32_t count = EF_U16(stream);
	uint32_t i;
	if (count == 0)
		return;
	begin_renderer_stream(renderer_stream_saved_esp());
	stream += 2u;
	for (i = 0; i < count; ++i)
		EF_U32(STREAM_SHADES + 4u * i) = (uint32_t)EF_U8(stream++) << 8;
	guest_fn_c48dc(stream, count);
	make_renderer_indices(count);
	EF_U32(STREAM_SHADE_MODE) = 4u;
	guest_fn_d2584(STREAM_INDICES, count);
	EF_U32(STREAM_SHADE_MODE) = 0;
}

enum textured_builder {
	TEXTURED_PLAIN,       /* 0xd2634 */
	TEXTURED_BIASED,      /* 0xd2704, half a unit further back */
	TEXTURED_ATLAS,       /* 0xd2a7c */
	TEXTURED_HANDLER      /* 0xd2d7c */
};

/* COUNT, the texture pairs, then the 2D positions. Shared by four entries
 * that differ only in the polygon builder. */
static void render_textured_stream(ef_addr32 stream, uint32_t saved_esp,
	enum textured_builder builder)
{
	uint32_t count = EF_U16(stream);
	if (count == 0)
		return;
	begin_renderer_stream(saved_esp);
	stream = read_renderer_texture_pairs(stream + 2u, count);
	guest_fn_c48dc(stream, count);
	make_renderer_indices(count);
	update_fc9f8((int32_t)EF_U32(STREAM_TEXTURE_ORIGIN));
	platform_select_texture_window(EF_U16(TEXTURE_WINDOW_SEL));
	switch (builder) {
	case TEXTURED_PLAIN:
		guest_fn_d2634(STREAM_INDICES, count);
		break;
	case TEXTURED_BIASED:
		EF_U32(STREAM_DEPTH) = EF_U32(STREAM_DEPTH_BASE) + 0x8000u;
		guest_fn_d2704(STREAM_INDICES, count);
		break;
	case TEXTURED_ATLAS:
		guest_fn_d2a7c(STREAM_INDICES, count);
		break;
	default:
		guest_fn_d2d7c(STREAM_INDICES, count);
		break;
	}
}

/* 0xc4a50, 155 bytes. */
void render_texture_stream_c4a50(ef_addr32 stream)
{
	render_textured_stream(stream, renderer_stream_saved_esp(), TEXTURED_PLAIN);
}

/* 0xc4aeb, 170 bytes. */
void render_texture_stream_c4aeb(ef_addr32 stream)
{
	render_textured_stream(stream, renderer_stream_saved_esp(), TEXTURED_BIASED);
}

/* 0xc4b95, 155 bytes. */
void render_texture_stream_c4b95(ef_addr32 stream)
{
	render_textured_stream(stream, renderer_stream_saved_esp(), TEXTURED_ATLAS);
}

/* 0xc4c30, 155 bytes. */
void render_texture_stream_c4c30(ef_addr32 stream)
{
	render_textured_stream(stream, renderer_stream_saved_esp(), TEXTURED_HANDLER);
}

/* 0xc4ccb, 120 bytes: shade selector word, colour mode word, COUNT, then
 * the 2D positions. */
void render_index_stream_c4ccb(ef_addr32 stream)
{
	uint32_t count = EF_U16(stream + 4u);
	if (count == 0)
		return;
	begin_renderer_stream(renderer_stream_saved_esp());
	EF_U32(STREAM_SHADE_SELECTOR) = EF_U16(stream);
	EF_U8(STREAM_COLOUR_MODE) = (uint8_t)EF_U16(stream + 2u);
	guest_fn_c48dc(stream + 6u, count);
	make_renderer_indices(count);
	guest_fn_d2500(STREAM_INDICES, count);
}

/* 0xc4d43, 112 bytes: colour mode word, COUNT, then the 2D positions. */
void render_index_stream_c4d43(ef_addr32 stream)
{
	uint32_t count = EF_U16(stream + 2u);
	if (count == 0)
		return;
	begin_renderer_stream(renderer_stream_saved_esp());
	EF_U8(STREAM_COLOUR_MODE) = (uint8_t)EF_U16(stream);
	guest_fn_c48dc(stream + 4u, count);
	make_renderer_indices(count);
	guest_fn_d2500(STREAM_INDICES, count);
}
