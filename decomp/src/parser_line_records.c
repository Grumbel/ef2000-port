/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_guest_memory.h"

static uint8_t read8(ef_addr32 address)
{
    return *(const uint8_t *)ef_guest_memory_ptr(address);
}

static ef_addr32 read32(ef_addr32 address)
{
    ef_addr32 value = 0;
    for (unsigned i = 0; i < 4u; ++i) {
        value |= (ef_addr32)read8(address + i) << (8u * i);
    }
    return value;
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

/* 0xe810, 138 bytes. Skip leading ASCII spaces and tabs, ignore lines
 * beginning with a semicolon, and retain the original >=1024 warning
 * without preventing the subsequent write (ORIGINAL BUG: overflow). */
uint8_t read_parser_line(void)
{
    write16(0x00121ea8, 0);
    for (;;) {
        uint32_t count = 0;
        uint8_t c = guest_parser_next_character();
        while (c == ' ' || c == '\t') {
            c = guest_parser_next_character();
        }
        uint8_t last = c;
        while (last != 0 && last != '\n') {
            if (count >= 0x400u) {
                guest_parser_line_overflow(0x00180c0c);
            }
            write8(0x001976b0 + count, last);
            ++count;
            last = guest_parser_next_character();
        }
        write8(0x001976b0 + count, 0);
        write32(0x00121eac, read32(0x00121eac) + 1u);
        uint8_t first = read8(0x001976b0);
        if (first == ';' || (first == 0 && last != 0)) { continue; }
        return first;
    }
}

/* 0xf8f8, 30 bytes. Free the optional guest-owned text block and
 * unconditionally clear its pointer afterwards. */
void release_secondary_parser_text(void)
{
    ef_addr32 text = read32(0x001a5904);
    if (text != 0) { guest_release_parser_buffer(text); }
    write32(0x001a5904, 0);
}

/* 0xfe4c, 125 bytes. Two token advances before the first allocated
 * field (read_parser_token then duplicate_parser_token, which tokenizes
 * once more); then five byte and six word conversions. */
uint32_t read_parser_text_record(ef_addr32 record)
{
    read_parser_line();
    read_parser_token();
    ef_addr32 name = duplicate_parser_token();
    write8(record + 9u, 0);
    write32(record, name);
    for (unsigned i = 5; i <= 9u; ++i) {
        write8(record + i, (uint8_t)read_parser_integer());
    }
    for (unsigned i = 0; i < 6u; ++i) {
        write16(record + 10u + i * 2u, (uint16_t)read_parser_integer());
    }
    return 1;
}
