/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* DOS interrupt wrappers, allocator calls and parser enum lookups, 0x880..0xf0ec. */

/* Call the original allocator stored at 0x12dd9c. */
static uint32_t call_allocator_12dd9c(uint32_t bytes, uint32_t flags)
{
    return ((uint32_t (*)(uint32_t, uint32_t))EF_U32(0x0012dd9cu))(bytes, flags);
}

/* 0x880, 53 bytes. Issue DOS service 0x350b and return the result register. */
uint32_t dos_call_350b_880(void)
{
    EF_U32(0x00196148u) = 0x350b;
    guest_fn_2b0(0x00196148u);
    if (EF_U32(0x00196160u) != 0)
        ui_report_missing_record(0x0017f128u);
    return EF_U32(0x00196148u);
}

/* 0xa9c, 60 bytes. Issue DOS service 0x2505 with two register values. */
void dos_call_2505_a9c(uint32_t first, uint32_t second)
{
    EF_U32(0x0019614cu) = second;
    EF_U32(0x00196150u) = first;
    EF_U32(0x00196148u) = 0x2505;
    guest_fn_2b0(0x00196148u);
    if (EF_U32(0x00196160u) != 0)
        ui_report_missing_record(0x0017f140u);
}

/* 0xc74, 55 bytes. Issue DOS service 0x3502 with a sign-extended 16-bit argument. */
void dos_call_3502_c74(int16_t value)
{
    EF_U32(0x00196148u) = 0x3502;
    EF_U32(0x0019614cu) = (uint32_t)(int32_t)value;
    guest_fn_2b0(0x00196148u);
    if (EF_U32(0x00196160u) != 0)
        ui_report_missing_record(0x0017f188u);
}

/* 0xcac, 38 bytes. Open a file through 0x9c2d3 and abort when it fails. */
uint32_t open_checked_cac(uint32_t name, uint32_t mode)
{
    uint32_t handle = guest_fn_9c2d3(name, mode);

    if (handle == 0xffffffffu)
        ui_report_missing_record(0x0017f1a4u);
    return handle;
}

/* 0x6df0, 38 bytes. Allocate and zero five bytes per unit. */
void alloc_zero_5x_6df0(uint8_t count)
{
    uint32_t bytes = (uint32_t)count * 5u;
    uint32_t block = call_allocator_12dd9c(bytes, 0);

    EF_U32(0x00197260u) = block;
    guest_fn_b3fb0(block, 0, bytes);
}

/* 0x7780, 40 bytes. Allocate and zero a block of the product of both sizes. */
uint32_t alloc_zero_product_7780(int32_t count, int32_t size)
{
    int32_t bytes = count * size;
    uint32_t block;

    if (bytes == 0)
        return 0;
    block = call_allocator_12dd9c((uint32_t)bytes, 0);
    guest_fn_b3fb0(block, 0, (uint32_t)bytes);
    return block;
}

/* 0xec3c, 79 bytes. Parse a token and look it up in the 13-entry table at 0x133304. */
uint32_t parse_enum_ec3c(void)
{
    uint32_t i;

    read_parser_token();
    for (i = 0; i < 13; i++) {
        if (guest_fn_a4380(0x00197abeu, EF_U32(0x00133304u + i * 4u)) == 0)
            return i;
    }
    guest_report_parser_enum_error(0x00180ddcu, 0x00197abeu, EF_U32(0x00121eacu));
    return 0;
}

/* 0xeea0, 92 bytes. Skip three tokens and look the fourth up in the 37-entry table at 0x1335a0. */
uint32_t parse_enum_eea0(void)
{
    int16_t i;

    read_parser_line();
    read_parser_token();
    read_parser_token();
    read_parser_token();
    for (i = 0; i < 0x25; i++) {
        if (guest_fn_a4380(0x00197abeu, EF_U32(0x001335a0u + (uint32_t)((int32_t)i * 4))) == 0)
            return (uint32_t)(int32_t)i;
    }
    guest_report_parser_enum_error(0x00180ec4u, 0x00197abeu, EF_U32(0x00121eacu));
    return 0;
}

/* 0xef98, 79 bytes. Parse a token and look it up in the 3-entry table at 0x132cc4. */
uint32_t parse_enum_ef98(void)
{
    uint32_t i;

    read_parser_token();
    for (i = 0; i < 3; i++) {
        if (guest_fn_a4380(0x00197abeu, EF_U32(0x00132cc4u + i * 4u)) == 0)
            return i;
    }
    guest_report_parser_enum_error(0x00180efcu, 0x00197abeu, EF_U32(0x00121eacu));
    return 0;
}

/* 0xf0ec, 79 bytes. Parse a token and look it up in the 6-entry table at 0x132d1c. */
uint32_t parse_enum_f0ec(void)
{
    uint32_t i;

    read_parser_token();
    for (i = 0; i < 6; i++) {
        if (guest_fn_a4380(0x00197abeu, EF_U32(0x00132d1cu + i * 4u)) == 0)
            return i;
    }
    guest_report_parser_enum_error(0x00180f34u, 0x00197abeu, EF_U32(0x00121eacu));
    return 0;
}

/* 0xf038, 83 bytes. Read a count and that many 17-byte parser records. */
uint32_t read_record_list_f038(ef_addr32 list)
{
    int16_t count;
    int16_t i = 0;

    read_parser_line();
    read_parser_token();
    count = (int16_t)guest_fn_e95c();
    EF_U16(list) = (uint16_t)count;
    EF_U32(list + 2u) = guest_allocate_parser_text((uint32_t)((int32_t)count * 17), 0);
    if ((int16_t)EF_U16(list) > 0) {
        do {
            ef_addr32 entry = EF_U32(list + 2u) + (uint32_t)((int32_t)i * 0x11);

            i++;
            guest_fn_efe8(entry);
        } while (i < (int16_t)EF_U16(list));
    }
    return 1;
}

/* 0xf08c, 95 bytes. Read a count and that many 6-byte groups of 17-byte parser records. */
uint32_t read_group_list_f08c(ef_addr32 list)
{
    int16_t count;
    int16_t i = 0;

    read_parser_line();
    read_parser_token();
    read_parser_token();
    count = (int16_t)guest_fn_e95c();
    EF_U16(list + 1u) = (uint16_t)count;
    EF_U32(list + 3u) = guest_allocate_parser_text((uint32_t)((int32_t)count * 6), 0);
    if ((int16_t)EF_U16(list + 1u) > 0) {
        do {
            ef_addr32 entry = EF_U32(list + 3u) + (uint32_t)((int32_t)i * 6);

            i++;
            read_record_list_f038(entry);
        } while (i < (int16_t)EF_U16(list + 1u));
    }
    return 1;
}
