/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* "RA" packed data, unpacked by 0x10da50 (caller 0xea420 checks the magic
 * word 0x4152 first). Header: +0 magic, +4 unpacked length, +8 unused word,
 * +0x0a checksum of the unpacked bytes; the stream starts at +0x0c.
 *
 * The stream mixes raw bytes with a bit buffer that is refilled from the
 * same byte pointer whenever its eight bits are used up, so a raw byte or
 * word is whatever follows the last bit byte loaded. Bits are read MSB
 * first. The helpers 0x10db10..0x10dc1d keep this state in AL/ESI and the
 * trees in the caller's frame; here they are inlined. */

struct ra_stream {
	ef_addr32 src;
	uint8_t buffer; /* remaining bits, terminated by a sentinel 1 */
};

/* The three decoding trees, laid out as in the original frame (EBP-0x850
 * upwards), so that even a tree index beyond its own table reads and writes
 * the same neighbouring table. Each tree is a pair of byte arrays indexed by
 * 2 * node + bit: CHILD is the next node, or 0 for a leaf, whose symbol is
 * in VALUE. Node 0 is the root. */
#define RA_DISTANCE_CHILD 0x000u
#define RA_DISTANCE_VALUE 0x020u
#define RA_LENGTH_CHILD   0x040u
#define RA_LENGTH_VALUE   0x240u
#define RA_RUN_CHILD      0x440u
#define RA_RUN_VALUE      0x640u
#define RA_TREES_SIZE     0x840u

static uint32_t ra_bit(struct ra_stream *s)
{
	uint32_t bit = (uint32_t)(s->buffer >> 7);
	uint8_t next;

	s->buffer = (uint8_t)(s->buffer << 1);
	if (s->buffer == 0) {
		next = EF_U8(s->src);
		s->src++;
		s->buffer = (uint8_t)((next << 1) | bit);
		bit = (uint32_t)(next >> 7);
	}
	return bit;
}

/* COUNT bits (1..8), first bit highest (0x10dc0d). */
static uint8_t ra_bits(struct ra_stream *s, uint8_t count)
{
	uint8_t value = 0;

	do {
		value = (uint8_t)((value << 1) | ra_bit(s));
	} while (--count != 0);
	return value;
}

static uint8_t ra_raw_byte(struct ra_stream *s)
{
	uint8_t value = EF_U8(s->src);

	s->src++;
	return value;
}

/* 0x10dbc1: a raw node count N, the child and leaf bit widths (3 bits each,
 * plus one), then 2 * N entries: a 1 bit and a leaf symbol, or a 0 bit and a
 * child node. A leaf entry clears its child; an inner entry leaves its
 * symbol untouched. */
static void ra_read_tree(struct ra_stream *s, uint8_t *trees, uint32_t child, uint32_t value)
{
	uint32_t entries = (uint32_t)ra_raw_byte(s) * 2u;
	uint8_t child_bits = (uint8_t)(ra_bits(s, 3) + 1u);
	uint8_t value_bits = (uint8_t)(ra_bits(s, 3) + 1u);

	do {
		if (ra_bit(s)) {
			trees[value] = ra_bits(s, value_bits);
			trees[child] = 0;
		} else {
			trees[child] = ra_bits(s, child_bits);
		}
		child++;
		value++;
	} while (--entries != 0);
}

/* Walk a tree from the root to a leaf (0x10db58). */
static uint8_t ra_decode(struct ra_stream *s, const uint8_t *trees, uint32_t child, uint32_t value)
{
	uint32_t entry;
	uint8_t node = 0;

	do {
		entry = 2u * node + ra_bit(s);
		node = trees[child + entry];
	} while (node != 0);
	return trees[value + entry];
}

/* Match length (0x10db10): symbols 0..0xfe directly; 0xff escapes to two
 * more bits and a raw byte, giving 0xff..0x4fe. */
