/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"

/* Palette clearing and argument-forwarding wrappers. */

/* 0xd65e8, 34 bytes. Upload an all-zero (black) 0x300-byte palette. */
void set_black_palette_d65e8(void)
{
    uint8_t buf[0x300];

    fill_bytes_b3fb0((uint32_t)(uintptr_t)buf, 0, 0x300);
    guest_fn_d663c((uint32_t)(uintptr_t)buf);
}

/* 0xcc7d0, 23 bytes. Forward two stack arguments to cc90c with the 16-bit register argument sign-extended. */
void forward_word_args_cc7d0(int16_t a, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t s0, uint32_t s1)
{
    guest_fn_cc90c_x((int32_t)a, u1, u2, u3, s0, s1);
}

/* 0xf8600, 35 bytes. Forward four stack arguments to f9448 with the 16-bit register argument sign-extended. */
void forward_word_args_f8600(int16_t a, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3)
{
    guest_fn_f9448_x((int32_t)a, u1, u2, u3, s0, s1, s2, s3);
}
