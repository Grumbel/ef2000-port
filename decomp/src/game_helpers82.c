/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Record dump printers (output through the original printf-style sink 0xb1bb0). */

/* 0xb2504, 89 bytes. Print a movement record with its kind name. */
void print_move_record_b2504(ef_addr32 rec, int16_t value, uint32_t handle)
{
    guest_fn_b1bb0(handle, 0x0018e150u, (int32_t)value, EF_U32(rec), EF_U32(rec + 4u), EF_S32(rec + 0xau) >> 16,
                   EF_S32(rec + 6u) >> 16, EF_S32(rec + 8u) >> 16, EF_U32(0x00132c2cu + (uint32_t)EF_U8(rec + 0x18u) * 4u),
                   EF_S32(rec + 0xeu) >> 16, EF_S32(rec + 0x10u) >> 16, EF_S32(rec + 0x12u) >> 16);
}

/* 0xb2960, 71 bytes. Print a classified record with its class name. */
void print_class_record_b2960(ef_addr32 rec, uint32_t handle)
{
    guest_fn_b1bb0(handle, 0x0018e278u, EF_U32(rec), EF_U32(rec + 4u), EF_S32(rec + 6u) >> 16, EF_S32(rec + 8u) >> 16,
                   EF_S32(rec + 0xau) >> 16, EF_S32(rec + 0xcu) >> 16,
                   EF_U32(0x00132cc4u + (uint32_t)EF_U8(rec + 0x10u) * 4u));
}

/* 0xb2a2c, 53 bytes. Print a plain coordinate record. */
void print_coordinate_record_b2a2c(ef_addr32 rec, uint32_t handle)
{
    guest_fn_b1bb0(handle, 0x0018e2d0u, EF_U32(rec), EF_U32(rec + 4u), EF_U32(rec + 8u), EF_S32(rec + 0xau) >> 16,
                   EF_S32(rec + 0xcu) >> 16, EF_S32(rec + 0xeu) >> 16);
}
