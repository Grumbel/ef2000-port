/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Game-state helpers, 0x3cf08..0x6b444. */

/* 0x3cf08, 137 bytes. Fire due timed events from the 11-byte table at 0x1a57d0. */
void run_timed_events_3cf08(void)
{
    int16_t i;
    for (i = 0; i < (int16_t)EF_U8(0x001a5aa0u); ++i) {
        ef_addr32 e = 0x001a57d0u + (uint32_t)(int32_t)i * 11u;
        if (EF_U8(e + 9u) == 0 && EF_U32(0x001228a4u) > EF_U32(e + 3u)) {
            if (EF_U8(e + 0xau) != 0)
                EF_U16(0x00159130u) = (uint16_t)(EF_U16(0x00159130u) ^ EF_U8(e));
            else
                guest_fn_9e508(EF_S32(e + 5u) >> 16, EF_U8(e));
            EF_U8(e + 9u) = 1;
        }
    }
}

/* 0x3db9c, 84 bytes. Fire due events from the 9-byte table at 0x1a5a28. */
void run_timed_events_3db9c(void)
{
    int16_t i;
    for (i = 0; i < (int16_t)EF_U8(0x001a5aa1u); ++i) {
        ef_addr32 e = 0x001a5a28u + (uint32_t)(int32_t)i * 9u;
        if (EF_U8(e + 8u) == 0 && EF_U32(0x001228a4u) >= EF_U32(e)) {
            guest_fn_151c4(EF_U32(e + 4u));
            EF_U8(e + 8u) = (uint8_t)(EF_U8(e + 8u) + 1u);
        }
    }
}

/* 0x41e24, 93 bytes. */
uint32_t code_in_set_41e24(uint32_t code)
{
    code &= 0xffu;
    if (code == 8 || code == 9 || code == 10 || code == 12 || code == 13 || code == 14 || code == 15)
        return 1;
    if (code >= 0x11 && code <= 0x17)
        return 1;
    return code == 0x1a;
}

/* 0x488f8, 99 bytes. Store a mode byte in unit AX and send the matching request. */
void set_unit_mode_488f8(int16_t unit, uint8_t mode)
{
    ef_addr32 record;
    uint32_t code;
    if ((EF_S32(0x0019bb5cu) >> 16) == 0xff)
        return;
    record = EF_U32(0x0019bb58u) + (uint32_t)(int32_t)unit * 0xcau;
    EF_U8(record + 0x5eu) = mode;
    if (mode == 0)
        code = 0x0b;
    else if (mode == 1)
        code = 0x0c;
    else if (mode == 2)
        code = 0x0d;
    else
        return;
    guest_fn_46168(EF_S32(record + 0x4eu) >> 16, code, 0, 0, 0);
}

/* 0x4bc80, 82 bytes. */
void update_marker_4bc80(ef_addr32 object)
{
    uint16_t flags = EF_U16(object + 0xa7u);
    if ((flags & 0x1000u) != 0 || (flags & 0x10u) == 0)
        return;
    if (EF_U8(0x001a559bu) == 0)
        guest_fn_4c78c(0x00137980u, 0x00137988u);
    else
        guest_fn_4c900(0x00137980u, 0x00137960u);
}

/* 0x4ca90, 82 bytes. Clear the pending (==1) entries of the list at 0x1a5510 and refresh if any. */
void clear_pending_4ca90(void)
{
    uint8_t any = 0;
    uint8_t i;
    for (i = 0; i < EF_U8(0x001a5531u); ++i) {
        if (EF_U8(0x001a5510u + i) == 1) {
            EF_U8(0x001a5510u + i) = 0;
            any = 1;
        }
    }
    if (any) {
        guest_fn_4f314(0x001322ceu, 0x001322f4u);
        guest_fn_4f7d0();
    }
}

