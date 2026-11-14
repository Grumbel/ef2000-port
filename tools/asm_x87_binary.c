/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

/* Raw arithmetic and exception-mask effects. No guest stack/memory/delivery. */
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

/* Return bit 17 when a pre-computation exception suppresses the destination.
   In that case out is untouched. Unmasked range exceptions instead return a
   rounded, exponent-wrapped result, as required for register arithmetic. */
unsigned ef_x87_binary_operand(const uint32_t left[3], const uint32_t right[3],
                               unsigned operation, unsigned control, unsigned denormal, uint32_t out[3])
{
    uint32_t result[3], scaled[2][3], rounded[3];
    uint64_t significand;
    int exponent[2], shift, result_exponent, common;
    unsigned flags, scaled_flags, precision = (control >> 8) & 3u;
    unsigned rounding = (control >> 10) & 3u, i;
    const uint32_t *raw;
    if (denormal > 1) return 0x10000u;
    flags = ef_x87_binary(left, right, operation, precision, rounding, result);
    if (flags & 0x10000u) return flags;
    /* Narrow subnormal operands are normalized by exact widening, but still
       signal DE unless invalid/NaN or divide-by-zero takes priority. */
    if (denormal && !(flags & 5u) &&
        !((result[2] & 0x7fffu) == 0x7fffu &&
          (result[0] || result[1] != 0x80000000u))) flags |= 2u;
    if ((flags & ~control & 1u)) return 0x20001u;
    if ((flags & ~control & 4u)) return 0x20004u;
    if ((flags & ~control & 2u)) return 0x20002u;
    /* Range adjustment applies only to finite nonzero results of finite
       operands. NaNs/invalid/divide-by-zero have already taken priority. */
    if (!(flags & 5u) && (control & 0x18u) != 0x18u &&
        (left[2] & 0x7fffu) != 0x7fffu && (right[2] & 0x7fffu) != 0x7fffu) {
        for (i = 0; i < 2; ++i) {
            raw = i ? right : left;
            significand = ((uint64_t)raw[1] << 32) | raw[0];
            exponent[i] = (int)(raw[2] & 0x7fffu);
            if (!exponent[i]) exponent[i] = 1;
            if (significand) {
                while (!(significand & ((uint64_t)1 << 63))) {
                    significand <<= 1; --exponent[i];
                }
            }
            scaled[i][0] = (uint32_t)significand;
            scaled[i][1] = (uint32_t)(significand >> 32);
            scaled[i][2] = raw[2] & 0x8000u;
        }
        if (operation < 2) {
            common = exponent[0] > exponent[1] ? exponent[0] : exponent[1];
            shift = 0x3fff - common;
        } else shift = 0;
        for (i = 0; i < 2; ++i) {
            if (!(scaled[i][0] | scaled[i][1])) continue;
            if (operation >= 2) scaled[i][2] |= 0x3fffu;
            else if (exponent[i] + shift > 0) scaled[i][2] |= (unsigned)(exponent[i] + shift);
            else {
                /* An operand this far below the other operand contributes
                   only a sticky bit. Preserve its nonzero sign/direction. */
                scaled[i][0] = 1; scaled[i][1] = 0;
            }
        }
        scaled_flags = ef_x87_binary(scaled[0], scaled[1], operation, precision, rounding, rounded);
        if (rounded[0] | rounded[1]) {
            result_exponent = (int)(rounded[2] & 0x7fffu);
            if (operation < 2) common = -shift;
            else if (operation == 2) common = exponent[0] + exponent[1] - 0x7ffe;
            else common = exponent[0] - exponent[1];
            result_exponent += common;
            if (!(control & 8u) && (flags & 8u)) {
                rounded[2] = (rounded[2] & 0x8000u) | (unsigned)(result_exponent - 24576);
                flags = (flags & 2u) | (scaled_flags & 0x220u) | 8u;
                for (i = 0; i < 3; ++i) result[i] = rounded[i];
            } else if (!(control & 0x10u) && result_exponent <= 0) {
                rounded[2] = (rounded[2] & 0x8000u) | (unsigned)(result_exponent + 24576);
                flags = (flags & 2u) | (scaled_flags & 0x220u) | 0x10u;
                for (i = 0; i < 3; ++i) result[i] = rounded[i];
            }
        }
    }
    for (i = 0; i < 3; ++i) out[i] = result[i];
    return flags;
}

unsigned ef_x87_binary_control(const uint32_t left[3], const uint32_t right[3],
                               unsigned operation, unsigned control, uint32_t out[3])
{
    return ef_x87_binary_operand(left, right, operation, control, 0, out);
}
