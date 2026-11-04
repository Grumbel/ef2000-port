/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Parser record readers and a state reset. */

static void clear_dwords(uint32_t addr, uint32_t count)
{
    uint32_t i;

    for (i = 0; i < count; i++)
        EF_U32(addr + i * 4u) = 0;
}

/* 0xe76c8, 365 bytes. Clear the 0x4322xx..0x432axx working state and set the 0.6 default scale. */
void reset_working_state_e76c8(void)
{
    clear_dwords(0x004322a8u, 6);
    clear_dwords(0x004322c8u, 6);
    clear_dwords(0x00432360u, 12);
    clear_dwords(0x004327f8u, 2);
    clear_dwords(0x004327c0u, 2);
    EF_U32(0x004327e4u) = 0x3fe33333u;
    clear_dwords(0x00432218u, 2);
    clear_dwords(0x00432a50u, 6);
    clear_dwords(0x004329f8u, 6);
    clear_dwords(0x00432338u, 6);
    clear_dwords(0x00432398u, 6);
    EF_U8(0x0043399cu) = 0;
    EF_U32(0x004327e0u) = 0x33333333u;
}

/* Read four parser integers into the current 0x16-byte record. */
static ef_addr32 read_record_words(void)
{
    ef_addr32 rec;
    uint32_t i;

    for (i = 0; i < 4; i++) {
        int32_t v;

        guest_fn_f2cd0();
        v = guest_convert_parser_integer(0x0048592cu);
        rec = 0x00485400u + (uint32_t)(EF_S32(0x001616e4u) >> 16) * 0x16u;
        EF_U16(rec + i * 2u) = (uint16_t)v;
    }
    guest_fn_f2cd0();
    rec = 0x00485400u + (uint32_t)(EF_S32(0x001616e4u) >> 16) * 0x16u;
    guest_string_copy(rec + 8u, 0x0048592cu);
    guest_string_append(rec + 8u, 0x00191918u);
    return rec;
}

/* 0xf2e74, 327 bytes. Read a plain entry record and bump the record count. */
void read_plain_entry_f2e74(void)
{
    ef_addr32 rec = read_record_words();

    EF_U8(rec + 0x13u) = 0;
    EF_U16(0x001616e6u) = (uint16_t)(EF_U16(0x001616e6u) + 1);
}

/* 0xf2fbc, 399 bytes. Read an entry record, with a trailing value unless the plain flag is set. */
void read_entry_f2fbc(void)
{
    ef_addr32 rec;
    int32_t v;

    if (EF_U8(0x004859deu) != 0) {
        read_plain_entry_f2e74();
        return;
    }
    rec = read_record_words();
    EF_U8(rec + 0x13u) = 1;
    guest_fn_f2cd0();
    v = guest_convert_parser_integer(0x0048592cu);
    rec = 0x00485400u + (uint32_t)(EF_S32(0x001616e4u) >> 16) * 0x16u;
    EF_U16(rec + 0x14u) = (uint16_t)v;
    EF_U16(0x001616e6u) = (uint16_t)(EF_U16(0x001616e6u) + 1);
}
