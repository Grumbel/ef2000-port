/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* The bit accumulator's low bit is a sentinel. A zero after shifting AL
 * causes a buffered-byte refill and preserves the carry flag into ADC.
 * ESI and EDX are explicit in the C model; the original uses registers. */
static uint8_t next_bit(uint8_t *buffer, ef_addr32 *source, int32_t *remaining)
{
    uint8_t value = *buffer;
    uint8_t bit = value >> 7;
    value <<= 1;
    if (value == 0u) {
        if (--*remaining < 0)
            bitstream_refill_fa332(source, remaining);
        value = EF_U8((*source)++);
        bit = value >> 7;
        value = (uint8_t)((value << 1) | 1u);
    }
    *buffer = value;
    return bit;
}

/* fa16e, 102 bytes: first Huffman tree with the 0xff extended-symbol escape.
 * The escape appends two bits in CH and a literal byte in CL, then adds 255
 * to the entire ECX result (not just CL). */
uint32_t bitstream_decode_extended_fa16e(uint8_t *buffer,
                                          ef_addr32 *source, int32_t *remaining)
{
    uint32_t node = 0u;
    uint8_t link;
    do {
        node = 2u * node + next_bit(buffer, source, remaining);
        link = EF_U8(0x162ba0u + node);
        if (link)
            node = link;
    } while (link);
    link = EF_U8(0x162da0u + node);
    if (link != 0xffu)
        return link;
    {
        uint32_t high = (uint32_t)next_bit(buffer, source, remaining) << 1;
        high |= next_bit(buffer, source, remaining);
        if (--*remaining < 0)
            bitstream_refill_fa332(source, remaining);
        return ((high << 8) | EF_U8((*source)++)) + 0xffu;
    }
}

static uint8_t read_bits(uint8_t count, uint8_t *buffer,
                         ef_addr32 *source, int32_t *remaining)
{
    uint8_t acc = 0;
    while (count--)
        acc = (uint8_t)((acc << 1) | next_bit(buffer, source, remaining));
    return acc;
}

/* fa251, 112 bytes. The first literal byte is a PAIR count; then two
 * 3-bit lengths (plus one) govern the values written to parallel byte
 * streams. One output is zero when the bit chooses the other stream. */
void bitstream_decode_pair_streams_fa251(uint8_t *buffer,
                                        ef_addr32 *source, int32_t *remaining,
                                        ef_addr32 out_a, ef_addr32 out_b)
{
    uint32_t count;
    uint8_t width_a, width_b;
    if (--*remaining < 0)
        bitstream_refill_fa332(source, remaining);
    count = (uint32_t)EF_U8((*source)++) * 2u;
    EF_U32(0x1631e6u) = count;
    width_a = (uint8_t)(read_bits(3, buffer, source, remaining) + 1u);
    EF_U8(0x1631e4u) = width_a;
    width_b = (uint8_t)(read_bits(3, buffer, source, remaining) + 1u);
    EF_U8(0x1631e5u) = width_b;
    while (count--) {
        if (next_bit(buffer, source, remaining)) {
            EF_U8(out_b) = read_bits(width_b, buffer, source, remaining);
            EF_U8(out_a) = 0;
        } else {
            EF_U8(out_a) = read_bits(width_a, buffer, source, remaining);
        }
        ++out_a;
        ++out_b;
        --EF_U32(0x1631e6u);
    }
}
