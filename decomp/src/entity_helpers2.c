/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Random-orientation, geometry and record helpers, 0xb038c..0xbf9d4. */

/* 0xb038c, 92 bytes. Random orientation words at 0x41f32e (mod 3), 0x41f32c (mod 360) and 0x41f32a (mod 46). */
void randomize_orientation_b038c(void)
{
    EF_U16(0x0041f32eu) = (uint16_t)((int32_t)(guest_fn_9c960() & 0xffffu) % 3);
    EF_U16(0x0041f32cu) = (uint16_t)((int32_t)(guest_fn_9c960() & 0xffffu) % 0x168);
    EF_U16(0x0041f32au) = (uint16_t)((int32_t)(guest_fn_9c960() & 0xffffu) % 0x2e);
}

/* 0xb0448, 136 bytes. As 0xb038c but offset by the previous values (high words of the neighbouring dwords). */
void perturb_orientation_b0448(void)
{
    int32_t value;
    EF_U16(0x0041f32eu) = (uint16_t)((int32_t)(guest_fn_9c960() & 0xffffu) % 3);
    value = (int32_t)(guest_fn_9c960() & 0xffffu) % 0x2d + (EF_S32(0x0041f32au) >> 16);
    EF_U16(0x0041f32cu) = (uint16_t)(value % 0x168);
    value = (int32_t)(guest_fn_9c960() & 0xffffu) % 10 + (EF_S32(0x0041f328u) >> 16);
    EF_U16(0x0041f32au) = (uint16_t)(value % 0x2d);
}

/* 0xb1740, 146 bytes. Free the sub-allocations of an object. */
void free_object_b1740(ef_addr32 object)
{
    int16_t i;
    if (EF_U32(object + 0x20u) != 0)
        guest_fn_cc768(EF_U32(object + 0x20u));
    if (EF_U32(object + 0x51u) != 0)
        guest_fn_cc768(EF_U32(object + 0x51u));
    if (EF_U32(object + 0x69u) != 0) {
        for (i = 0; i < (int16_t)EF_U8(object + 1u); ++i) {
            uint32_t block = EF_U32(EF_U32(object + 0x69u) + (uint32_t)(int32_t)i * 0x16u);
            if (block != 0)
                guest_fn_cc768(block);
        }
        guest_fn_cc768(EF_U32(object + 0x69u));
    }
    if (EF_U32(object + 0x24u) != 0)
        guest_fn_cc768(EF_U32(object + 0x24u));
    if (EF_U32(object + 0x78u) != 0)
        guest_fn_cc768(EF_U32(object + 0x78u));
    if (EF_U8(0x00125846u) != 3 && EF_U32(object + 0x74u) != 0)
        guest_fn_cc768(EF_U32(object + 0x74u));
}

/* 0xb1b10, 123 bytes. */
void reset_view_tables_b1b10(void)
{
    guest_fn_b1814(0x00137960u, 0x00137964u);
    guest_fn_b1974(0x00137958u, 0x0013795cu);
    guest_fn_b18b8(0x00137988u, 0x0013798cu);
    guest_fn_b19e4(0x00137970u, 0x00137974u);
    guest_fn_b1ae4(0x00137978u, 0x0013797cu);
    guest_fn_b188c(0x00137980u, 0x00137984u);
    guest_fn_b1a10(0x001379bcu);
    guest_fn_b1a3c(0x001379c0u);
    guest_fn_f8f8();
    guest_fn_cbf00();
}

/* 0xb3988, 100 bytes. Approximate length from two components: max - max/32 - max/128 + 3/8 min + 3/128 min... */
int32_t approx_length_b3988(uint32_t first, uint32_t second)
{
    int32_t x = (int32_t)guest_fn_cf94a(first);
    int32_t y = (int32_t)guest_fn_cf94a(second);
    int32_t high = x > y ? x : y;
    int32_t low = x > y ? y : x;
    int32_t low_scaled = low + (low >> 1);
    return (high - (high >> 5) - (high >> 7)) + (low_scaled >> 2) + (low_scaled >> 6);
}

/* 0xb3b1c, 90 bytes. Quadrant-aware wrapper around 0xb3ae0 (angle of the vector EAX/EDX, 16-bit angle units). */
int32_t angle_of_b3b1c(int32_t x, int32_t y)
{
    if (x == 0) {
        if (y < 0)
            return 0x8000;
        return 0;
    }
    if (y == 0)
        return x < 0 ? 0xc000 : 0x4000;
    if (x < 0) {
        if (y < 0)
            return (int32_t)guest_fn_b3ae0(-x, -y) - 0x8000;
        return -(int32_t)guest_fn_b3ae0(-x, y);
    }
    if (y >= 0)
        return (int32_t)guest_fn_b3ae0(x, y);
    return 0x8000 - (int32_t)guest_fn_b3ae0(x, -y);
}

