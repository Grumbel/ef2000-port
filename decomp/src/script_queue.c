/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* Inline the arithmetic of implicit-register1600c4, avoiding its old
 * pointer-parameter draft ABI. All additions and squares wrap at32 bits. */
static uint32_t rounded_component_square(uint32_t value)
{
    uint32_t sign = (uint32_t)((int32_t)value >> 31);
    value = (value ^ sign) - sign;
    value = (uint32_t)((int32_t)(value + 0x8000u) >> 16);
    return value * value;
}

/* 0x160329, 200 bytes. No register inputs; saves ESI/EDI/EBP.
 * Sole direct caller a0a1d consumes no scratch-register result. */
void append_rotated_script_entry_160329(void)
{
    ef_addr32 world = EF_U32(0x001600b0u);
    uint32_t x = EF_U32(world);
    uint32_t y = EF_U32(world + 4u);
    uint32_t z = EF_U32(world + 8u);
    uint32_t repeat = EF_U32(0x001600b8u);
    uint8_t value = EF_U8(EF_U32(0x001600acu));
    uint32_t slot = EF_U16(0x00146e52u);
    uint32_t dword_offset = slot * 4u;
    uint32_t word_offset = slot * 2u;
    uint32_t length, sign, signed_length, bias, magnitude;
    ef_addr32 rotation;
    uint16_t rx, ry, rz;

    EF_U8(0x00149e58u + slot) = (uint8_t)repeat;
    EF_U8(0x00149658u + slot) = value;
    EF_U8(0x00148e58u + slot) = 0x38u;
    EF_U8(0x0014a658u + slot) = 0u;
    EF_U32(0x00150e58u + dword_offset) = x;
    EF_U32(0x00152e58u + dword_offset) = y;
    EF_U32(0x00154e58u + dword_offset) = z;
    length = integer_sqrt(rounded_component_square(x)
        + rounded_component_square(y) + rounded_component_square(z));
    sign = (uint32_t)((int32_t)z >> 31);
    signed_length = (length ^ sign) - sign;
    /* Reload script globals after the preceding possibly aliasing stores. */
    EF_U32(0x00156e58u + dword_offset) = EF_U32(0x001600acu);
    EF_U16(0x0014ae58u + word_offset) = (uint16_t)EF_U32(0x001600a8u);
    EF_U16(0x0014fe58u + word_offset) = (uint16_t)length;
    rotation = EF_U32(0x001600b4u);
    rx = EF_U16(rotation);
    ry = EF_U16(rotation + 2u);
    rz = EF_U16(rotation + 4u);
    EF_U16(0x0014be58u + word_offset) = rx;
    EF_U16(0x0014ce58u + word_offset) = ry;
    EF_U16(0x0014de58u + word_offset) = rz;
    EF_U16(0x0014ee58u + word_offset) = (uint16_t)EF_U32(0x001600bcu);
    bias = EF_U32(0x001600c0u);
    sign = (uint32_t)((int32_t)bias >> 31);
    magnitude = (bias ^ sign) - sign;
    /* SAR retains the negative INT32_MIN absolute-value overflow case. */
    guest_fn_c41b0_x(signed_length + bias
        + (uint32_t)((int32_t)magnitude >> 2));
}