static uint32_t ra_match_length(struct ra_stream *s, const uint8_t *trees)
{
	uint32_t length = ra_decode(s, trees, RA_LENGTH_CHILD, RA_LENGTH_VALUE);
	uint32_t high;

	if (length != 0xffu)
		return length;
	high = ra_bit(s);
	high = (high << 1) | ra_bit(s);
	return ((high << 8) | ra_raw_byte(s)) + 0xffu;
}

/* Match distance (0x10db7e): code 0 or 1 (or a negative byte) is the
 * distance itself; code C >= 2 is a leading 1 followed by C - 1 bits. The
 * copy starts DISTANCE + 1 bytes back. */
static uint32_t ra_match_distance(struct ra_stream *s, const uint8_t *trees)
{
	uint8_t code = ra_decode(s, trees, RA_DISTANCE_CHILD, RA_DISTANCE_VALUE);
	uint32_t distance = code;
	uint32_t bits;

	if ((int8_t)code >= 2) {
		distance = 1;
		bits = (uint32_t)code - 1u;
		do {
			distance = (distance << 1) | ra_bit(s);
		} while (--bits != 0);
	}
	return distance;
}

/* 0x10dc1d: two running byte sums over LENGTH bytes, low sum in the low
 * byte. As in the original, LENGTH must be at least 2. */
static uint16_t ra_checksum(ef_addr32 data, uint32_t length)
{
	uint32_t pairs = length >> 1;
	uint8_t sum = 0;
	uint8_t sum_of_sums = 0;

	do {
		sum = (uint8_t)(sum + EF_U8(data));
		sum_of_sums = (uint8_t)(sum_of_sums + sum);
		sum = (uint8_t)(sum + EF_U8(data + 1u));
		sum_of_sums = (uint8_t)(sum_of_sums + sum);
		data += 2u;
	} while (--pairs != 0);
	if (length & 1u) {
		sum = (uint8_t)(sum + EF_U8(data));
		sum_of_sums = (uint8_t)(sum_of_sums + sum);
	}
	return (uint16_t)(((uint32_t)sum_of_sums << 8) | sum);
}

/* 0x10da50, 192 bytes, stdcall (callee pops both arguments): unpack PACKED
 * to OUT and return 1 when the checksum of the unpacked length matches the
 * header, else 0. After one skipped bit, each block holds a raw step count
 * and the length, run and distance trees; each step copies a run of raw
 * bytes and then, for a non-zero match length, a match from the output
 * already written (byte by byte, so short distances repeat). A 1 bit after
 * the steps starts another block. Zero counts, empty trees and paths into
 * tree entries the stream never wrote are not valid input: the original
 * counts down without a zero check and reads stale frame bytes there. The
 * original's PUSH DS / POP ES is a no-op in the flat model. */
uint32_t unpack_ra_10da50(ef_addr32 packed, ef_addr32 out)
{
	uint8_t trees[RA_TREES_SIZE];
	struct ra_stream s;
	uint32_t length = EF_U32(packed + 4u);
	uint16_t checksum = EF_U16(packed + 0x0au);
	ef_addr32 dst = out;
	ef_addr32 from;
	uint32_t steps;
	uint32_t count;

	s.src = packed + 0x0cu;
	s.buffer = 0x80u;
	ra_bit(&s);
	do {
		steps = EF_U16(s.src);
		s.src += 2u;
		ra_read_tree(&s, trees, RA_LENGTH_CHILD, RA_LENGTH_VALUE);
		ra_read_tree(&s, trees, RA_RUN_CHILD, RA_RUN_VALUE);
		ra_read_tree(&s, trees, RA_DISTANCE_CHILD, RA_DISTANCE_VALUE);
		do {
			for (count = ra_decode(&s, trees, RA_RUN_CHILD, RA_RUN_VALUE); count != 0; --count)
				EF_U8(dst++) = ra_raw_byte(&s);
			count = ra_match_length(&s, trees);
			if ((uint16_t)count != 0) {
				from = dst - ra_match_distance(&s, trees) - 1u;
				for (; count != 0; --count)
					EF_U8(dst++) = EF_U8(from++);
			}
		} while (--steps != 0);
	} while (ra_bit(&s));
	return ra_checksum(out, length) == checksum ? 1u : 0u;
}
