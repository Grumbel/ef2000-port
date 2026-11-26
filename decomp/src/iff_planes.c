/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/* Interleaved bitmap (ILBM) row conversion: bitplanes to chunky pixels. */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define IFF_MAX_PLANES      8u
#define PATTERN_PLANE_BYTES 0x40u /* 16 nibble patterns of 4 bytes */

/* Four pixels from one nibble of every plane: plane P contributes the
 * pattern for its nibble from the P-th 64-byte block of PATTERN. All
 * planes are read before the four bytes are written, as in the original. */
static void expand_nibbles(ef_addr32 pixels, ef_addr32 planes, ef_addr32 pattern,
                           uint32_t count, uint32_t plane_bytes, int high)
{
	uint8_t out[4];
	uint32_t plane;
	uint32_t i;

	out[0] = out[1] = out[2] = out[3] = 0u;
	for (plane = 0u; plane < count; ++plane) {
		uint32_t bits = EF_U8(planes + plane * plane_bytes);
		ef_addr32 entry;

		bits = high ? bits >> 4 : bits & 0x0fu;
		entry = pattern + plane * PATTERN_PLANE_BYTES + bits * 4u;
		for (i = 0u; i < 4u; ++i)
			out[i] |= EF_U8(entry + i);
	}
	for (i = 0u; i < 4u; ++i)
		EF_U8(pixels + i) = out[i];
}

/* 0xc1ee8, 4139 bytes. Convert one decoded ILBM row to pixels. PLANES
 * holds g_iff_plane_count planes of g_iff_plane_bytes bytes each; every
 * byte column yields eight pixels (high nibble first) built by ORing the
 * per-plane patterns from fill_bit_pattern_table_c2f80. A plane count
 * above 8 writes nothing but still advances, like the original switch's
 * default. Both globals are re-read for every column. */
void planes_to_pixels_c1ee8(ef_addr32 pixels, ef_addr32 planes, ef_addr32 pattern)
{
	int32_t column;

	for (column = 0; column < (int32_t)g_iff_plane_bytes; ++column) {
		uint32_t count = g_iff_plane_count;

		if (count != 0u && count <= IFF_MAX_PLANES) {
			uint32_t plane_bytes = g_iff_plane_bytes;

			expand_nibbles(pixels, planes, pattern, count, plane_bytes, 1);
			expand_nibbles(pixels + 4u, planes, pattern, count, plane_bytes, 0);
		}
		pixels += 8u;
		++planes;
	}
}
