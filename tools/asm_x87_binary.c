/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

/* Masked raw arithmetic only. No guest stack, memory or exception delivery. */
#include <stdint.h>
#include "platform.h"
#include "softfloat.h"

static void calculate(unsigned operation, const extFloat80_t *a,
                      const extFloat80_t *b, extFloat80_t *out)
{
    switch (operation) {
    case 0: extF80M_add(a, b, out); break;
    case 1: extF80M_sub(a, b, out); break;
    case 2: extF80M_mul(a, b, out); break;
    default: extF80M_div(a, b, out); break;
    }
}

unsigned ef_x87_binary(const uint32_t left[3], const uint32_t right[3],
                       unsigned operation, unsigned precision, unsigned rounding,
                       uint32_t out[3])
{
    static const uint_fast8_t modes[4] = {0, 2, 3, 1};
    static const uint_fast8_t precisions[4] = {32, 0, 64, 80};
    extFloat80_t operands[2], result, truncated;
    const uint32_t *raw;
    uint_fast8_t old_mode, old_flags, old_tininess, old_precision, flags;
    unsigned i, exceptions = 0;
    if (operation > 3 || precision > 3 || precision == 1 || rounding > 3)
        return 0x10000u;
    for (i = 0; i < 2; ++i) {
        raw = i ? right : left;
        operands[i].signif = ((uint64_t)raw[1] << 32) | raw[0];
        operands[i].signExp = (uint16_t)raw[2];
        if ((raw[2] & 0x7fffu) && !(raw[1] & 0x80000000u)) {
            out[0] = 0; out[1] = 0xc0000000u; out[2] = 0xffffu;
            return 1;
        }
        if (!(raw[2] & 0x7fffu) && operands[i].signif) {
            exceptions |= 2;
            if (raw[1] & 0x80000000u) ++operands[i].signExp;
        }
    }
    old_mode = softfloat_roundingMode;
    old_flags = softfloat_exceptionFlags;
    old_tininess = softfloat_detectTininess;
    old_precision = extF80_roundingPrecision;
    softfloat_roundingMode = modes[rounding];
    softfloat_exceptionFlags = 0;
    softfloat_detectTininess = softfloat_tininess_afterRounding;
    extF80_roundingPrecision = precisions[precision];
    calculate(operation, &operands[0], &operands[1], &result);
    flags = softfloat_exceptionFlags;
    softfloat_roundingMode = softfloat_round_minMag;
    softfloat_exceptionFlags = 0;
    calculate(operation, &operands[0], &operands[1], &truncated);
    softfloat_roundingMode = old_mode;
    softfloat_exceptionFlags = old_flags;
    softfloat_detectTininess = old_tininess;
    extF80_roundingPrecision = old_precision;
    /* Invalid/NaN and divide-by-zero take priority over denormal operands. */
    if ((flags & (softfloat_flag_invalid | softfloat_flag_infinite)) ||
        ((result.signExp & 0x7fffu) == 0x7fffu &&
         result.signif != ((uint64_t)1 << 63))) exceptions = 0;
    exceptions |= ((flags & 16u) >> 4) | ((flags & 8u) >> 1) |
                  ((flags & 4u) << 1) | ((flags & 2u) << 3) | ((flags & 1u) << 5);
    if ((flags & 1u) && ((result.signExp & 0x7fffu) > (truncated.signExp & 0x7fffu) ||
        ((result.signExp & 0x7fffu) == (truncated.signExp & 0x7fffu) &&
         result.signif > truncated.signif))) exceptions |= 0x200u;
    out[0] = (uint32_t)result.signif;
    out[1] = (uint32_t)(result.signif >> 32);
    out[2] = result.signExp;
    return exceptions;
}
