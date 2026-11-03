/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Menu-state, key-class and record-swap helpers, 0xa4030..0xfaa30. */

/* 0xa4030, 211 bytes. Recompute the total size at 0x135838 from the two index lists. */
void recompute_total_a4030(void)
{
    uint32_t total;
    uint32_t i;

    if (EF_U8(0x00135798u) == 0 || EF_U8(0x0013581eu) != 0)
        return;
    guest_fn_a3fe0();
    guest_fn_a410c();
    for (i = 0; (int32_t)i < (int32_t)EF_U8(EF_U32(0x00136b88u)); i++) {
        ef_addr32 entry = EF_U32(0x00135878u + ((uint32_t)EF_U8(EF_U32(0x00136b84u) + i) << 5));

        EF_U32(0x00135838u) += EF_U16(entry + 9u);
    }
    total = EF_U32(0x00135838u);
    for (i = 0; (int32_t)i < (int32_t)EF_U8(EF_U32(0x00136b78u)); i++) {
        ef_addr32 entry = EF_U32(0x00135878u + ((uint32_t)EF_U8(EF_U32(0x00136b74u) + i) << 5));

        total += EF_U16(entry + 9u);
    }
    EF_U32(0x00135838u) = total;
    if (EF_U8(0x0013581du) != 0) {
        EF_U8(0x0013581eu) = 1;
        guest_fn_a3da0();
    }
}

/* 0xa7be8, 237 bytes. Handle the current key code at 0x41f833; returns 1 if consumed. */
uint32_t handle_key_a7be8(uint8_t modifier)
{
    uint32_t key = EF_U8(0x0041f833u);
    ef_addr32 object = EF_U32(0x004191d0u);
    uint32_t buttons;

    if (key == 0x39u && object != 0 && EF_U8(object + 0xfu) != 0 &&
        EF_S16(0x00425fa6u) < EF_S16(0x004194aeu))
        return 0;
    if (EF_U8(0x0041f7c8u) != 0 && key == 0x1fu) {
        EF_U8(0x001371fbu) = 1;
        return 1;
    }
    if (modifier == 0 && key == 0x39u) {
        EF_U8(0x0041f833u) = modifier;
        return 1;
    }
    if (modifier == 0 && key == 1u) {
        EF_U8(0x001371eeu) = 1;
        return key;
    }
    if (key == 0x19u)
        EF_U8(0x001371fdu) = 1;
    if (modifier != 0)
        return 0;
    buttons = mouse_read_buttons_ce064();
    if ((buttons & 1u) != 0)
        return 1;
    if ((buttons & 2u) != 0) {
        EF_U8(0x001371eeu) = 1;
        return 1;
    }
    return 0;
}

/* 0xa9d48, 231 bytes. Toggle between the live view state and the saved copy at 0x4195b0.. */
void toggle_view_state_a9d48(void)
{
    if (EF_U8(0x001371ffu) == 0) {
        EF_U32(0x004195bcu) = EF_U32(0x004191dcu);
        EF_U32(0x004195b8u) = EF_U32(0x004191f8u);
        EF_U16(0x004195b6u) = EF_U16(0x004194acu);
        EF_U32(0x004191dcu) = EF_U32(EF_U32(0x004191dcu) + 1u);
        EF_U8(0x004195b4u) = EF_U8(0x0041955fu);
        EF_U8(0x001371ffu) = 1;
        EF_U32(0x004195b0u) = EF_U32(0x004191d4u);
        save_table_a7e4c();
        guest_fn_a7df4();
        if (EF_U8(0x00137206u) != 0)
            guest_fn_de4b0();
    } else {
        EF_U32(0x004191dcu) = EF_U32(0x004195bcu);
        EF_U32(0x004191f8u) = EF_U32(0x004195b8u);
        EF_U8(0x001371ffu) = 0;
        EF_U16(0x004194acu) = EF_U16(0x004195b6u);
        EF_U8(0x0041955fu) = EF_U8(0x004195b4u);
        EF_U8(0x00137201u) = 1;
        EF_U32(0x004191d4u) = EF_U32(0x004195b0u);
        restore_table_a7e88();
        guest_fn_a7e20();
        if (EF_U8(0x00137208u) != 0) {
            EF_U32(0x004191dcu) = EF_U32(EF_U32(0x004191dcu) + 1u);
            EF_U8(0x00137208u) = 0;
            guest_fn_f41c4();
        }
    }
    EF_U8(0x00137200u) = 0;
}

/* Swap n bytes at offset between two records. */
static void swap_u8(ef_addr32 a, ef_addr32 b, uint32_t offset)
{
    uint8_t t = EF_U8(a + offset);

    EF_U8(a + offset) = EF_U8(b + offset);
    EF_U8(b + offset) = t;
}

static void swap_u16(ef_addr32 a, ef_addr32 b, uint32_t offset)
{
    uint16_t t = EF_U16(a + offset);

    EF_U16(a + offset) = EF_U16(b + offset);
    EF_U16(b + offset) = t;
}

/* 0xe65d8, 169 bytes. Exchange the fields of two 0x15-byte records. */
void swap_records_e65d8(ef_addr32 a, ef_addr32 b)
{
    swap_u8(a, b, 0);
    swap_u8(a, b, 5);
    swap_u8(a, b, 6);
    swap_u16(a, b, 7);
    swap_u16(a, b, 9);
    swap_u16(a, b, 0xb);
    swap_u16(a, b, 0xd);
    swap_u16(a, b, 0xf);
    swap_u16(a, b, 0x11);
    swap_u8(a, b, 0x13);
    swap_u8(a, b, 0x14);
    {
        uint32_t t = EF_U32(a + 1u);

        EF_U32(a + 1u) = EF_U32(b + 1u);
        EF_U32(b + 1u) = t;
    }
}

/* 0xfaa30, 202 bytes. Map a scan code to a key class, 0 if unmapped. */
uint8_t scan_code_class_faa30(uint8_t code)
{
    switch (code) {
    case 0x00: case 0x39: case 0x47: case 0x57: case 0x58:
        return 7;
    case 0x05: case 0x0b: case 0x13: case 0x1d: case 0x26: case 0x2b:
    case 0x49: case 0x4f: case 0x56: case 0x59:
        return 0xa;
    case 0x31:
        return 6;
    case 0x44: case 0x45: case 0x46:
        return 0xb;
    case 0x65:
        return 5;
    case 0x66:
        return 0x12;
    case 0x67:
        return 0x13;
    case 0x68: case 0x69:
        return 9;
    default:
        return 0;
    }
}
