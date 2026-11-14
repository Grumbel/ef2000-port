/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

/* Generic x87 extended-to-real conversion adapter. No guest memory effects.
   Uses the separately pinned SoftFloat 3e 8086 specialization. */
#include <stdint.h>
#include "platform.h"
#include "softfloat.h"

unsigned ef_x87_narrow(const uint32_t raw[3], unsigned width,
                       unsigned rounding, uint32_t out[2])
{
    static const uint_fast8_t modes[4] = {0, 2, 3, 1};
    extFloat80_t input;
    uint_fast8_t previous_mode, previous_flags, previous_tininess, flags;
    uint64_t bits, truncated, magnitude_mask;
    unsigned result;
    if ((width != 32 && width != 64) || rounding > 3) return 0x10000u;
    input.signif = ((uint64_t)raw[1] << 32) | raw[0];
    input.signExp = (uint16_t)raw[2];
    /* Unsupported extended encodings generate the x87 indefinite value. */
    if ((input.signExp & 0x7fffu) && !(raw[1] & 0x80000000u)) {
        out[0] = width == 32 ? 0xffc00000u : 0;
        out[1] = width == 32 ? 0 : 0xfff80000u;
        return 1;
    }
    /* Pseudo-denormals denote the corresponding exponent-one value. */
    if (!(input.signExp & 0x7fffu) && (raw[1] & 0x80000000u)) ++input.signExp;
    previous_mode = softfloat_roundingMode;
    previous_flags = softfloat_exceptionFlags;
    previous_tininess = softfloat_detectTininess;
    softfloat_roundingMode = modes[rounding];
    softfloat_exceptionFlags = 0;
    softfloat_detectTininess = softfloat_tininess_afterRounding;
    if (width == 32) bits = extF80M_to_f32(&input).v;
    else bits = extF80M_to_f64(&input).v;
    flags = softfloat_exceptionFlags;
    /* C1 records an increase in magnitude, including directed overflow.
       A second exact-source conversion toward zero supplies that reference. */
    softfloat_roundingMode = softfloat_round_minMag;
    softfloat_exceptionFlags = 0;
    if (width == 32) truncated = extF80M_to_f32(&input).v;
    else truncated = extF80M_to_f64(&input).v;
    softfloat_roundingMode = previous_mode;
    softfloat_exceptionFlags = previous_flags;
    softfloat_detectTininess = previous_tininess;
    result = ((flags & 16u) >> 4) | ((flags & 8u) >> 1) |
             ((flags & 4u) << 1) | ((flags & 2u) << 3) | ((flags & 1u) << 5);
    magnitude_mask = width == 32 ? (uint64_t)0x7fffffffu :
                     (((uint64_t)1 << 63) - 1u);
    if ((flags & 1u) && (bits & magnitude_mask) > (truncated & magnitude_mask)) result |= 0x200u;
    out[0] = (uint32_t)bits;
    out[1] = width == 32 ? 0 : (uint32_t)(bits >> 32);
    return result;
}
