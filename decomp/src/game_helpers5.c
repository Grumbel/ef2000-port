/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Small game-state setters, list helpers and record copiers, 0xe744..0x77be0. */

/* 0xe744, 31 bytes. Allocate-and-clear: allocate count bytes via b3db0/b3f58 and zero them. */
ef_addr32 alloc_cleared_e744(uint32_t count)
{
    ef_addr32 block = guest_fn_b3f58((int16_t)guest_fn_b3db0(count));

    guest_fn_b3fb0(block, 0, count);
    return block;
}

/* 0x1d778, 42 bytes. */
void select_stage_byte_1d778(ef_addr32 object)
{
    uint32_t index = EF_U8(object + 0xa5u);

    EF_U8(object + 0x41u) = EF_U8(EF_U32(object + 0xa1u) + index * 25u + 0x18u);
}

/* 0x1d8a8, 63 bytes. Advance the stage counter for some stage kinds; returns 1 if advanced by 0x1d8e8. */
uint32_t advance_stage_1d8a8(ef_addr32 object)
{
    uint8_t kind = EF_U8(object + 0x41u);
    uint8_t result = 0;

    if (kind < 0xc) {
        if (kind == 1)
            EF_U8(object + 0xa5u) += 1u;
        return 0;
    }
    if (kind != 0xc && kind != 0x10)
        return 0;
    if ((guest_fn_1d8e8(object) & 0xffu) != 0) {
        result = 1;
        EF_U8(object + 0xa5u) += 1u;
    }
    return result;
}

/* 0x1e130, 53 bytes. Forget the cached record pointer if it equals record index. */
void forget_record_1e130(int16_t index)
{
    ef_addr32 record = EF_U32(0x0019af7cu) + (uint32_t)((int32_t)index * 171);

    if (record == EF_U32(0x0019af78u))
        EF_U32(0x0019af78u) = 0;
}

/* 0x28a34, 57 bytes. Move the head of the used list to the free list. */
void free_list_node_28a34(void)
{
    ef_addr32 node = EF_U32(0x0019b8c4u);
    uint32_t old_free;

    if (node == EF_U32(0x0019b8c8u)) {
        EF_U32(0x0019b8c4u) = 0;
        EF_U32(0x0019b8c8u) = 0;
    } else {
        EF_U32(0x0019b8c4u) = EF_U32(node + 0xau);
    }
    old_free = EF_U32(0x0019b8ccu);
    EF_U32(0x0019b8ccu) = node;
    EF_U32(node + 0xau) = old_free;
}

/* 0x2af20, 65 bytes. Set three flag byte ranges at 0x19bb60.. to 1. */
void set_flag_ranges_2af20(void)
{
    uint32_t i;

    for (i = 0; i < 7; i++)
        EF_U8(0x0019bb6eu + i) = 1;
    for (i = 0; i < 9; i++)
        EF_U8(0x0019bb60u + i) = 1;
    for (i = 0; i < 5; i++)
        EF_U8(0x0019bb69u + i) = 1;
}

/* 0x2af64, 17 bytes. */
void reset_flag_ranges_2af64(void)
{
    set_flag_ranges_2af20();
    EF_U16(0x0019bb5cu) = 0;
}

/* 0x2db64, 63 bytes. Does the unit named by table entry slot carry id? */
uint32_t unit_has_id_2db64(int16_t id, uint32_t slot)
{
    ef_addr32 unit;

    if (id == 0xff)
        return 0;
    slot &= 0xffu;
    unit = guest_fn_7489c(EF_S16(0x001a5c30u + slot * 27u));
    return unit != 0 && EF_U16(unit + 0x28u) == (uint16_t)id;
}

/* 0x35528, 59 bytes. */
void reset_state_35528(void)
{
    EF_U16(0x0019f588u) = 0x7fff;
    EF_U8(0x0019f59fu) = 0;
    EF_U16(0x0019f584u) = 0;
    guest_fn_b3fb0(0x0019f310u, 0, 8);
    EF_U32(0x0019f33cu) = 0;
}

