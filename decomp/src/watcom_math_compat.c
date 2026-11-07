/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_types.h"

/* OpenWatcom's sqrt87 helper passes a double and error code on the stack and
 * expects the callee to pop 12 bytes and return a double in ST0. Preserve
 * that ABI for now; error reporting is intentionally deferred from M1. */
double __math87_err(double value, uint32_t code);
#pragma aux __math87_err parm []

double __math87_err(double value, uint32_t code)
{
    (void)code;
    return value;
}

double __log_matherr(double value, uint32_t code);
#pragma aux __log_matherr parm []

double __log_matherr(double value, uint32_t code)
{
    (void)code;
    return value;
}
