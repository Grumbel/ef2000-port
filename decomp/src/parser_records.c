/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_guest_memory.h"

/* X-32 guest addresses, not pointers into the host process. */
enum {
    parser_cursor = 0x00121ea8,
    parser_error_context = 0x00121eac,
    parser_input = 0x001976b0,
    parser_token = 0x00197abe,
    parser_enum_table = 0x00132f5c,
    parser_active_buffer = 0x001379c4
};

static uint8_t read8(ef_addr32 address)
{
    return *(const uint8_t *)ef_guest_memory_ptr(address);
}

static uint16_t read16(ef_addr32 address)
{
    return (uint16_t)((uint16_t)read8(address) | ((uint16_t)read8(address + 1u) << 8));
}

static ef_addr32 read32(ef_addr32 address)
{
    return (ef_addr32)read16(address) | ((ef_addr32)read16(address + 2u) << 16);
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

static void write32(ef_addr32 address, ef_addr32 value)
{
    write16(address, (uint16_t)value);
    write16(address + 2u, (uint16_t)(value >> 16));
}

/* 0xe710: release an optional +0x0a child, then the root; re-read the
 * root after the first callback because the original does so. */
void release_parser_buffers(void)
{
    ef_addr32 root = read32(parser_active_buffer);
    if (root == 0) { return; }
    ef_addr32 child = read32(root + 0x0au);
    if (child != 0) { guest_release_parser_buffer(child); }
    guest_release_parser_buffer(read32(parser_active_buffer));
    write32(parser_active_buffer, 0);
}

/* 0xe7dc: copies the initializer byte (not necessarily a NUL) into
 * the first input and token slots, zeroing the counters and cursor. */
void initialize_parser_records(void)
{
    uint8_t initializer = read8(0x00180c08);
    write32(0x00121eb4, 0);
    write32(0x00121eb0, 0);
    write32(parser_error_context, 0);
    write8(parser_input, initializer);
    write8(parser_token, initializer);
    write16(parser_cursor, 0);
}

/* 0xe89c: skip spaces/tabs, read a quoted or comma/whitespace-delimited
 * token, then write NUL and the low-16-bit input cursor. Only 80 source
 * bytes fit; a truncated token deliberately leaves the cursor mid-token. */
uint32_t read_parser_token(void)
{
    uint16_t cursor = read16(parser_cursor);
    uint32_t count = 0;
    uint8_t ch;
    while ((ch = read8(parser_input + (uint32_t)(int16_t)cursor)) == ' ' || ch == '\t') {
        ++cursor;
    }
    if (ch == '"') {
        ++cursor;
        for (;;) {
            ch = read8(parser_input + (uint32_t)(int16_t)cursor);
            if (ch == '"' || ch == 0 || count >= 0x50u) { break; }
            write8(parser_token + count++, ch);
            ++cursor;
        }
        if (read8(parser_input + (uint32_t)(int16_t)cursor) == '"') { ++cursor; }
    } else {
        for (;;) {
            ch = read8(parser_input + (uint32_t)(int16_t)cursor);
            if (ch == ' ' || ch == '\t' || ch == 0 || ch == ',' || count >= 0x50u) {
                break;
            }
            write8(parser_token + count++, ch);
            ++cursor;
        }
    }
    write8(parser_token + count, 0);
    write16(parser_cursor, cursor);
    return count;
}

/* 0xe980: decimal/float conversion remains an original guest dependency. */
uint32_t read_parser_decimal(void)
{
    read_parser_token();
    return guest_convert_parser_decimal(parser_token);
}

/* 0xe95c: a missing token yields -1; otherwise the original integer
 * conversion is a tail jump and owns the return value. */
int32_t read_parser_integer(void)
{
    read_parser_token();
    if (guest_string_length(parser_token) == 0) { return -1; }
    return guest_convert_parser_integer(parser_token);
}

/* 0xe990: allocate strlen+1 bytes via the game's indirect allocator,
 * then copy the token into the resulting guest buffer. */
ef_addr32 duplicate_parser_token(void)
{
    read_parser_token();
    uint32_t bytes = guest_string_length(parser_token) + 1u;
    ef_addr32 result = guest_allocate_parser_text(bytes, parser_token);
    guest_string_copy(result, parser_token);
    return result;
}

/* 0xe9bc: 11 enum names, compare-zero means found. On exhaustion,
 * call the original indirect diagnostic with the three source arguments. */
uint32_t read_parser_enum(void)
{
    read_parser_token();
    for (uint32_t i = 0; i < 11u; ++i) {
        if (guest_compare_parser_token(parser_token, read32(parser_enum_table + i * 4u)) == 0) {
            return i;
        }
    }
    guest_report_parser_enum_error(0x00180c48, parser_token, read32(parser_error_context));
    return 0;
}

/* 0xee5c: three guest decimal conversions followed by three integer
 * conversions, with word truncation at +0x0c/+0x0e/+0x10. */
uint32_t read_parser_six_field_record(ef_addr32 destination)
{
    write32(destination, read_parser_decimal());
    write32(destination + 4u, read_parser_decimal());
    write32(destination + 8u, read_parser_decimal());
    write16(destination + 0x0cu, (uint16_t)read_parser_integer());
    write16(destination + 0x0eu, (uint16_t)read_parser_integer());
    write16(destination + 0x10u, (uint16_t)read_parser_integer());
    return 1;
}
