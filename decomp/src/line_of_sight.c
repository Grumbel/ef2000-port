/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Scaling, selection and line-of-sight request helpers, 0x1c60c..0x66d80. */

/* 0x1c60c, 210 bytes. Scale six signed words by the percentage in dword 0x122dc4. */
void scale_words_1c60c(void)
{
    static const uint32_t words[6] = {0x00420d9cu, 0x00420d9au, 0x00420d9eu, 0x00420d96u, 0x00198446u, 0x0019844cu};
    int32_t percent;
    uint32_t i;
    if ((uint16_t)guest_fn_d6694() == 0)
        return;
    if (EF_U8(0x0015bac6u) == 0 && EF_U8(0x00420a72u) == 0)
        return;
    percent = EF_S32(0x00122dc4u);
    for (i = 0; i < 6; ++i)
        EF_U16(words[i]) = (uint16_t)(((int32_t)EF_S16(words[i]) * percent) / 100);
}

/* 0x238c0, 161 bytes. Can unit AX attack the target of its own target? */
uint8_t can_support_238c0(int32_t unit)
{
    ef_addr32 first = guest_fn_7489c((int32_t)(int16_t)unit);
    ef_addr32 second;
    ef_addr32 base;
    uint8_t flags;
    if (first == 0)
        return 0;
    second = guest_fn_7489c(EF_S32(first + 0x156u) >> 16);
    if (second == 0)
        return 0;
    if ((uint32_t)EF_U16(second + 0x146u) != (uint32_t)unit)
        return 0;
    base = EF_U32(0x0019bb58u);
    if (EF_U16(base + (uint32_t)(EF_S32(second + 0x26u) >> 16) * 0xcau + 0x15u) ==
        EF_U16(base + (uint32_t)(EF_S32(first + 0x26u) >> 16) * 0xcau + 0x15u))
        return 0;
    flags = EF_U8(second + 0x24u);
    if ((flags & 4u) || (flags & 8u))
        return 0;
    if (flags & 2u)
        return (uint8_t)(guest_fn_7574c(EF_S32(second + 0x14u) >> 16) != 0);
    return 1;
}

/* 0x327e0, 156 bytes. Latch the position and target vectors. */
void latch_vectors_327e0(void)
{
    ef_addr32 position = EF_U32(0x0019e54du);
    ef_addr32 target;
    if (position == 0) {
        EF_U8(0x0019e555u) = 0;
        return;
    }
    if (EF_U8(0x0019e5e1u) == 1) {
        EF_U32(0x0019e560u) = EF_U32(position);
        EF_U32(0x0019e564u) = EF_U32(position + 4u);
        EF_U32(0x0019e568u) = EF_U32(position + 8u);
        EF_U16(0x0019e56cu) = EF_U16(position + 0xcu);
        EF_U16(0x0019e56eu) = EF_U16(position + 0xeu);
        EF_U16(0x0019e570u) = EF_U16(position + 0x10u);
    }
    target = EF_U32(0x0019e551u);
    if (target != 0 && EF_U8(0x0019e5e2u) == 1) {
        EF_U32(0x0019e572u) = EF_U32(target);
        EF_U32(0x0019e576u) = EF_U32(target + 4u);
        EF_U32(0x0019e57au) = EF_U32(target + 8u);
        return;
    }
    EF_U8(0x0019e556u) = 0;
}

/* 0x41028, 167 bytes. */
void setup_dialog_41028(void)
{
    EF_U8(0x00125849u) = 0;
    if (EF_U8(0x00125935u) != 0) {
        ef_addr32 dialog;
        EF_U8(0x00125839u) = 1;
        dialog = guest_fn_a46cc(0x001843b8u);
        if (dialog != 0) {
            guest_fn_a73cc(dialog, 0x001843c0u, 1);
            guest_fn_a73cc(dialog, 0x001843ccu, 0);
        }
    } else {
        EF_U8(0x00125839u) = 0;
    }
    guest_fn_abe44(0x001843d8u);
    EF_U8(0x0041f2e7u) = 0;
    EF_U32(0x0019f36cu) = 0;
    if (EF_U8(0x00125839u) == 1 && (EF_U8(0x0013579au) == 1 || EF_U8(0x0013579au) == 2))
        guest_fn_a4db0(0x001843e4u);
    else
        guest_fn_a4d90(0x001843e4u);
}

/* 0x5a8d0, 197 bytes. */
void submit_view_5a8d0(void)
{
    if (guest_fn_7489c(EF_S32(0x001f71c8u) >> 16) == 0)
        return;
    guest_fn_5af50(EF_U8(0x001a6d1cu));
    guest_fn_5aa64(EF_U8(0x001a6d1cu));
    if (EF_U8(0x001a6d20u) == 0)
        return;
    EF_U16(0x004208e4u) = EF_U8(0x00128148u);
    EF_U16(0x004208e6u) = EF_U16(0x001a6cf2u);
    EF_U16(0x004208e8u) = EF_U16(0x001a6cf0u);
    EF_U16(0x004208eau) = EF_U16(0x001a6cf8u);
    EF_U16(0x004208ecu) = EF_U16(0x001a6cf6u);
    guest_fn_cf78c();
    EF_U16(0x004208e4u) = EF_U8(0x00128148u);
    EF_U16(0x004208e6u) = EF_U16(0x001a6cf2u);
    EF_U16(0x004208e8u) = EF_U16(0x001a6cf6u);
    EF_U16(0x004208eau) = EF_U16(0x001a6cf8u);
    EF_U16(0x004208ecu) = EF_U16(0x001a6cf0u);
    guest_fn_cf78c();
}