/* 0xbcb50, 111 bytes. */
uint32_t code_in_set_bcb50(uint32_t code)
{
    code &= 0xffu;
    if (code >= 0x3a && code <= 0x41)
        return 1;
    if (code == 1 || code == 2 || code == 3)
        return 1;
    return code >= 0x5f && code <= 0x64;
}

/* 0xbcec0, 87 bytes. */
uint32_t code_pair_matches_bcec0(uint8_t value, uint8_t code)
{
    if (code == 0x31)
        return value == 0 || value == 2;
    if (code == 0x49)
        return value == 1 || value == 2;
    if (code == 0x0b)
        return value == 2;
    return 0;
}

/* 0xbd490, 78 bytes. */
uint32_t slot_type_is_special_bd490(uint8_t type, uint8_t flag)
{
    if (type == 2 || type == 3 || type == 4 || type == 0x0c || type == 6 || type == 7 || type == 8 || type == 9)
        return 1;
    return EF_U8(0x0012583au) > 4 && type == 0 && flag == 0;
}

/* 0xbd5a8, 78 bytes. */
uint32_t slot_usable_bd5a8(ef_addr32 object, int16_t slot)
{
    uint32_t type = EF_U8(EF_U32(object + 6u) + (uint32_t)(int32_t)slot * 10u + 9u);
    if ((uint16_t)slot_type_is_special_bd490((uint8_t)type, EF_U8(object)) == 0)
        return 0;
    if ((int16_t)guest_fn_bd508(object) > (int16_t)guest_fn_bd554(object))
        return 0;
    return 1;
}

/* 0xbd688, 80 bytes. Mark all slots (10 bytes each) present and total their counts. */
void reset_slots_bd688(ef_addr32 object)
{
    int16_t i;
    EF_U8(object + 2u) = 0;
    EF_U8(object + 3u) = EF_U8(object + 5u);
    for (i = 0; i < (int16_t)EF_U8(object + 5u); ++i) {
        ef_addr32 slot = EF_U32(object + 6u) + (uint32_t)(int32_t)i * 10u;
        EF_U16(slot) = 1;
        EF_U8(object + 2u) = (uint8_t)(EF_U8(object + 2u) + EF_U8(slot + 3u));
        EF_U8(slot + 3u) = EF_U8(slot + 2u);
    }
}

/* 0xbd6d8, 78 bytes. Count the present slots and total their counts. */
void count_slots_bd6d8(ef_addr32 object)
{
    int16_t i;
    EF_U8(object + 2u) = 0;
    EF_U8(object + 3u) = 0;
    for (i = 0; i < (int16_t)EF_U8(object + 5u); ++i) {
        ef_addr32 slot = EF_U32(object + 6u) + (uint32_t)(int32_t)i * 10u;
        if (EF_U16(slot) != 0) {
            EF_U8(object + 3u) = (uint8_t)(EF_U8(object + 3u) + 1u);
            EF_U8(object + 2u) = (uint8_t)(EF_U8(object + 2u) + EF_U8(slot + 3u));
        }
    }
}

/* 0xbf980, 82 bytes. Do the records at +0x33 and +0x37 differ in the bytes at +0x20 or +0x21? */
uint32_t records_differ_bf980(ef_addr32 object)
{
    ef_addr32 first = EF_U32(object + 0x33u);
    ef_addr32 second = EF_U32(object + 0x37u);
    if (EF_U8(first + 0x20u) != 0 && EF_U8(first + 0x20u) != EF_U8(second + 0x20u))
        return 1;
    first = EF_U32(object + 0x33u);
    if (EF_U8(first + 0x21u) != 0 && EF_U8(first + 0x21u) != EF_U8(EF_U32(object + 0x37u) + 0x21u))
        return 1;
    return 0;
}

/* 0xbf9d4, 82 bytes. As 0xbf980 for the bytes at +0x23 and +0x24. */
uint32_t records_differ_bf9d4(ef_addr32 object)
{
    ef_addr32 first = EF_U32(object + 0x33u);
    ef_addr32 second = EF_U32(object + 0x37u);
    if (EF_U8(first + 0x23u) != 0 && EF_U8(first + 0x23u) != EF_U8(second + 0x23u))
        return 1;
    first = EF_U32(object + 0x33u);
    if (EF_U8(first + 0x24u) != 0 && EF_U8(first + 0x24u) != EF_U8(EF_U32(object + 0x37u) + 0x24u))
        return 1;
    return 0;
}
