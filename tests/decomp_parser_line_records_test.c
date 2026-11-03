/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"

#include <assert.h>
#include <string.h>

static uint8_t mem[0x230000];
static const char *input_stream;
static unsigned offset, char_reads, overflows, releases;
static unsigned token_reads, duplicates, integer_reads;
static ef_addr32 last_released;

void *ef_guest_memory_ptr(ef_addr32 addr)
{
    assert(addr < sizeof(mem));
    return mem + addr;
}

static uint32_t get32(ef_addr32 addr)
{
    return (uint32_t)mem[addr] | ((uint32_t)mem[addr + 1] << 8) |
           ((uint32_t)mem[addr + 2] << 16) | ((uint32_t)mem[addr + 3] << 24);
}

static uint16_t get16(ef_addr32 addr)
{
    return (uint16_t)(mem[addr] | ((uint16_t)mem[addr + 1] << 8));
}

static void put32(ef_addr32 addr, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) { mem[addr + i] = (uint8_t)(value >> (8u * i)); }
}

uint8_t guest_parser_next_character(void)
{
    ++char_reads;
    unsigned char ch = (unsigned char)input_stream[offset];
    if (ch) { ++offset; }
    return ch;
}

void guest_parser_line_overflow(ef_addr32 message)
{
    assert(message == 0x180c0c);
    ++overflows;
}

void guest_release_parser_buffer(ef_addr32 ptr)
{
    last_released = ptr;
    ++releases;
}

uint32_t read_parser_token(void)
{
    ++token_reads;
    return 0;
}

ef_addr32 duplicate_parser_token(void)
{
    ++duplicates;
    return 0x1c0000;
}

int32_t read_parser_integer(void)
{
    ++integer_reads;
    return (int32_t)((int)integer_reads * 100 + (int)integer_reads);
}

static void reset(void)
{
    memset(mem, 0, sizeof(mem));
    offset = char_reads = overflows = releases = 0;
    token_reads = duplicates = integer_reads = 0;
    last_released = 0;
}

static void test_lines(void)
{
    reset();
    input_stream = " ;a comment\n  \t\n\talpha beta\n";
    assert(read_parser_line() == 'a');
    assert(strcmp((char *)(mem + 0x1976b0), "alpha beta") == 0);
    assert(get16(0x121ea8) == 0);
    assert(get32(0x121eac) == 3);

    reset();
    input_stream = "";
    assert(read_parser_line() == 0);
    assert(get32(0x121eac) == 1);

    reset();
    input_stream = "X\n";
    assert(read_parser_line() == 'X');
    assert(mem[0x1976b0] == 'X' && mem[0x1976b1] == 0);
    assert(char_reads == 2);
}

static void test_release(void)
{
    reset();
    release_secondary_parser_text();
    assert(releases == 0);
    put32(0x1a5904, 0x1d0000);
    release_secondary_parser_text();
    assert(releases == 1 && last_released == 0x1d0000);
    assert(get32(0x1a5904) == 0);
}

static void test_text_record(void)
{
    reset();
    input_stream = "COMMAND\n";
    mem[0x1d0014] = 0xfe;
    assert(read_parser_text_record(0x1d0000) == 1);
    assert(get32(0x1d0000) == 0x1c0000);
    assert(token_reads == 1 && duplicates == 1 && integer_reads == 11);
    assert(mem[0x1d0005] == 101 && mem[0x1d0009] == (uint8_t)505);
    assert(get16(0x1d000a) == 606 && get16(0x1d0014) == 1111);
    assert(get32(0x121eac) == 1);
}

int main(void)
{
    test_lines();
    test_release();
    test_text_record();
    return 0;
}
