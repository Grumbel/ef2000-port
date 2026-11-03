/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t memory[0x230000];
static unsigned token_reads, comparisons, errors;
static ef_addr32 expected_error_format;
static const char *incoming_token;

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(memory));
    return memory + address;
}

static void write32(ef_addr32 address, ef_addr32 value)
{
    for (unsigned i = 0; i < 4; ++i) { memory[address + i] = (uint8_t)(value >> (8 * i)); }
}

uint32_t read_parser_token(void)
{
    ++token_reads;
    strcpy((char *)(memory + 0x197abe), incoming_token);
    return (uint32_t)strlen(incoming_token);
}

int32_t guest_compare_parser_token(ef_addr32 token, ef_addr32 candidate)
{
    assert(token == 0x197abe);
    ++comparisons;
    return strcmp((const char *)memory + token, (const char *)memory + candidate);
}

void guest_report_parser_enum_error(ef_addr32 format, ef_addr32 token, uint32_t context)
{
    assert(format == expected_error_format);
    assert(token == 0x197abe);
    assert(context == 0xd00dfeedu);
    ++errors;
}

typedef uint32_t (*enum_fn)(void);

typedef struct {
    enum_fn function;
    ef_addr32 table;
    unsigned entries;
    ef_addr32 error_format;
} enum_case;

static const enum_case cases[] = {
    {read_parser_enum_30_names, 0x132e6c, 30, 0x180cf0},
    {read_parser_enum_39_names, 0x132d34, 39, 0x180d2c},
    {read_parser_enum_18_names, 0x1333f8, 18, 0x180d68},
    {read_parser_enum_27_names, 0x133488, 27, 0x180da0},
    {read_parser_enum_106_names, 0x132fb4, 106, 0x180e18},
    {read_parser_enum_10_names, 0x13336c, 10, 0x180e54},
    {read_parser_enum_19_names, 0x132c2c, 19, 0x180e8c}
};

static void reset(void)
{
    memset(memory, 0, sizeof(memory));
    memcpy(memory + 0x1c0000, "WRONG", 6);
    memcpy(memory + 0x1c0010, "MATCH", 6);
    incoming_token = "MATCH";
    token_reads = comparisons = errors = 0;
    write32(0x121eac, 0xd00dfeedu);
}

static void test_tables(void)
{
    for (unsigned c = 0; c < sizeof(cases) / sizeof(cases[0]); ++c) {
        const enum_case *v = &cases[c];
        reset();
        expected_error_format = v->error_format;
        for (unsigned i = 0; i < v->entries; ++i) {
            write32(v->table + 4u * i, 0x1c0000);
        }
        write32(v->table, 0x1c0010);
        assert(v->function() == 0);
        assert(token_reads == 1 && comparisons == 1 && errors == 0);

        token_reads = comparisons = errors = 0;
        write32(v->table, 0x1c0000);
        write32(v->table + 4u * (v->entries - 1u), 0x1c0010);
        assert(v->function() == v->entries - 1u);
        assert(token_reads == 1 && comparisons == v->entries && errors == 0);

        incoming_token = "NOT_IN_TABLE";
        token_reads = comparisons = errors = 0;
        assert(v->function() == 0);
        assert(token_reads == 1 && comparisons == v->entries && errors == 1);
    }
}

static void test_two_families(void)
{
    static const ef_addr32 names[] = {
        0x180c84, 0x180c88, 0x180c8c, 0x180c90,
        0x180c94, 0x180c9c, 0x180ca4, 0x180cac
    };
    reset();
    expected_error_format = 0x180cb4;
    for (unsigned i = 0; i < 8; ++i) {
        char text[12];
        /* Eight distinct (asset-free) names at original guest offsets. */
        text[0] = 'A';
        text[1] = (char)('0' + i);
        text[2] = 0;
        memcpy(memory + names[i], text, 3);
    }
    for (unsigned i = 0; i < 8; ++i) {
        incoming_token = (const char *)memory + names[i];
        token_reads = comparisons = errors = 0;
        assert(read_parser_enum_two_families() == i % 4);
        assert(token_reads == 1 && comparisons == i + 1u && errors == 0);
    }
    incoming_token = "UNKNOWN";
    token_reads = comparisons = errors = 0;
    assert(read_parser_enum_two_families() == 0);
    assert(token_reads == 1 && comparisons == 8 && errors == 1);
}

int main(void)
{
    test_tables();
    test_two_families();
    return 0;
}