/* 0x66bf4, 224 bytes. Line-of-sight request from the viewpoint at 0x1a5bec to the object. */
uint8_t request_los_66bf4(ef_addr32 object)
{
    int32_t above;
    uint8_t result;
    if (object == 0)
        return 0;
    EF_U32(0x001a81fcu) = EF_U32(0x001a5becu);
    EF_U32(0x001a8200u) = EF_U32(0x001a5bf0u);
    EF_U32(0x001a8204u) = EF_U32(0x001a5be8u);
    EF_S32(0x001a81f0u) = EF_S32(object) >> 8;
    EF_S32(0x001a81f8u) = EF_S32(object + 8u) >> 8;
    above = EF_S32(object + 4u) >> 8;
    EF_U32(0x00420910u) = EF_U32(0x001a5becu);
    EF_U32(0x00420904u) = EF_U32(0x001a5bf0u);
    EF_U32(0x00420918u) = EF_U32(0x001a5be8u);
    EF_S32(0x001a81f4u) = above;
    EF_U32(0x00420914u) = EF_U32(0x001a81f0u);
    EF_S32(0x00420908u) = above - 10;
    EF_U32(0x0042091cu) = EF_U32(0x001a81f8u);
    EF_U16(0x00420930u) = EF_U16(0x001a5becu);
    EF_U16(0x00420932u) = EF_U16(0x001a5bf0u);
    EF_U16(0x00420934u) = EF_U16(0x001a5be8u);
    EF_U16(0x00420936u) = EF_U16(0x00420914u);
    EF_U16(0x00420938u) = EF_U16(0x00420908u);
    EF_U16(0x0042093au) = EF_U16(0x0042091cu);
    result = (uint8_t)guest_fn_7b7d8();
    EF_U8(0x001a8244u) = result;
    return result;
}

/* 0x66cd4, 169 bytes. Line-of-sight request from `object` to unit AX. */
uint8_t request_los_66cd4(int16_t unit, ef_addr32 object)
{
    ef_addr32 target = guest_fn_8ffe8((int32_t)unit);
    int32_t y;
    if (target == 0 || object == 0)
        return 1;
    EF_S32(0x00420910u) = EF_S32(object) >> 8;
    y = (EF_S32(object + 4u) >> 8) + 0x19;
    EF_S32(0x00420918u) = EF_S32(object + 8u) >> 8;
    EF_S32(0x00420914u) = EF_S32(target) >> 8;
    EF_S32(0x00420904u) = y;
    EF_U16(0x00420932u) = (uint16_t)y;
    EF_S32(0x0042091cu) = EF_S32(target + 8u) >> 8;
    EF_U16(0x00420930u) = EF_U16(0x00420910u);
    EF_U16(0x00420934u) = EF_U16(0x00420918u);
    EF_S32(0x00420908u) = (EF_S32(target + 4u) >> 8) - 2;
    EF_U16(0x00420936u) = EF_U16(0x00420914u);
    EF_U16(0x00420938u) = EF_U16(0x00420908u);
    EF_U16(0x0042093au) = EF_U16(0x0042091cu);
    return (uint8_t)guest_fn_7b7d8();
}

/* 0x66d80, 165 bytes. As 0x66bf4 from the point at 0x21444c. */
uint8_t request_los_66d80(ef_addr32 object)
{
    int32_t above;
    uint8_t result;
    if (object == 0)
        return 0;
    EF_U32(0x00420910u) = EF_U32(0x0021444cu);
    EF_U32(0x00420904u) = EF_U32(0x00214448u);
    EF_U32(0x00420918u) = EF_U32(0x00214450u);
    EF_S32(0x00420914u) = EF_S32(object) >> 8;
    EF_S32(0x0042091cu) = EF_S32(object + 8u) >> 8;
    EF_U16(0x00420930u) = EF_U16(0x0021444cu);
    EF_U16(0x00420932u) = EF_U16(0x00214448u);
    above = (EF_S32(object + 4u) >> 8) - 2;
    EF_U16(0x00420934u) = EF_U16(0x00214450u);
    EF_S32(0x00420908u) = above;
    EF_U16(0x00420936u) = EF_U16(0x00420914u);
    EF_U16(0x00420938u) = (uint16_t)above;
    EF_U16(0x0042093au) = EF_U16(0x0042091cu);
    result = (uint8_t)guest_fn_7b7d8();
    EF_U8(0x001a8244u) = result;
    return result;
}
