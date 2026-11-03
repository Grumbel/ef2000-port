/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Device-open, string compare, x87 helper and list helpers, 0xe0b88..0xfc6c8. */

/* 0xe0b88, 65 bytes. Open the device selected by value and report success. */
uint32_t open_device_e0b88(ef_addr32 value)
{
    guest_fn_e10e0();
    guest_fn_e10a0(value);
    guest_fn_e10a0(0x001905f4u);
    EF_U8(0x0015f694u) = 1;
    EF_U32(0x0015f690u) = value;
    guest_fn_e1110(0x1e);
    return (guest_fn_e0b10(0x001905f8u) & 0xffu) != 0;
}

/* 0xe5830, 62 bytes. */
void refresh_slot_e5830(ef_addr32 object)
{
    uint8_t flag = EF_U8(0x0013725cu) != 2u;

    guest_fn_e59cc(EF_U32(0x0041edfcu + (uint32_t)EF_U8(object + 0x50u) * 4u), object);
    guest_fn_e52a4(EF_U8(object + 0x50u), flag);
}

/* 0xe5960, 58 bytes. Compute the rectangle of a record into 0x41ec08..0x41ec0e. */
void compute_rect_e5960(ef_addr32 record)
{
    uint16_t x = (uint16_t)(EF_U16(record + 0x41u) + EF_U16(record + 0x71u));
    uint32_t y = EF_U16(record + 0x43u) + (uint32_t)EF_U16(record + 0x73u);

    EF_U16(0x0041ec08u) = x;
    EF_U16(0x0041ec0au) = (uint16_t)y;
    EF_U16(0x0041ec0cu) = EF_U16(record + 0x75u);
    EF_U16(0x0041ec0eu) = EF_U16(record + 0x77u);
}

/* 0xe5c03, 41 bytes. strncmp. */
int32_t compare_strings_e5c03(ef_addr32 a, ef_addr32 b, uint32_t count)
{
    while (count != 0) {
        uint8_t ca = EF_U8(a);
        uint8_t cb = EF_U8(b);

        if (ca != cb)
            return (int32_t)((uint32_t)ca - (uint32_t)cb);
        if (EF_U8(a) == 0)
            return 0;
        a++;
        b++;
        count--;
    }
    return 0;
}

/* 0xeb448, 48 bytes. */
void enter_handler_eb448(uint32_t value)
{
    EF_U8(0x0017d404u)++;
    if (EF_U8(0x0017d404u) == 1u) {
        EF_U32(0x00160a24u) = value;
        if (EF_U32(0x0040d0b0u) != 0x20u)
            guest_fn_eb25a(0xffffffffu);
    }
}

/* 0xeff7f, 53 bytes. Convert an unsigned 32-bit integer to an 80-bit extended value at dest. */
void uint_to_extended_eff7f(uint32_t value, ef_addr32 destination)
{
    uint32_t mantissa = 0;
    uint32_t exponent = 0;

    if (value != 0) {
        uint32_t n = 31;

        while ((value >> n) == 0)
            n--;
        mantissa = value << (31u - n);
        exponent = n + 0x3fffu;
    }
    EF_U32(destination) = 0;
    EF_U32(destination + 4u) = mantissa;
    EF_U16(destination + 8u) = (uint16_t)exponent;
}

/* 0xf79b0, 62 bytes. OR the mask at 0x46438c into the flag words. */
void or_flag_mask_f79b0(void)
{
    uint16_t mask;
    ef_addr32 object;

    if (EF_U8(0x0041f2e2u) != 2u)
        guest_fn_f6124();
    object = EF_U32(0x0041ecccu);
    mask = EF_U16(0x0046438cu);
    EF_U16(0x0041ec3cu) = (uint16_t)(EF_U16(0x0041ec3cu) | mask);
    EF_U16(object + 0x14u) = (uint16_t)(EF_U16(object + 0x14u) | mask);
}

/* 0xf88a0, 69 bytes. */
uint32_t check_entry_f88a0(ef_addr32 object, int16_t index)
{
    uint32_t r = guest_fn_bd4e0(EF_U8(EF_U32(object + 5u) + (uint32_t)((int32_t)index * 10) + 9u));
    int16_t a;
    int16_t b;

    if ((uint16_t)r == 0)
        return r;
    a = (int16_t)guest_fn_f8a30(object);
    b = (int16_t)guest_fn_f89e0(object);
    return a <= b;
}

/* 0xfa77b, 57 bytes. Move the node owning object from the list at 0x485f9c to the free list. */
void release_list_node_fa77b(ef_addr32 object)
{
    ef_addr32 link = 0x00485f9cu;
    ef_addr32 node;

    for (;;) {
        node = EF_U32(link);
        if (node == 0)
            return;
        if (EF_U32(node + 4u) == object)
            break;
        link = node;
    }
    EF_U8(object + 0xcu) = (uint8_t)(EF_U8(object + 0xcu) | 3u);
    EF_U32(link) = EF_U32(node);
    {
        uint32_t old = EF_U32(0x004644e0u);

        EF_U32(0x004644e0u) = node;
        EF_U32(node) = old;
    }
}

/* 0xfc050, 57 bytes. Classify a kind into 1, 2 or 3. */
uint8_t classify_kind_fc050(uint8_t kind)
{
    if (kind == 0xd || kind == 0xe || kind == 0x19)
        return 3;
    if (kind >= 0xf && kind <= 0x11)
        return 2;
    return 1;
}

/* 0xfc6c8, 92 bytes. Query the video driver and clamp the width field. */
void query_video_fc6c8(uint32_t value)
{
    ef_addr32 info;
    uint32_t width;

    EF_U32(0x00164630u) = value;
    EF_U32(0x00164634u) = 0x4f06u;
    EF_U32(0x00164628u) = 0;
    guest_fn_8b8(0x10, 0x00164618u);
    EF_U16(0x0016462au) = 0;
    info = EF_U32(0x00164610u);
    width = EF_U16(info + 0x12u);
    if ((int32_t)width < EF_S32(0x00164628u))
        width = EF_U32(0x00164628u);
    EF_U16(info + 0x10u) = (uint16_t)width;
}
