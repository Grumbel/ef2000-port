/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_guest_memory.h"

static uint8_t read8(ef_addr32 a)
{
    return *(const uint8_t *)ef_guest_memory_ptr(a);
}

static uint16_t read16(ef_addr32 a)
{
    return (uint16_t)(read8(a) | (uint16_t)(read8(a + 1u) << 8));
}

static uint32_t read32(ef_addr32 a)
{
    return (uint32_t)read16(a) | (uint32_t)read16(a + 2u) << 16;
}

static void write16(ef_addr32 a, uint16_t v)
{
    *(uint8_t *)ef_guest_memory_ptr(a) = (uint8_t)v;
    *(uint8_t *)ef_guest_memory_ptr(a + 1u) = (uint8_t)(v >> 8);
}

/* The six wrappers below call the cdecl formatter 0xa4342 with
 * destination (EAX), a format string in the data segment, and one value. */

/* 0x82720, 26 bytes. Byte at 0x1fcb30, format 0x188b24. */
void write_status_byte_1fcb30(ef_addr32 destination)
{
    format_ui_status_value(destination, 0x00188b24u, read8(0x001fcb30u));
}

/* 0x8273c, 26 bytes. Byte at 0x1a5c0d, format 0x188b34. */
void write_status_byte_1a5c0d(ef_addr32 destination)
{
    format_ui_status_value(destination, 0x00188b34u, read8(0x001a5c0du));
}

/* 0x82758, 27 bytes. Signed high word of dword 0x12b364, format 0x188b40. */
void write_status_high_word(ef_addr32 destination)
{
    format_ui_status_value(destination, 0x00188b40u,
                           (uint32_t)((int32_t)read32(0x0012b364u) >> 16));
}

/* 0x82774, 26 bytes. Byte at 0x15b7e4, format 0x188b4c. */
void write_status_byte_15b7e4(ef_addr32 destination)
{
    format_ui_status_value(destination, 0x00188b4cu, read8(0x0015b7e4u));
}

/* 0x827b4, 25 bytes. The same destination pointer is first passed to the
 * guest value routine 0x810ec, whose EAX result is formatted. */
void write_status_queried_value(ef_addr32 destination)
{
    uint32_t value = guest_query_status_value(destination);
    format_ui_status_value(destination, 0x00188b68u, value);
}

/* 0x827f4, 24 bytes. Dword at 0x122dc4, format 0x188b84. */
void write_status_dword_122dc4(ef_addr32 destination)
{
    format_ui_status_value(destination, 0x00188b84u, read32(0x00122dc4u));
}

/* 0x82bf0, 26 bytes. Byte at 0x15bdd0, format 0x188c10. */
void write_status_byte_15bdd0(ef_addr32 destination)
{
    format_ui_status_value(destination, 0x00188c10u, read8(0x0015bdd0u));
}

/* 0x845a4, 25 bytes. Word 0x12b4a6 = 2 and byte 0x1fcc43 = 1; returns AL=0
 * while AH is left as 1. */
uint8_t set_pending_mode_two(void)
{
    write16(0x0012b4a6u, 2);
    *(uint8_t *)ef_guest_memory_ptr(0x001fcc43u) = 1;
    return 0;
}

/* 0x81794, 17 bytes. Store the halfword from 0xde864(0x1889d4). */
void load_halfword_1fcaec(void)
{
    write16(0x001fcaecu, guest_load_halfword(0x001889d4u));
}

/* 0x7a434, 30 bytes. Release the block at pointer 0x12afc4 if set and clear it. */
void release_block_12afc4(void)
{
    ef_addr32 block = read32(0x0012afc4u);
    if (block != 0)
        guest_release_block(block);
    write16(0x0012afc4u, 0);
    write16(0x0012afc6u, 0);
}
