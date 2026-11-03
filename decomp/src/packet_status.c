/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"

static void pack16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void pack32(uint8_t *p, uint32_t v)
{
    pack16(p, (uint16_t)v);
    pack16(p + 2, (uint16_t)(v >> 16));
}

/* 0x548a4, 60 bytes. EAX AX, EDX DX, EBX BL, ECX dword, then two
 * caller-stack dwords in that order. All eighteen packet bytes are defined. */
void send_packet_15(int16_t ax, int16_t dx, uint8_t tag, uint32_t value,
                    uint32_t stack_a, uint32_t stack_b)
{
    uint8_t bytes[18];
    bytes[0] = 0x15;
    pack16(bytes + 1, (uint16_t)ax);
    pack16(bytes + 3, (uint16_t)dx);
    bytes[5] = tag;
    pack32(bytes + 6, value);
    pack32(bytes + 10, stack_a);
    pack32(bytes + 14, stack_b);
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x55b44, 58 bytes. The stack argument contributes its low word; the
 * final byte is the global grid marker. Original callee cleans four bytes. */
void send_packet_41(int16_t dx, int16_t bx, uint8_t code, uint16_t stack_index)
{
    uint8_t bytes[9];
    bytes[0] = 0x41;
    pack16(bytes + 1, (uint16_t)dx);
    pack16(bytes + 3, (uint16_t)bx);
    bytes[5] = code;
    pack16(bytes + 6, stack_index);
    bytes[8] = g_world_grid_marker;
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x56974, 61 bytes. Three signed/raw AX/BX/DX words, one word and one
 * byte of game state. Note AX,BX,DX packet order (different from ABI). */
void send_packet_2b(int16_t ax, int16_t dx, int16_t bx)
{
    uint8_t bytes[10];
    bytes[0] = 0x2b;
    pack16(bytes + 1, (uint16_t)ax);
    pack16(bytes + 3, (uint16_t)bx);
    pack16(bytes + 5, (uint16_t)dx);
    pack16(bytes + 7, g_packet_2b_word);
    bytes[9] = g_packet_2b_byte;
    emit_command_packet(bytes, sizeof(bytes));
}

/* 0x82790, 35 bytes. a4342 is a cdecl formatter: stack arguments are
 * destination, format pointer, then input flag B followed by flag A. */
void write_ui_status_flags_a(ef_addr32 destination)
{
    format_ui_status_pair(destination, 0x00188b58,
                          g_mode_input_enabled_a, g_ui_status_flag_a);
}

/* 0x827d0, 35 bytes. Same format interface with two other global flags. */
void write_ui_status_flags_b(ef_addr32 destination)
{
    format_ui_status_pair(destination, 0x00188b74,
                          g_ui_status_flag_c, g_ui_status_flag_b);
}

/* 0x82bd4, 27 bytes. The source is float32, promoted by FLD/FSTP to a
 * double before passing through the guest formatting function. */
void write_ui_status_float(ef_addr32 destination)
{
    format_ui_status_float(destination, 0x00188bfc, (double)g_ui_status_scalar);
}

/* 0x59484, 30 bytes. Mode byte 1 maps to 0; all others map to 1.
 * The input item is a full 32-bit guest pointer. */
void draw_tactical_item_mode(ef_addr32 item)
{
    draw_tactical_mode_item(g_tactical_display_mode == 1 ? 0u : 1u, item);
}

/* 0x5331c, 17 bytes. The indirect callback runs before sampling the byte. */
void refresh_tactical_indicator(void)
{
    read_tactical_indicator_callback();
    g_tactical_indicator_latch = g_tactical_indicator_source;
}