/* 0x357a4, 70 bytes. */
void start_screen_357a4(void)
{
    if (EF_U8(0x0019f59cu) != 0)
        return;
    EF_U8(0x0041f2e8u) = 1;
    guest_fn_a42f4();
    recompute_total_a4030();
    recompute_total_a4030();
    EF_U16(0x001357b2u) = 0;
    if (EF_U8(0x00125846u) == 3u)
        guest_fn_b1e0();
    guest_fn_abe44(0x00182f24u);
}

/* 0x35cc8, 49 bytes. */
void mark_slot_35cc8(void)
{
    EF_U16(0x0019f58eu) = 1;
    guest_fn_b3fb0(0x0019f318u, 0, 8);
    EF_U8(0x0019f318u + EF_U16(0x001357aeu)) = 1;
}

/* 0x3cd6c, 45 bytes. Find the node with tag 0x2117b0 in the ring starting after the head. */
ef_addr32 find_tagged_node_3cd6c(void)
{
    ef_addr32 stop = EF_U32(0x0020f164u);
    ef_addr32 node = EF_U32(EF_U32(0x0020f160u) + 0x34u);

    while (node != stop) {
        if (EF_U32(node + 0x30u) == 0x002117b0u)
            return node;
        node = EF_U32(node + 0x34u);
    }
    return 0;
}

/* 0x3ce30, 61 bytes. Count down the timer at 0x1a35ac, firing 151c4 when it runs out. */
void tick_timer_3ce30(void)
{
    int32_t timer = EF_S32(0x001a35acu);

    if (timer > 0 && timer - (int32_t)EF_U16(0x00127d92u) <= 0)
        guest_fn_151c4(0x0012fc88u);
    EF_U32(0x001a35acu) = EF_U32(0x001a35acu) - EF_U16(0x00127d92u);
}

/* 0x41ec8, 54 bytes. Is the kind in 3..10? */
uint32_t is_kind_3_to_10_41ec8(uint32_t kind)
{
    kind &= 0xffu;
    return kind >= 3 && kind <= 10;
}

/* 0x41f24, 63 bytes. Clear the 20 seven-byte slots and rebuild the screen buffer. */
void clear_slots_41f24(void)
{
    int16_t i;

    for (i = 0; i < 0x14; i++) {
        uint32_t record = (uint32_t)i * 7u;

        EF_U8(0x001a4a0cu + record) = 0;
        EF_U16(0x001a4a0du + record) = 0;
        EF_U16(0x001a4a0fu + record) = 0;
        EF_U8(0x001a4a12u + record) = 0;
    }
    EF_U16(0x001a4b9au) = 0;
    guest_fn_413b0();
}

/* 0x46038, 56 bytes. */
void reset_queue_46038(void)
{
    guest_fn_45ba0();
    EF_U16(0x0012665au) = 0;
    EF_U16(0x00126658u) = 0;
    EF_U16(0x00126656u) = 0;
    EF_U8(0x00126654u) = 0;
    EF_U32(0x00126688u) = 0x0012665cu;
    EF_U8(0x00126655u) = 0;
}

/* 0x48020, 68 bytes. Advance the tick counter at 0x126658 by the frame time (a third in slow mode). */
void advance_ticks_48020(void)
{
    uint16_t ticks = EF_U16(0x00126658u);

    if (EF_U8(0x001a3647u) == 0)
        ticks = (uint16_t)(ticks + EF_U16(0x00127d92u));
    else
        ticks = (uint16_t)((int16_t)ticks + (int32_t)(EF_U16(0x00127d92u) / 3u));
    EF_U16(0x00126658u) = ticks;
}

/* 0x4be88, 56 bytes. */
void check_flags_4be88(ef_addr32 object)
{
    uint16_t flags = EF_U16(object + 0xa7u);

    if ((flags & 0x1000u) != 0)
        return;
    if ((flags & 0x10u) == 0)
        return;
    guest_fn_4ce4c(0x00137980u, 0x00137988u);
}

/* 0x50c70, 51 bytes. */
void reset_text_50c70(void)
{
    uint8_t value;

    EF_U32(0x001a58acu) = EF_U32(0x001a58b0u);
    EF_U8(0x00127ce4u) = 0;
    EF_U8(0x00127d30u) = 0;
    value = EF_U8(0x00185120u);
    EF_U16(0x00127ce2u) = 0;
    EF_U8(0x001a5986u) = value;
    EF_U8(0x001a5968u) = value;
}