/* 0x4d114, 107 bytes. Draw the sprite described at 0x1a54f6. */
void draw_sprite_4d114(void)
{
    if (EF_U8(0x001a53dcu) == 0 || EF_U32(0x001a54f6u) == 0)
        return;
    guest_fn_ca620((EF_S32(0x001346cbu) >> 16) + (EF_S32(0x001346cdu) >> 16) * 640 + (int32_t)EF_U32(0x00126570u),
                   (int16_t)(EF_U16(0x001a54feu) - 0x16u), (int16_t)(EF_U16(0x001a5500u) - 0x15u),
                   0x2c, 0x2a, 0x280);
}

/* 0x4d180, 107 bytes. As 0x4d114 for the sprite at 0x1a54fa. */
void draw_sprite_4d180(void)
{
    if (EF_U8(0x001a53dcu) == 0 || EF_U32(0x001a54fau) == 0)
        return;
    guest_fn_ca620((EF_S32(0x001346cbu) >> 16) + (EF_S32(0x001346cdu) >> 16) * 640 + (int32_t)EF_U32(0x00126570u),
                   (int16_t)(EF_U16(0x001a54feu) - 0x16u), (int16_t)(EF_U16(0x001a5500u) - 0x15u),
                   0x2c, 0x2a, 0x280);
}

/* 0x4d5f0, 100 bytes. */
void set_ranges_4d5f0(void)
{
    guest_fn_9e0c4(EF_S32(0x001a53d0u) >> 16, 0, 0, 0);
    guest_fn_9e0c4(EF_S32(0x001a53d4u) >> 16, 0x007a1200u, 0, 0xff676980u);
    guest_fn_9e0c4(EF_S32(0x001a53d2u) >> 16, 0xfff85ee0u, 0, 0x00e4e1c0u);
    guest_fn_9e0c4(EF_S32(0x001a53ceu) >> 16, 0x003d0900u, 0, 0xfe363c80u);
}

/* 0x54328, 82 bytes. */
void step_sequence_54328(void)
{
    if (EF_U8(0x0019726au) == 0)
        return;
    if ((int16_t)EF_U8(0x0019726au) <= (int16_t)EF_U16(0x001a5c1au))
        EF_U16(0x001a5c1au) = 0;
    guest_fn_759c((int16_t)EF_U16(EF_U32(0x00197260u) + (EF_U32(0x001a5c18u) >> 16) * 5u + 1u));
    EF_U16(0x001a5c1au) = (uint16_t)(EF_U16(0x001a5c1au) + 1u);
}

/* 0x54ba0, 109 bytes. Copy three dwords and a byte from a message into record +7/+0xb/+0xf/+0x13. */
void copy_message_54ba0(ef_addr32 message)
{
    ef_addr32 record = EF_U32(0x001a14f8u) + (uint32_t)(int32_t)(int16_t)EF_U16(message + 1u) * 69u;
    EF_U32(record + 7u) = EF_U32(message + 3u);
    EF_U32(record + 0xbu) = EF_U32(message + 7u);
    EF_U32(record + 0xfu) = EF_U32(message + 0xbu);
    EF_U8(record + 0x13u) = EF_U8(message + 0xfu);
}

/* 0x54ff4, 91 bytes. */
void handle_message_54ff4(ef_addr32 message)
{
    int16_t unit;
    uint8_t owner;
    if (EF_U8(0x001f9012u) != 0)
        return;
    unit = (int16_t)EF_U16(message + 1u);
    owner = EF_U8(EF_U32(0x0019bb58u) + (uint32_t)(int32_t)unit * 0xcau);
    if (owner == EF_U8(0x001a666bu) || owner == 0xffu || owner == 0xfeu) {
        guest_fn_54c10((int32_t)unit);
        guest_fn_546e8((int32_t)unit, EF_U8(message + 3u));
    }
}

/* 0x557e8, 117 bytes. */
void update_view_557e8(void)
{
    if (EF_U8(0x001a666bu) != 0 && EF_U8(0x0019bc02u) == 0 && EF_U8(0x001f9010u) == 0) {
        uint32_t player;
        if ((EF_S32(0x001f71c8u) >> 16) != 0xff) {
            EF_U16(0x001f71ceu) = EF_U16(0x001f71cau);
            guest_fn_8f644(0xff);
        }
        player = EF_U8(0x001a666bu);
        EF_U8(0x0019bc02u) = 1;
        EF_U8(0x001a5c32u + player * 27u) = 0;
        guest_fn_544c0();
    }
    guest_fn_54600();
}

