/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

/* 0x7a480, size 0x21, four caller functions. The subtraction and
 * comparison are signed after 32-bit wrap. The original diagnostic is an
 * indirect call taking an immediate string address as a stack argument. */
void check_buffer_capacity(void)
{
    int32_t consumed = (int32_t)(uint32_t)(g_buffer_cursor - g_buffer_start);
    if (consumed > 0x4000) {
        report_buffer_capacity_exceeded(0x187b8c);
    }
}

/* 0x7a584, size 0x1b, three callers. Writes low AX before moving the
 * pointer; overflow is checked only after the write. */
void buffer_write_word(int16_t value)
{
    *(int16_t *)ef_guest_memory_ptr(g_buffer_cursor) = value;
    g_buffer_cursor += 2;
    check_buffer_capacity();
}

/* 0x7a5a0, size 0x1b, three callers. Advancing the pointer also happens
 * before reporting overflow, and the returned AX is sign-extended. */
int32_t buffer_read_word(void)
{
    int16_t value = *(const int16_t *)ef_guest_memory_ptr(g_buffer_cursor);
    g_buffer_cursor += 2;
    check_buffer_capacity();
    return value;
}
