/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static uint8_t memory[0x230000];
static unsigned release_count, token_converts, int_converts, allocations;
static unsigned copies, enum_comparisons, enum_errors;
static ef_addr32 released[3], allocated_source;
static uint32_t allocated_size;
static int rewrite_root_on_child;

enum {
    cursor_at = 0x121ea8,
    error_context_at = 0x121eac,
    active_root_at = 0x1379c4,
    input_at = 0x1976b0,
    token_at = 0x197abe,
    enum_table_at = 0x132f5c
};

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(memory));
    return memory + address;
}

static uint16_t get16(ef_addr32 addr)
{
    assert(addr + 1 < sizeof(memory));
    return (uint16_t)(memory[addr] | ((uint16_t)memory[addr + 1] << 8));
}

static ef_addr32 get32(ef_addr32 addr)
{
    return (ef_addr32)get16(addr) | ((ef_addr32)get16(addr + 2) << 16);
}

static void put16(ef_addr32 addr, uint16_t value)
{
    memory[addr] = (uint8_t)value;
    memory[addr + 1] = (uint8_t)(value >> 8);
}

static void put32(ef_addr32 addr, ef_addr32 value)
{
    put16(addr, (uint16_t)value);
    put16(addr + 2, (uint16_t)(value >> 16));
}

static void reset_test(void)
{
    memset(memory, 0, sizeof(memory));
    memset(released, 0, sizeof(released));
    release_count = token_converts = int_converts = allocations = 0;
    copies = enum_comparisons = enum_errors = 0;
    allocated_size = allocated_source = 0;
    rewrite_root_on_child = 0;
}

void guest_release_parser_buffer(ef_addr32 buffer)
{
    assert(release_count < 3);
    released[release_count++] = buffer;
    if (rewrite_root_on_child && release_count == 1) {
        put32(active_root_at, 0x1d1000);
    }
}

uint32_t guest_string_length(ef_addr32 text)
{
    return (uint32_t)strlen((const char *)ef_guest_memory_ptr(text));
}

uint32_t guest_convert_parser_decimal(ef_addr32 text)
{
    assert(text == token_at);
    ++token_converts;
    return (uint32_t)strtol((const char *)ef_guest_memory_ptr(text), NULL, 10);
}

int32_t guest_convert_parser_integer(ef_addr32 text)
{
    assert(text == token_at);
    ++int_converts;
    return (int32_t)strtol((const char *)ef_guest_memory_ptr(text), NULL, 10);
}

ef_addr32 guest_allocate_parser_text(uint32_t bytes, ef_addr32 source)
{
    ++allocations;
    allocated_size = bytes;
    allocated_source = source;
    return 0x1b0000;
}

void guest_string_copy(ef_addr32 dest, ef_addr32 src)
{
    assert(dest == 0x1b0000 && src == token_at);
    ++copies;
    strcpy((char *)ef_guest_memory_ptr(dest), (const char *)ef_guest_memory_ptr(src));
}

int32_t guest_compare_parser_token(ef_addr32 token, ef_addr32 candidate)
{
    assert(token == token_at);
    ++enum_comparisons;
    return strcmp((const char *)ef_guest_memory_ptr(token),
                  (const char *)ef_guest_memory_ptr(candidate));
}

void guest_report_parser_enum_error(ef_addr32 format, ef_addr32 token,
                                    uint32_t context)
{
    assert(format == 0x180c48 && token == token_at);
    assert(context == 0xcafebabeu);
    ++enum_errors;
}

static void check_release(void)
{
    reset_test();
    release_parser_buffers();
    assert(release_count == 0);

    put32(active_root_at, 0x1d0000);
    put32(0x1d000a, 0x1d0020);
    release_parser_buffers();
    assert(release_count == 2);
    assert(released[0] == 0x1d0020 && released[1] == 0x1d0000);
    assert(get32(active_root_at) == 0);

    release_count = 0;
    put32(active_root_at, 0x1d0000);
    rewrite_root_on_child = 1;
    release_parser_buffers();
    assert(released[0] == 0x1d0020 && released[1] == 0x1d1000);
    assert(get32(active_root_at) == 0);
}

