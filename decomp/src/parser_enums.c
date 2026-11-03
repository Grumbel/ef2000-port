/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_guest_memory.h"

static ef_addr32 parser_read32(ef_addr32 address)
{
    const uint8_t *p = (const uint8_t *)ef_guest_memory_ptr(address);
    return (ef_addr32)p[0] | ((ef_addr32)p[1] << 8) |
           ((ef_addr32)p[2] << 16) | ((ef_addr32)p[3] << 24);
}

/* Every one of the seven original functions implements this exact
 * zero-based linear search. A miss calls the indirect guest diagnostic
 * with its own format address before returning zero. */
static uint32_t read_named_enum(ef_addr32 table, uint32_t entries,
                                ef_addr32 error_format)
{
    read_parser_token();
    for (uint32_t i = 0; i < entries; ++i) {
        ef_addr32 candidate = parser_read32(table + i * 4u);
        if (guest_compare_parser_token(0x00197abe, candidate) == 0) {
            return i;
        }
    }
    guest_report_parser_enum_error(error_format, 0x00197abe,
                                   parser_read32(0x00121eac));
    return 0;
}

/* 0xea0c, 234 bytes. Original guards (0x180c84 and 0x180c94) yield
 * zero, while two three-entry families each return one through three. */
uint32_t read_parser_enum_two_families(void)
{
    read_parser_token();
    if (guest_compare_parser_token(0x00197abe, 0x00180c84) == 0) { return 0; }
    if (guest_compare_parser_token(0x00197abe, 0x00180c88) == 0) { return 1; }
    if (guest_compare_parser_token(0x00197abe, 0x00180c8c) == 0) { return 2; }
    if (guest_compare_parser_token(0x00197abe, 0x00180c90) == 0) { return 3; }
    if (guest_compare_parser_token(0x00197abe, 0x00180c94) == 0) { return 0; }
    if (guest_compare_parser_token(0x00197abe, 0x00180c9c) == 0) { return 1; }
    if (guest_compare_parser_token(0x00197abe, 0x00180ca4) == 0) { return 2; }
    if (guest_compare_parser_token(0x00197abe, 0x00180cac) == 0) { return 3; }
    guest_report_parser_enum_error(0x00180cb4, 0x00197abe,
                                   parser_read32(0x00121eac));
    return 0;
}

/* 0xeaf8, 79 bytes; byte offset 0x78 is 30 pointers. */
uint32_t read_parser_enum_30_names(void)
{
    return read_named_enum(0x00132e6c, 30, 0x00180cf0);
}

/* 0xeb48, 82 bytes; byte offset 0x9c is 39 pointers. */
uint32_t read_parser_enum_39_names(void)
{
    return read_named_enum(0x00132d34, 39, 0x00180d2c);
}

/* 0xeb9c, 79 bytes; byte offset 0x48 is 18 pointers. */
uint32_t read_parser_enum_18_names(void)
{
    return read_named_enum(0x001333f8, 18, 0x00180d68);
}

/* 0xebec, 79 bytes; byte offset 0x6c is 27 pointers. */
uint32_t read_parser_enum_27_names(void)
{
    return read_named_enum(0x00133488, 27, 0x00180da0);
}

/* 0xec8c, 82 bytes; byte offset 0x1a8 is 106 pointers. */
uint32_t read_parser_enum_106_names(void)
{
    return read_named_enum(0x00132fb4, 106, 0x00180e18);
}

/* 0xece0, 79 bytes; byte offset 0x28 is 10 pointers. */
uint32_t read_parser_enum_10_names(void)
{
    return read_named_enum(0x0013336c, 10, 0x00180e54);
}

/* 0xed30, 79 bytes; byte offset 0x4c is 19 pointers. */
uint32_t read_parser_enum_19_names(void)
{
    return read_named_enum(0x00132c2c, 19, 0x00180e8c);
}
