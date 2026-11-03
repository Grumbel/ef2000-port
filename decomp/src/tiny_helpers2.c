/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Trivial accessors, flag setters and empty stubs. */

/* 0x106a8, 1 bytes. Empty stub (single ret). */
void nop_106a8(void)
{
}

/* 0x151c4, 1 bytes. Empty stub (single ret). */
void nop_151c4(void)
{
}

/* 0x392fc, 1 bytes. Empty stub (single ret). */
void nop_392fc(void)
{
}

/* 0x3930c, 1 bytes. Empty stub (single ret). */
void nop_3930c(void)
{
}

/* 0x54b9c, 1 bytes. Empty stub (single ret). */
void nop_54b9c(void)
{
}

/* 0x68a28, 1 bytes. Empty stub (single ret). */
void nop_68a28(void)
{
}

/* 0x76944, 1 bytes. Empty stub (single ret). */
void nop_76944(void)
{
}

/* 0x80d88, 1 bytes. Empty stub (single ret). */
void nop_80d88(void)
{
}

/* 0x81864, 1 bytes. Empty stub (single ret). */
void nop_81864(void)
{
}

/* 0x866d4, 1 bytes. Empty stub (single ret). */
void nop_866d4(void)
{
}

/* 0x9be0c, 1 bytes. Empty stub (single ret). */
void nop_9be0c(void)
{
}

/* 0xe7a30, 1 bytes. Empty stub (single ret). */
void nop_e7a30(void)
{
}

/* 0xeb259, 1 bytes. Empty stub (single ret). */
void nop_eb259(void)
{
}

/* 0xfc8c0, 1 bytes. Empty stub (single ret). */
void nop_fc8c0(void)
{
}

/* 0xfd162, 1 bytes. Empty stub (single ret). */
void nop_fd162(void)
{
}

/* 0x10483c, 1 bytes. Empty stub (single ret). */
void nop_10483c(void)
{
}

/* 0xd7760, 1 bytes. Empty stub (single ret). */
void nop_d7760(void)
{
}

/* 0x9d770, 1 bytes. Empty stub (single ret). */
void nop_9d770(void)
{
}

/* 0x54008, 5 bytes. Tail jump to the empty stub 0x106a8. */
void nop_54008(void)
{
}

/* 0x9d774, 3 bytes. Empty stub that pops two stack arguments (ret 8). */
void nop_9d774(uint32_t stack_a, uint32_t stack_b)
{
    (void)stack_a;
    (void)stack_b;
}

/* 0x151c8, 8 bytes. */
void set_flag_151c8(void)
{
    EF_U8(0x001228a0u) = 1;
}

/* 0x17474, 9 bytes. */
uint32_t call_2dba4_17474(int16_t index)
{
    return guest_fn_2dba4((int32_t)index) & 0xffff00ffu;
}

/* 0x39310, 7 bytes. */
void set_global_39310(int16_t value)
{
    EF_U32(0x00126308u) = (uint32_t)(int32_t)value;
}

/* 0x3ce2c, 3 bytes. */
uint8_t return_true_3ce2c(void)
{
    return 1;
}

/* 0x3efa0, 6 bytes. */
uint8_t get_byte_3efa0(void)
{
    return EF_U8(0x0015b7d4u);
}

/* 0x4904c, 6 bytes. */
uint8_t get_byte_4904c(void)
{
    return EF_U8(0x001a527eu);
}

/* 0x4b188, 3 bytes. */
uint8_t return_false_4b188(void)
{
    return 0;
}

/* 0x54efc, 8 bytes. */
void set_flag_54efc(void)
{
    EF_U8(0x001a5d1fu) = 1;
}

/* 0x57134, 9 bytes. Copy the byte at +1 of the record to 0x1a3649. */
void copy_byte_57134(ef_addr32 record)
{
    EF_U8(0x001a3649u) = EF_U8(record + 1u);
}

/* 0x7eebc, 8 bytes. */
uint32_t get_flag_7eebc(void)
{
    return EF_U8(0x0012b070u);
}