static void check_initialize(void)
{
    reset_test();
    memory[0x180c08] = 0x7e;
    put32(0x121eb4, 0x12345678);
    put32(0x121eb0, 0x12345678);
    put32(error_context_at, 0x12345678);
    put16(cursor_at, 99);
    initialize_parser_records();
    assert(get32(0x121eb4) == 0 && get32(0x121eb0) == 0);
    assert(get32(error_context_at) == 0 && get16(cursor_at) == 0);
    assert(memory[input_at] == 0x7e && memory[token_at] == 0x7e);
}

static void check_tokens(void)
{
    reset_test();
    memcpy(memory + input_at, "  \"hello world\" next,more", 25);
    assert(read_parser_token() == 11);
    assert(strcmp((char *)memory + token_at, "hello world") == 0);
    assert(get16(cursor_at) == 15);
    assert(read_parser_token() == 4);
    assert(strcmp((char *)memory + token_at, "next") == 0);
    assert(get16(cursor_at) == 20);
    assert(read_parser_token() == 0 && get16(cursor_at) == 20);
    put16(cursor_at, 21);
    assert(read_parser_token() == 4);
    assert(strcmp((char *)memory + token_at, "more") == 0);
    assert(read_parser_token() == 0);

    reset_test();
    memset(memory + input_at, 'x', 82);
    memory[input_at + 82] = 0;
    assert(read_parser_token() == 80);
    assert(get16(cursor_at) == 80);
    assert(memory[token_at + 79] == 'x' && memory[token_at + 80] == 0);
    assert(read_parser_token() == 2 && get16(cursor_at) == 82);
}

static void check_values(void)
{
    reset_test();
    memcpy(memory + input_at, " 17 -28", 8);
    assert(read_parser_decimal() == 17 && token_converts == 1);
    assert(read_parser_integer() == -28 && int_converts == 1);
    assert(read_parser_integer() == -1 && int_converts == 1);

    reset_test();
    memcpy(memory + input_at, "\"named value\"", 14);
    assert(duplicate_parser_token() == 0x1b0000);
    assert(allocations == 1 && copies == 1);
    assert(allocated_source == token_at && allocated_size == 12);
    assert(strcmp((char *)memory + 0x1b0000, "named value") == 0);
}

static void check_enums(void)
{
    reset_test();
    memcpy(memory + input_at, "GAMMA", 6);
    memcpy(memory + 0x1c0000, "ALPHA", 6);
    memcpy(memory + 0x1c0010, "GAMMA", 6);
    for (unsigned i = 0; i < 11; ++i) { put32(enum_table_at + i * 4, 0x1c0000); }
    put32(enum_table_at + 3 * 4, 0x1c0010);
    assert(read_parser_enum() == 3);
    assert(enum_comparisons == 4 && enum_errors == 0);

    reset_test();
    memcpy(memory + input_at, "INVALID", 8);
    memcpy(memory + 0x1c0000, "ALPHA", 6);
    for (unsigned i = 0; i < 11; ++i) { put32(enum_table_at + i * 4, 0x1c0000); }
    put32(error_context_at, 0xcafebabeu);
    assert(read_parser_enum() == 0);
    assert(enum_comparisons == 11 && enum_errors == 1);
}

static void check_record(void)
{
    reset_test();
    memcpy(memory + input_at, "12 -4 6 7 -8 65535", sizeof("12 -4 6 7 -8 65535"));
    assert(read_parser_six_field_record(0x1a2000) == 1);
    assert(get32(0x1a2000) == 12u);
    assert(get32(0x1a2004) == (uint32_t)-4);
    assert(get32(0x1a2008) == 6u);
    assert(get16(0x1a200c) == 7);
    assert(get16(0x1a200e) == (uint16_t)-8);
    assert(get16(0x1a2010) == 65535);
    assert(token_converts == 3 && int_converts == 3);
}

int main(void)
{
    check_release();
    check_initialize();
    check_tokens();
    check_values();
    check_enums();
    check_record();
    return 0;
}
