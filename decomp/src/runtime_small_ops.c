/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_guest_memory.h"

static uint8_t read8(ef_addr32 address)
{
    return *(const uint8_t *)ef_guest_memory_ptr(address);
}

static uint16_t read16(ef_addr32 address)
{
    return (uint16_t)((uint16_t)read8(address) |
                      ((uint16_t)read8(address + 1u) << 8));
}

static uint32_t read32(ef_addr32 address)
{
    return (uint32_t)read16(address) | ((uint32_t)read16(address + 2u) << 16);
}

static void write8(ef_addr32 address, uint8_t value)
{
    *(uint8_t *)ef_guest_memory_ptr(address) = value;
}

static void write16(ef_addr32 address, uint16_t value)
{
    write8(address, (uint8_t)value);
    write8(address + 1u, (uint8_t)(value >> 8));
}

static void write32(ef_addr32 address, uint32_t value)
{
    write16(address, (uint16_t)value);
    write16(address + 2u, (uint16_t)(value >> 16));
}

/* 0xd530, 29 bytes: copy three unrelated packed guest settings. */
void copy_runtime_input_state(ef_addr32 word_a, ef_addr32 word_b,
                              ef_addr32 byte_c)
{
    write16(word_a, read16(0x00197696));
    write16(word_b, read16(0x0019bb5e));
    write8(byte_c, read8(0x00121e8c));
}

/* 0x10640, 12 bytes. Indirect sample callback updates current tick. */
uint32_t sample_runtime_tick(void)
{
    uint32_t current = guest_read_runtime_tick();
    write32(0x00197c14, current);
    return current;
}

/* 0x1065c, 42 bytes. The signed wrapped tick delta cannot be less than
 * two, including when the clock rolls backwards. */
int32_t advance_runtime_tick(void)
{
    sample_runtime_tick();
    uint32_t current = read32(0x00197c14);
    uint32_t previous = read32(0x00197c10);
    int32_t elapsed = (int32_t)(current - previous);
    write32(0x00197c10, current);
    return elapsed >= 2 ? elapsed : 2;
}

static int32_t shift11_floor(int32_t value)
{
    int32_t quotient = value / 2048;
    return (value < 0 && value % 2048 != 0) ? quotient - 1 : quotient;
}

/* 0x106b0, 94 bytes. Divide both position components by 2^11, then
 * store their residuals centered around -1024. The original preserves
 * only the low 16 bits of each field. */
void pack_runtime_position(int16_t heading, int32_t x,
                           int32_t y, uint8_t code)
{
    int32_t coarse_x = shift11_floor(x);
    int32_t coarse_y = shift11_floor(y);
    write16(0x00197c28, (uint16_t)heading);
    write16(0x00197c2a, (uint16_t)coarse_x);
    write16(0x00197c2c, (uint16_t)coarse_y);
    write16(0x00197c24,
            (uint16_t)((uint32_t)x - ((uint32_t)coarse_x << 11) - 1024u));
    write16(0x00197c26,
            (uint16_t)((uint32_t)y - ((uint32_t)coarse_y << 11) - 1024u));
    write8(0x00121ec6, 0);
    write8(0x00197c2e, code);
}

/* 0x10f28, 71 bytes: the four exact unsigned AX command cases
 * leave, swap/negate or negate two packed words in place. */
void transform_runtime_word_pair(uint16_t mode, ef_addr32 a, ef_addr32 b)
{
    uint16_t first = read16(a);
    uint16_t second = read16(b);
    switch (mode) {
    case 1:
        write16(a, (uint16_t)(0u - second));
        write16(b, first);
        break;
    case 2:
        write16(a, (uint16_t)(0u - first));
        write16(b, (uint16_t)(0u - second));
        break;
    case 3:
        write16(a, second);
        write16(b, (uint16_t)(0u - first));
        break;
    default:
        break;
    }
}