/* 0x50dc8, 31 bytes. Does the text at 0x1a5968 end in ':'? */
uint32_t text_ends_with_colon_50dc8(void)
{
    int16_t length = (int16_t)guest_fn_b1b8b(0x001a5968u);

    return EF_U8(0x001a5967u + (uint32_t)(int32_t)length) == 0x3au;
}

/* 0x52128, 57 bytes. */
void note_extension_52128(void)
{
    EF_U8(0x001a5a99u) = 1;
    guest_fn_50d04();
    if (EF_U8(0x001a5968u) == 0x2e && guest_fn_b1b8b(0x001a5968u) < 5u)
        copy_string_9d749(0x001a5a1cu, 0x001a5968u);
}

/* 0x54eb4, 69 bytes. Mark slot b[ptr+1] in table 0x1a5d10 if the sender matches. */
void mark_slot_54eb4(ef_addr32 message)
{
    uint8_t slot = EF_U8(message + 1u);

    if (EF_U8(0x001a6668u) != slot) {
        EF_U8(0x001a6669u) = 1;
        return;
    }
    if (EF_U8(0x001a5d10u + slot) == 0) {
        EF_U8(0x001a5d10u + slot) = 1;
        EF_U8(0x001a5d1du) = (uint8_t)(EF_U8(0x001a5d1du) + 1u);
    }
}

/* 0x54f04, 69 bytes. As above for table 0x1a5d08 / counter 0x1a5d1e. */
void mark_slot_54f04(ef_addr32 message)
{
    uint8_t slot = EF_U8(message + 1u);

    if (EF_U8(0x001a6668u) != slot) {
        EF_U8(0x001a6669u) = 1;
        return;
    }
    if (EF_U8(0x001a5d08u + slot) == 0) {
        EF_U8(0x001a5d08u + slot) = 1;
        EF_U8(0x001a5d1eu) = (uint8_t)(EF_U8(0x001a5d1eu) + 1u);
    }
}

/* 0x55050, 52 bytes. */
void clear_pending_55050(ef_addr32 message)
{
    if (EF_U8(message + 3u) == EF_U8(0x001a666bu) && EF_U16(0x00198442u) == EF_U16(message + 1u)) {
        EF_U16(0x00198444u) = 0;
        EF_U16(0x00198442u) = 0xff;
    }
}

/* 0x550d8, 67 bytes. Copy the 0x19-byte payload of a message into object message[1]. */
void copy_payload_550d8(ef_addr32 message)
{
    ef_addr32 object = guest_lookup_object_8ffe8((int32_t)EF_S16(message + 1u));

    if (object == 0)
        return;
    EF_U32(object) = EF_U32(message + 3u);
    EF_U32(object + 4u) = EF_U32(message + 7u);
    EF_U32(object + 8u) = EF_U32(message + 0xbu);
    EF_U16(object + 0xcu) = EF_U16(message + 0xfu);
    EF_U16(object + 0xeu) = EF_U16(message + 0x11u);
    EF_U16(object + 0x10u) = EF_U16(message + 0x13u);
    EF_U32(object + 0x12u) = EF_U32(message + 0x15u);
}

/* 0x56d18, 40 bytes. */
void set_name_56d18(uint32_t value)
{
    guest_fn_56d40(value);
    if (guest_fn_b1b8b(0x001971b4u) == 0) {
        copy_string_9d749(0x001971b4u, value + 1u);
        guest_fn_6b50();
    }
}

/* 0x56e94, 59 bytes. */
void handle_message_56e94(ef_addr32 message)
{
    if (EF_U8(message + 0x7au) == 0 ||
        EF_U8(0x0012584cu + EF_U8(0x001a666bu)) == EF_U8(0x0012584cu + EF_U8(message + 1u)))
        guest_fn_789d0(EF_U8(message + 1u), message + 2u);
    guest_fn_56ed0();
}