/* 0x56018, 106 bytes. Record a value in the history of twenty dwords at 0x1a5d70; returns 1 if already present. */
uint8_t remember_value_56018(uint32_t value)
{
    int16_t count = (int16_t)EF_U16(0x001a6630u);
    int16_t i;
    uint8_t result = 0;
    for (i = 0; i < count; ++i) {
        if (value == EF_U32(0x001a5d70u + (uint32_t)(int32_t)i * 4u)) {
            result = 1;
            break;
        }
    }
    if (!result) {
        EF_U32(0x001a5d70u + (uint32_t)(int32_t)count * 4u) = value;
        ++count;
        if (count >= 0x14) {
            for (i = 0; i < 0x13; ++i)
                EF_U32(0x001a5d70u + (uint32_t)(int32_t)i * 4u) = EF_U32(0x001a5d74u + (uint32_t)(int32_t)i * 4u);
            --count;
        }
    }
    EF_U16(0x001a6630u) = (uint16_t)count;
    return result;
}

/* 0x5c07c, 77 bytes. Move word 0x1281f6 towards 0 or 100 by word 0x127d92. */
void adjust_level_5c07c(void)
{
    int16_t level = (int16_t)EF_U16(0x001281f6u);
    if (EF_U8(0x00126528u) != 0) {
        if (level != 0) {
            level = (int16_t)(level - (int16_t)EF_U16(0x00127d92u));
            if (level < 0)
                level = 0;
        }
    } else if (level < 100) {
        level = (int16_t)(level + (int16_t)EF_U16(0x00127d92u));
        if (level > 100)
            level = 100;
    }
    EF_U16(0x001281f6u) = (uint16_t)level;
}

/* 0x5f8d4, 106 bytes. Switch indicator 0x1a6d18 on (mode 1) or off. */
void set_indicator_5f8d4(uint8_t mode)
{
    ef_addr32 unit = guest_fn_7489c(EF_S32(0x001f71c8u) >> 16);
    if (unit == 0)
        return;
    if (mode == 1) {
        if (EF_U8(0x001a6d18u) == 0)
            guest_fn_9e508(EF_S32(unit + 0x1eu) >> 16, 0x23);
        EF_U8(0x001a6d18u) = 1;
        return;
    }
    if (EF_U8(0x001a6d18u) != 0)
        guest_fn_9e508(EF_S32(unit + 0x1eu) >> 16, 0x24);
    EF_U8(0x001a6d18u) = 0;
}

/* 0x64fa4, 107 bytes. Build the doubly linked free list of 100 entries (0x1c bytes) at 0x1a6d60. */
void init_free_list_64fa4(void)
{
    int16_t i;
    EF_U16(0x001a81aau) = 0;
    EF_U32(0x001a7c58u) = 0x001a6d60u;
    EF_U32(0x001a7c54u) = 0;
    for (i = 0; i < 100; ++i) {
        uint32_t entry = (uint32_t)(int32_t)i * 0x1cu;
        EF_U8(entry + 0x001a6d6cu) = 0;
        if (i != 0) {
            uint32_t previous = (uint32_t)(int32_t)(i - 1) * 0x1cu;
            EF_U32(previous + 0x001a6d76u) = 0x001a6d60u + entry;
            EF_U32(entry + 0x001a6d72u) = previous + 0x001a6d60u;
        }
    }
    EF_U32(0x001a784au) = 0;
    EF_U32(0x001a6d72u) = 0;
}

