/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Camera, entity-list and progress helpers, 0x8dcf4..0x9a4d8. */

/* 0x8dcf4, 83 bytes. Copy flag bit 15 of word +0x24 into bit 0 of word +0x26. */
void toggle_flag_8dcf4(int16_t unit)
{
    ef_addr32 record = guest_fn_7489c((int32_t)unit);
    uint16_t bit;
    if (record == 0)
        return;
    bit = (uint16_t)(EF_U16(record + 0x24u) >> 15);
    EF_U8(record + 0x26u) = (uint8_t)(EF_U8(record + 0x26u) & 0xfeu);
    EF_U16(record + 0x26u) = (uint16_t)(EF_U16(record + 0x26u) | bit);
    if (EF_U8(0x00125847u) != 0)
        guest_fn_9e508(EF_S32(record + 0x1eu) >> 16, ((EF_U8(record + 0x26u) & 1u) == 0) + 7u);
}

/* 0x90828, 88 bytes. */
void update_ticker_90828(void)
{
    uint32_t random = guest_fn_9c960() & 0xffffu;
    int16_t i;
    int16_t id = 0x2001;
    EF_U8(0x001251acu) = (int32_t)(random % 20u) < (int32_t)EF_U16(0x00127d92u);
    for (i = 0; i < (int16_t)EF_U16(0x00211ae0u); ++i) {
        guest_fn_38284((int32_t)id);
        ++id;
    }
}

/* 0x90a70, 84 bytes. Initialise the forty 0x30-byte entries at 0x211b00. */
void init_entries_90a70(void)
{
    int32_t i;
    for (i = 0; i < 0x28; ++i) {
        uint32_t offset = (uint32_t)i * 0x30u;
        EF_U8(offset + 0x00211b14u) = (uint8_t)((EF_U8(offset + 0x00211b14u) | 2u) & 0xfau);
        EF_U16(offset + 0x00211b10u) = (uint16_t)guest_fn_9ddfc(0x0018ab28u, offset);
        guest_fn_9e3f8(EF_S32(offset + 0x00211b0eu) >> 16);
    }
}

/* 0x92f98, 98 bytes. */
void update_viewpoint_92f98(void)
{
    ef_addr32 position;
    if (guest_fn_7489c(EF_S32(0x001f71c8u) >> 16) == 0) {
        guest_fn_91c48(0x12);
        EF_U16(0x002144a2u) = (uint16_t)guest_fn_d0b68(0x0012dc10u, 1);
        return;
    }
    guest_fn_84678();
    position = EF_U32(0x00214458u);
    EF_S32(0x0021444cu) = EF_S32(position) >> 8;
    EF_S32(0x00214448u) = EF_S32(position + 4u) >> 8;
    EF_S32(0x00214450u) = EF_S32(position + 8u) >> 8;
}

/* 0x92ffc, 99 bytes. Step byte 0x12dc34 through the list of words at +0x50 of the current record, skipping the player. */
void step_list_92ffc(void)
{
    uint8_t index = EF_U8(0x0012dc34u);
    ef_addr32 record = EF_U32(0x0019bb58u) + (uint32_t)(EF_S32(0x0019bb5cu) >> 16) * 0xcau;
    ++index;
    if (index >= EF_U8(record + 0x4fu))
        index = 0;
    record = EF_U32(0x0019bb58u) + (uint32_t)(EF_S32(0x0019bb5cu) >> 16) * 0xcau;
    if ((int32_t)EF_U16(record + (uint32_t)index * 2u + 0x50u) == (EF_S32(0x001f71c8u) >> 16))
        ++index;
    EF_U8(0x0012dc34u) = index;
}

/* 0x94484, 109 bytes. */
void update_camera_94484(void)
{
    guest_fn_d099c(0x0021439cu, 0x0012dbecu);
    EF_U16(0x0012db9au) = (uint16_t)guest_fn_d13dc(0x0021439cu, 0xfc00u);
    guest_fn_92070();
    guest_fn_d0ebc(0x0021439cu, 0xfc00u);
    if ((EF_S32(0x0012db98u) >> 16) < -0x1800) {
        guest_fn_95018();
        guest_fn_d0ebc(0x0021439cu, 0x400u);
    }
    guest_fn_d099c(0x0012dbecu, 0x0021439cu);
    guest_fn_93a8c();
}

/* 0x947c4, 108 bytes. */
void look_at_unit_947c4(int16_t unit)
{
    ef_addr32 player = guest_fn_7489c(EF_S32(0x001f71c8u) >> 16);
    ef_addr32 target = guest_fn_7489c((int32_t)unit);
    if (target == 0) {
        EF_U16(0x0021449cu) = 1;
        EF_U8(0x0012db9eu) = EF_U8(0x00214486u);
        return;
    }
    if ((uint16_t)guest_fn_9125c(EF_S32(target + 0x1eu) >> 16) != 0)
        EF_U16(0x0021448au) = (uint16_t)guest_fn_9125c(EF_S32(target + 0x1eu) >> 16);
    guest_fn_9457c(target, player);
}

/* 0x94830, 78 bytes. */
void look_from_player_94830(int16_t unit)
{
    ef_addr32 player = guest_fn_7489c(EF_S32(0x001f71c8u) >> 16);
    ef_addr32 target = guest_fn_7489c((int32_t)unit);
    if ((uint16_t)guest_fn_9125c(EF_S32(player + 0x1eu) >> 16) != 0)
        EF_U16(0x0021448au) = (uint16_t)guest_fn_9125c(EF_S32(player + 0x1eu) >> 16);
    guest_fn_9457c(player, target);
}

/* 0x94a1c, 87 bytes. Unit 0xff means the record at dword 0x1a821c. */
void look_at_target_94a1c(int16_t unit)
{
    ef_addr32 player = guest_fn_7489c(EF_S32(0x001f71c8u) >> 16);
    ef_addr32 target;
    if (unit == 0xff)
        target = EF_U32(0x001a821cu);
    else
        target = guest_fn_7489c((int32_t)unit);
    if ((uint16_t)guest_fn_9125c(EF_S32(target + 0x1eu) >> 16) != 0)
        EF_U16(0x0021448au) = (uint16_t)guest_fn_9125c(EF_S32(target + 0x1eu) >> 16);
    guest_fn_9457c(target, player);
}

/* 0x985f4, 97 bytes. Percentage of progress (value - dword 0x40c120) * 100 / dword 0x40c118, reported on change. */
void update_progress_985f4(uint8_t channel, int32_t value)
{
    int32_t total = EF_S32(0x0040c118u);
    int32_t percent;
    if (total == 0)
        return;
    percent = ((value - EF_S32(0x0040c120u)) * 100) / total;
    if (percent <= 0)
        percent = 0;
    else if (percent >= 100)
        percent = 100;
    if (percent == EF_S32(0x0040c11cu))
        return;
    EF_S32(0x0040c11cu) = percent;
    guest_fn_98658(channel);
}

/* 0x9a4d8, 108 bytes. */
void clear_tables_9a4d8(void)
{
    guest_fn_b3fb0(0x0040cdf8u, 0, 0x78);
    guest_fn_b3fb0(0x0040cf00u, 0, 0x78);
    guest_fn_b3fb0(0x0040cd80u, 0, 0x78);
    guest_fn_b3fb0(0x0040ce88u, 0, 0x78);
    EF_U16(0x0040d02cu) = 0;
    EF_U16(0x0040d026u) = 0;
    EF_U16(0x0040d02au) = 0;
    EF_U16(0x0040d02eu) = 0;
    guest_fn_9a348();
}