/* 0x57160, 55 bytes. */
void notify_unit_57160(ef_addr32 message)
{
    ef_addr32 unit = guest_fn_7489c((int32_t)EF_S16(message + 1u));
    uint32_t kind;

    if (unit == 0)
        return;
    kind = EF_U8(unit + 0xe9u);
    if (kind == 0x20 || kind == 0x21)
        guest_fn_9e508(EF_S16(unit + 0x20u), 0x29);
}

/* 0x571b8, 55 bytes. */
void notify_unit_571b8(ef_addr32 message)
{
    ef_addr32 unit = guest_fn_7489c((int32_t)EF_S16(message + 1u));
    uint32_t kind;

    if (unit == 0)
        return;
    kind = EF_U8(unit + 0xe9u);
    if (kind == 0x20 || kind == 0x21)
        guest_fn_9e508(EF_S16(unit + 0x20u), 0x28);
}

/* 0x57620, 48 bytes. */
void handle_entry_57620(ef_addr32 message)
{
    int32_t index = EF_S16(message + 1u);

    open_entry_cd150((int16_t)EF_S16(0x00211b10u + (uint32_t)(index * 48)));
    guest_fn_cd2e8(EF_S16(message + 3u));
}

/* 0x5ea64, 62 bytes. */
void reset_range_5ea64(void)
{
    EF_U16(0x001a6cccu) = 0;
    EF_U16(0x001a6ccau) = 0;
    EF_U16(0x001a6cceu) = 0xffff;
    EF_U32(0x001a6b08u) = 0;
    EF_U32(0x001a6b04u) = 0;
    EF_U32(0x001a6b00u) = 0x7fffffffu;
}

/* 0x62504, 67 bytes. Process every entry of the 0x17-byte table at 0x1a7d32. */
void process_entries_62504(void)
{
    int16_t i;

    for (i = 0; i < EF_S16(0x001a81c2u); i++) {
        uint32_t record = (uint32_t)(int32_t)i * 0x17u;

        guest_fn_65030(EF_U8(0x001a7d33u + record), EF_S16(0x001a7d34u + record), 0xff);
    }
}

/* 0x65d18, 12 bytes. */
void store_631bc_65d18(void)
{
    EF_U16(0x001a8240u) = (uint16_t)guest_fn_631bc();
}

/* 0x689f0, 56 bytes. Copy the 20 three-byte entries of table 0x1f6eac to 0x1f6ef4. */
void copy_table_689f0(void)
{
    int32_t i;

    for (i = 0; i < 20; i++) {
        EF_U16(0x001f6ef5u + (uint32_t)(i * 3)) = EF_U16(0x001f6eadu + (uint32_t)(i * 3));
        EF_U8(0x001f6ef4u + (uint32_t)(i * 3)) = EF_U8(0x001f6eacu + (uint32_t)(i * 3));
    }
}

/* 0x68a2c, 52 bytes. */
void request_68a2c(void)
{
    EF_U8(0x001f6f0fu) = 0x22;
    EF_U16(0x001f6f10u) = 1;
    guest_fn_e7c50(EF_S16(0x001282dcu), EF_S16(0x0012cfccu), 9);
}

/* 0x73d1c, 52 bytes. Clear a unit flag one time in five. */
void clear_unit_flag_73d1c(int16_t unit_id)
{
    ef_addr32 unit = guest_fn_7489c(unit_id);

    if (unit == 0)
        return;
    if ((half_word_9c960() & 0xffffu) % 5u == 0)
        EF_U8(unit + 0x13fu) = 0;
}

/* 0x7597c, 58 bytes. */
void reset_selection_7597c(void)
{
    EF_U32(0x001f88e4u) = 0xffffffffu;
    EF_U32(0x001f88e0u) = 0xffffffffu;
    EF_U16(0x001f88eau) = 0xffff;
    EF_U8(0x00128548u) = 1;
    EF_U16(0x001f88ecu) = 0;
    EF_U16(0x0012855cu) = 0;
}

/* 0x77be0, 54 bytes. */
void reset_slots_77be0(void)
{
    uint8_t i;

    for (i = 0; i < 8; i++)
        EF_U16(0x001f89a8u + (uint32_t)i * 0x7au) = 0;
    EF_U32(0x0012adf8u) = 0xffffffffu;
    EF_U16(0x001f8d56u) = 0;
}