/* 0x810ec, 8 bytes. */
uint32_t get_flag_810ec(void)
{
    return EF_U8(0x0012b104u);
}

/* 0x85cf8, 8 bytes. */
void set_mode_85cf8(void)
{
    EF_U8(0x001a3646u) = 3;
}

/* 0x8bde0, 6 bytes. */
void set_global_8bde0(uint32_t value)
{
    EF_U32(0x0020f430u) = value;
}

/* 0x8f3c0, 3 bytes. */
uint32_t return_zero_8f3c0(void)
{
    return 0;
}

/* 0xcc8e0, 3 bytes. Returns the ESC scan code. */
uint8_t return_esc_cc8e0(void)
{
    return 0x1b;
}

/* 0xce08c, 6 bytes. */
uint32_t get_global_ce08c(void)
{
    return EF_U32(0x0015b764u);
}

/* 0xcf94a, 7 bytes. Absolute value. */
uint32_t abs_cf94a(int32_t value)
{
    if (value < 0)
        return 0u - (uint32_t)value;
    return (uint32_t)value;
}

/* 0xd76f4, 5 bytes. */
void set_word_d76f4(ef_addr32 record, uint16_t value)
{
    EF_U16(record + 0xfu) = value;
}

/* 0xd76fc, 5 bytes. */
uint16_t get_word_d76fc(ef_addr32 record)
{
    return EF_U16(record + 0xfu);
}

/* 0xd7714, 5 bytes. */
void set_word_d7714(ef_addr32 record, uint16_t value)
{
    EF_U16(record + 0x11u) = value;
}

/* 0xd771c, 7 bytes. High word of the dword at +0xf, sign-extended. */
int32_t get_high_word_d771c(ef_addr32 record)
{
    return (int32_t)EF_U32(record + 0xfu) >> 16;
}

/* 0xd7730, 4 bytes. */
uint32_t get_dword_d7730(ef_addr32 record)
{
    return EF_U32(record + 0x13u);
}

/* 0xd7754, 4 bytes. */
uint32_t get_dword_d7754(ef_addr32 record)
{
    return EF_U32(record + 0x18u);
}

/* 0xd7758, 4 bytes. */
void set_dword_d7758(ef_addr32 record, uint32_t value)
{
    EF_U32(record + 0x18u) = value;
}

/* 0xd775c, 4 bytes. */
uint32_t get_dword_d775c(ef_addr32 record)
{
    return EF_U32(record + 0x1cu);
}

/* 0xd7800, 4 bytes. */
void set_dword_d7800(ef_addr32 record, uint32_t value)
{
    EF_U32(record + 0x20u) = value;
}

/* 0xe2b54, 6 bytes. */
uint32_t get_global_e2b54(void)
{
    return EF_U32(0x0015f758u);
}

/* 0xe2fd5, 6 bytes. */
uint32_t get_address_e2fd5(void)
{
    return 0x0015f76cu;
}

/* 0xebd30, 6 bytes. */
uint32_t get_address_ebd30(void)
{
    return 0x00484484u;
}

/* 0xebd36, 6 bytes. */
uint32_t get_address_ebd36(void)
{
    return 0x00484480u;
}

/* 0xfa89a, 6 bytes. */
uint32_t get_global_fa89a(void)
{
    return EF_U32(0x00132a3cu);
}

/* 0xfddb0, 7 bytes. (a * b) / c with a 64-bit intermediate; EDX is preserved. */
int32_t muldiv_fddb0(int32_t a, int32_t b, int32_t c)
{
    return (int32_t)(((int64_t)a * (int64_t)b) / (int64_t)c);
}

/* 0x109e6c, 3 bytes. */
uint32_t return_zero_109e6c(void)
{
    return 0;
}

/* 0x10d9e0, 3 bytes. */
uint32_t return_zero_10d9e0(void)
{
    return 0;
}

/* 0x10d1f4, 9 bytes. */
uint32_t block_start_10d1f4(ef_addr32 block)
{
    return ((EF_U32(block - 4u) & 0xfffffffeu)) - 4u;
}
