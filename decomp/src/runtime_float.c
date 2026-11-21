/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

#define TEXT_INF 0x00195a88u /* "inf" */
#define TEXT_NAN 0x00195a8cu /* "nan" */

#define DOUBLE_EXPONENT  0x7ff00000u
#define DOUBLE_SIGN      0x80000000u
#define DOUBLE_MANTISSA  0x000fffffu

/* 0x1204dc, 132 bytes. printf helper: if the double LO:HI (two stack
 * dwords) is an infinity or NaN, write an optional '-' and "inf" or "nan"
 * (four bytes, with the terminator) to OUT and return 1; finite values
 * return 0 and write nothing. Callee pops its 12 bytes of arguments. */
uint32_t format_nonfinite_double(uint32_t lo, uint32_t hi, ef_addr32 out)
{
	if ((hi & DOUBLE_EXPONENT) != DOUBLE_EXPONENT)
		return 0u;
	if ((hi & DOUBLE_SIGN) != 0u) {
		EF_U8(out) = '-';
		out++;
	}
	if ((lo | (hi & DOUBLE_MANTISSA)) == 0u)
		EF_U32(out) = EF_U32(TEXT_INF);
	else
		EF_U32(out) = EF_U32(TEXT_NAN);
	return 1u;
}

#define EXTENDED_BIAS_SHIFT 0x3c00u /* 16383 - 1023 */
#define FPU_INVALID         0x8101u /* AL: invalid-operation flag, AH: signal code */
#define FPU_RAISE           0x000ee43cu /* set status, signal unless masked; EBP = context */

/* 0xeffb4, 128 bytes. Soft-float runtime: widen the double HI:LO to the
 * 80-bit extended format at OUT (explicit integer bit, rebiased
 * exponent, normalized denormals; zero loses its sign). A signalling NaN
 * raises invalid operation through 0xee43c on the emulator context in
 * EBP and becomes quiet. */
/* The caller's EBP: the entry has a standard frame (pragma "frame"), so
 * it is the saved value at [EBP]. */
static uint32_t soft_float_context(void);
#pragma aux soft_float_context = "mov eax,[ebp]" value [eax] modify exact [eax]

void double_to_extended_effb4(uint32_t lo, uint32_t hi, ef_addr32 out)
{
	ef_addr32 context = soft_float_context();
	uint32_t mant_lo = lo << 11;
	uint32_t mant_hi = (hi << 11) | (lo >> 21);
	uint16_t exponent = (uint16_t)((hi >> 20) & 0x7ffu);
	uint16_t sign = (hi & 0x80000000u) != 0u ? 0x8000u : 0u;

	if (exponent == 0u) {
		if ((mant_hi | mant_lo) == 0u) {
			sign = 0u;
		} else {
			exponent = EXTENDED_BIAS_SHIFT + 1u;
			if (mant_hi == 0u) {
				mant_hi = mant_lo;
				mant_lo = 0u;
				exponent -= 32u;
			}
			while ((mant_hi & 0x80000000u) == 0u) {
				mant_hi = (mant_hi << 1) | (mant_lo >> 31);
				mant_lo <<= 1;
				exponent--;
			}
		}
	} else if (exponent == 0x7ffu) {
		exponent = 0x7fffu;
		if ((mant_hi & 0x7fffffffu) != 0u || mant_lo != 0u) {
			guest_callptr_ebp_x(FPU_RAISE, FPU_INVALID, context);
			mant_hi |= 0x40000000u;
		}
		mant_hi |= 0x80000000u;
	} else {
		exponent += EXTENDED_BIAS_SHIFT;
		mant_hi |= 0x80000000u;
	}
	EF_U32(out) = mant_lo;
	EF_U32(out + 4u) = mant_hi;
	EF_U16(out + 8u) = (uint16_t)(sign | exponent);
}