/* 0x65360, 118 bytes. Find the active entry with type AL and id DX, unlink it and return it to the free list. */
void release_entry_65360(uint8_t type, uint16_t id)
{
    ef_addr32 node = EF_U32(0x001a7c54u);
    ef_addr32 next;
    ef_addr32 previous;
    while (node != 0) {
        if (EF_U8(node + 0xcu) == type && EF_U16(node + 0xeu) == id)
            break;
        node = EF_U32(node + 0x16u);
    }
    if (node == 0)
        return;
    EF_U8(node + 0xcu) = 0;
    next = EF_U32(node + 0x16u);
    previous = EF_U32(node + 0x12u);
    if (next != 0)
        EF_U32(next + 0x12u) = previous;
    if (previous != 0)
        EF_U32(previous + 0x16u) = next;
    else
        EF_U32(0x001a7c54u) = next;
    EF_U32(node + 0x16u) = EF_U32(0x001a7c58u);
    EF_U32(node + 0x12u) = 0;
    EF_U32(0x001a7c58u) = node;
    EF_U32(0x001a81aau) = EF_U16(0x001a81aau) - 1u;
}

/* 0x65748, 124 bytes. Flag 4 or 1 in byte 0x1a81de when the object is within range of the viewpoint (positions scaled by 256). */
void check_range_65748(ef_addr32 object)
{
    ef_addr32 target = guest_fn_87f9c((EF_S32(object + 0xcu) >> 28) & 0xff, EF_U16(object + 0xeu) & 0xfffu);
    int32_t dx, dz, distance;
    if (target == 0 || EF_U16(0x001f71cau) != EF_U16(target + 0x56u))
        return;
    dx = ((int32_t)EF_U32(0x001a5becu) - (EF_S32(object) >> 8)) >> 8;
    dz = ((int32_t)EF_U32(0x001a5be8u) - (EF_S32(object + 8u) >> 8)) >> 8;
    distance = dx * dx + dz * dz;
    if (distance >= 0x288)
        return;
    if (distance < 0xa2)
        EF_U8(0x001a81deu) |= 4;
    else
        EF_U8(0x001a81deu) |= 1;
}

/* 0x657c4, 120 bytes. As 0x65748 without the second shift, flags in byte 0x1a81dd. */
void check_range_657c4(ef_addr32 object)
{
    ef_addr32 target = guest_fn_87f9c((EF_S32(object + 0xcu) >> 28) & 0xff, EF_U16(object + 0xeu) & 0xfffu);
    int32_t dx, dz, distance;
    if (target == 0 || EF_U16(0x001f71cau) != EF_U16(target + 0x56u))
        return;
    dx = (int32_t)EF_U32(0x001a5becu) - (EF_S32(object) >> 8);
    dz = (int32_t)EF_U32(0x001a5be8u) - (EF_S32(object + 8u) >> 8);
    distance = dx * dx + dz * dz;
    if (distance >= 0x288)
        return;
    if (distance < 0xa2)
        EF_U8(0x001a81ddu) |= 4;
    else
        EF_U8(0x001a81ddu) |= 1;
}

/* 0x6a090, 90 bytes. True when any of the twenty slots refers to a record whose type byte is 10 or 11. */
uint8_t any_special_slot_6a090(void)
{
    int32_t i;
    for (i = 0; i < 0x14; ++i) {
        int32_t a = EF_S32(0x001282e2u + (uint32_t)i * 2u) >> 16;
        int32_t first = EF_S32(0x001f6ef1u + (uint32_t)a * 3u) >> 24;
        int32_t second = EF_S32(0x001f6ef1u + (uint32_t)i * 3u) >> 24;
        if (first == 10 || second == 10 || second == 11 || first == 11)
            return 1;
    }
    return 0;
}

/* 0x6b444, 104 bytes. */
void notify_unit_6b444(int16_t unit)
{
    ef_addr32 record = guest_fn_7489c((int32_t)unit);
    uint8_t type;
    if (record == 0 || (uint16_t)unit == EF_U16(0x001f71cau))
        return;
    type = EF_U8(record + 0x16cu);
    if (type != 1 && type != 5 && type != 6)
        return;
    if (guest_fn_7489c(EF_S32(record + 0x156u) >> 16) == 0)
        return;
    guest_fn_46168((int32_t)unit, 0x2e, EF_U16(record + 0x158u), 0, 0);
}
