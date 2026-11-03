/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Game-state helpers from the low code range (0xb04..0x1d3c4). */

/* 0xb04, 72 bytes. */
void init_far_stub_b04(void)
{
    uint32_t block;
    uint32_t selector;
    ef_addr32 stub;
    EF_U32(0x00120d24u) = guest_fn_a60();
    EF_U32(0x0019613cu) = guest_fn_510(6);
    selector = guest_fn_604();
    block = EF_U32(0x00120d24u);
    guest_fn_a9c(block, selector);
    stub = EF_U32(0x0019613cu);
    EF_U8(stub) = 0x9au;
    EF_U8(stub + 5u) = 0xcfu;
    guest_fn_9c3cf(0xad8u);
}

/* 0xd08, 93 bytes. Select a record address by kind. */
ef_addr32 record_for_kind(int32_t kind)
{
    switch (kind) {
    case 0: return 0x0017f1c8u;
    case 6: return 0x0017f1d4u;
    case 9: return 0x0017f1e4u;
    case 10: return 0x0017f200u;
    case 11: return 0x0017f20cu;
    case 12: return 0x0017f220u;
    case 13: return 0x0017f22cu;
    case 14: return 0x0017f248u;
    default: return 0x0017f254u;
    }
}

/* 0x1ef8, 108 bytes. Every second call pick a byte from tables 0x1961f8 (stride 51) or 0x196191 (stride 52)
 * and store 0xf60 of it in byte 0x197086. */
void tick_selector_1ef8(void)
{
    uint32_t index;
    uint32_t value;
    EF_U8(0x00121195u) = (uint8_t)(EF_U8(0x00121195u) + 1u);
    if (EF_U8(0x00121195u) < 2u)
        return;
    index = EF_U8(0x00197085u);
    EF_U8(0x00121195u) = 0;
    if (index != 0xffu) {
        value = EF_U8(0x001961f8u + index * 51u);
    } else {
        value = EF_U8(0x00196191u + (uint32_t)EF_U8(0x00197087u) * 52u);
    }
    EF_U8(0x00197086u) = (uint8_t)guest_fn_f60(value & 0xffu);
}

/* 0x3cf0, 129 bytes. Reset the selection state from the record at dword 0x197061. */
void reset_selection_3cf0(void)
{
    ef_addr32 record;
    EF_U16(0x00196fecu) = 0;
    EF_U8(0x0019707du) = 1;
    EF_U8(0x00196ff0u) = 0;
    record = EF_U32(0x00197061u);
    EF_U32(0x00196ff3u) = record;
    guest_fn_9d749(0x00196ffbu, EF_U32(0x00132d34u + (uint32_t)EF_U8(record + 0xe9u) * 4u));
    EF_U16(0x00197089u) = 0x12c;
    EF_U8(0x0019707eu) = 0;
    EF_U32(0x00196ff7u) = 0;
    EF_U8(0x00197088u) = 0xffu;
    EF_U8(0x0019702du) = EF_U8(0x0017f5f8u);
    guest_fn_2fbc4(0x0017f630u);
}

/* 0x4bf8, 87 bytes. */
void select_entry_4bf8(void)
{
    int32_t index;
    ef_addr32 slot;
    guest_fn_e710();
    index = (EF_S32(0x00121e44u) >> 16) - 1;
    guest_fn_9d749(0x00125828u, (uint32_t)index * 76u + EF_U32(0x00121e40u) + 0x3au);
    guest_fn_45a4();
    slot = guest_fn_a7ec4(0x0017f6ccu);
    EF_U32(slot) = EF_U8(0x0012583bu);
    guest_fn_aca88();
}

/* 0x5f60, 120 bytes. Initialisation sequence. */
void init_sequence_5f60(void)
{
    guest_fn_5f944();
    guest_fn_121f0();
    guest_fn_12160();
    guest_fn_64fa4();
    guest_fn_65960();
    guest_fn_5f634();
    guest_fn_1f960();
    guest_fn_1f0c8();
    guest_fn_5f8a8();
    EF_U8(0x00197256u) = 1;
    EF_U8(0x00197257u) = 0;
    guest_fn_5f8d4(1);
    EF_U8(0x00197252u) = 0;
    guest_fn_6bf4();
    guest_fn_6b1c();
    guest_fn_6b50();
    guest_fn_14ef4();
    guest_fn_150d4();
    EF_U16(0x0019724eu) = 0;
    EF_U8(0x00197253u) = 0;
}

/* 0x6acc, 78 bytes. Category of an object from the byte at +0xe9: 1 (<=0xf), 2 (0x10..0x15), 3 (0x20..0x21), 4 (0x22..0x24). */
uint8_t object_category(ef_addr32 object)
{
    uint32_t type;
    if (object == 0)
        return 0;
    type = EF_U8(object + 0xe9u);
    if (type <= 0x0fu)
        return 1;
    if (type >= 0x10u && type <= 0x15u)
        return 2;
    if (type >= 0x20u && type <= 0x21u)
        return 3;
    if (type >= 0x22u && type <= 0x24u)
        return 4;
    return 0;
}

/* 0x6e18, 89 bytes. */
void reset_slots_6e18(void)
{
    uint8_t count = EF_U8(0x00121e55u);
    uint8_t i;
    EF_U8(0x0019726au) = 0;
    EF_U8(0x00121e54u) = 1;
    guest_fn_6df0(count);
    count = EF_U8(0x00121e55u);
    for (i = 0; i < count; ++i) {
        ef_addr32 slot = EF_U32(0x00197260u) + (uint32_t)i * 5u;
        EF_U16(slot + 1u) = 0xffu;
        EF_U16(slot + 3u) = 0;
    }
    EF_U8(0x00121e55u) = count;
}

/* 0x90c0, 83 bytes. */
void update_unit_90c0(int16_t index)
{
    ef_addr32 record;
    uint32_t type;
    if (guest_fn_2dba4(index) != 0)
        EF_U8(0x001f9011u) = 1;
    record = EF_U32(0x0019bb58u) + (uint32_t)(int32_t)index * 0xcau;
    type = (uint32_t)EF_U8(record + 0x19u) * 0xe5u;
    if (EF_U8(0x001285a1u + type) != 0) {
        guest_fn_8f9c((int32_t)index);
        return;
    }
    guest_fn_8fe4((int32_t)index, EF_U8(record + 0x5fu));
}

/* 0xe774, 104 bytes. Read one character from the 0x100-byte buffer at 0x197b0f; 0x1a becomes 0. */
uint8_t read_buffered_char(void)
{
    uint32_t position;
    uint8_t c;
    for (;;) {
        position = EF_U32(0x00121eb4u);
        if ((int32_t)position >= EF_S32(0x00121eb0u)) {
            uint32_t got;
            EF_U32(0x00121eb4u) = 0;
            got = guest_fn_9cde8(0x00197b0fu, 0x100u, EF_U32(0x00197ab0u));
            position = EF_U32(0x00121eb4u);
            EF_U32(0x00121eb0u) = got;
            if (got == 0)
                EF_U8(0x00197b0fu) = 0;
        }
        c = EF_U8(0x00197b0fu + position);
        ++position;
        /* ORIGINAL BUG: on a carriage return the position is reloaded from memory
         * before it has been stored, so the same byte is read again. */
        if (c == 0x0d)
            continue;
        break;
    }
    if (c == 0x1a)
        c = 0;
    EF_U32(0x00121eb4u) = position;
    return c;
}

/* 0xee0c, 77 bytes. Read a record through the 0xe95c byte/word readers. */
uint32_t read_record_ee0c(ef_addr32 record)
{
    guest_fn_e810();
    guest_fn_e89c();
    EF_U8(record) = (uint8_t)guest_fn_e95c();
    EF_U8(record + 1u) = (uint8_t)guest_fn_e95c();
    EF_U8(record + 2u) = (uint8_t)guest_fn_e95c();
    EF_U8(record + 3u) = (uint8_t)guest_fn_ed30();
    EF_U8(record + 4u) = (uint8_t)guest_fn_eaf8();
    EF_U16(record + 5u) = (uint16_t)guest_fn_e95c();
    EF_U16(record + 7u) = (uint16_t)guest_fn_e95c();
    return 1;
}

/* 0xeefc, 94 bytes. */
uint32_t read_record_eefc(ef_addr32 record)
{
    guest_fn_e810();
    guest_fn_e89c();
    guest_fn_e89c();
    EF_U8(record) = (uint8_t)guest_fn_ec8c();
    EF_U32(record + 1u) = guest_fn_e990();
    EF_U16(record + 7u) = (uint16_t)guest_fn_e95c();
    EF_U16(record + 9u) = (uint16_t)guest_fn_e95c();
    EF_U16(record + 0xbu) = (uint16_t)guest_fn_e95c();
    EF_U16(record + 0xdu) = (uint16_t)guest_fn_e95c();
    EF_U16(record + 0xfu) = (uint16_t)guest_fn_e95c();
    EF_U16(record + 0x11u) = (uint16_t)guest_fn_e95c();
    return 1;
}

/* 0xefe8, 79 bytes. */
uint32_t read_record_efe8(ef_addr32 record)
{
    guest_fn_e810();
    guest_fn_e89c();
    EF_U32(record) = guest_fn_e980();
    EF_U32(record + 4u) = guest_fn_e980();
    EF_U16(record + 8u) = (uint16_t)guest_fn_e95c();
    EF_U16(record + 0xau) = (uint16_t)guest_fn_e95c();
    EF_U16(record + 0xcu) = (uint16_t)guest_fn_e95c();
    EF_U16(record + 0xeu) = (uint16_t)guest_fn_e95c();
    EF_U8(record + 0x10u) = (uint8_t)guest_fn_ef98();
    return 1;
}

/* 0xf13c, 91 bytes. */
void read_record_f13c(ef_addr32 record)
{
    guest_fn_e810();
    guest_fn_e89c();
    EF_U8(record) = (uint8_t)guest_fn_f0ec();
    if (EF_U8(record) == 4) {
        int16_t i;
        for (i = 0; i < 0x15; ++i)
            EF_U16(record + (uint32_t)(int32_t)i * 2u + 1u) = (uint16_t)guest_fn_e95c();
    } else {
        EF_U16(record + 1u) = (uint16_t)guest_fn_e95c();
        EF_U16(record + 3u) = (uint16_t)guest_fn_e95c();
        EF_U16(record + 5u) = (uint16_t)guest_fn_e95c();
    }
    EF_U8(record + 0x2bu) = 0;
}

/* 0x117d4, 75 bytes. Is there an entry (0x2c bytes each, count word +4, array dword +0xa) with byte 0 clear
 * and byte +0x2b not 100? */
uint8_t has_free_entry_117d4(void)
{
    ef_addr32 table = EF_U32(0x001379c4u);
    uint8_t result = 0;
    if (table == 0)
        return 0;
    {
        int16_t i;
        for (i = 0; i < (int16_t)EF_U16(table + 4u); ++i) {
            ef_addr32 entry = EF_U32(table + 0xau) + (uint32_t)(int32_t)i * 0x2cu;
            if (EF_U8(entry) == 0 && EF_U8(entry + 0x2bu) != 100u) {
                result = 1;
                break;
            }
        }
    }
    EF_U32(0x001379c4u) = table;
    return result;
}

/* 0x12ec8, 80 bytes. Mirror byte 0x123790 with the flag at 0x121fb4 + 29*index. */
void sync_flag_12ec8(void)
{
    int32_t index = EF_S32(0x0012287eu) >> 16;
    uint32_t address = 0x00121fb4u + (uint32_t)index * 29u;
    if (EF_U8(0x00122882u) == 0) {
        EF_U8(0x00123790u) = EF_U8(address);
    } else {
        EF_U8(0x00123790u) = 1;
        EF_U8(address) = 1;
    }
}

/* 0x169b4, 79 bytes. */
uint32_t unit_in_state_169b4(int32_t index)
{
    uint8_t state = EF_U8(EF_U32(0x0019bb58u) + (uint32_t)index * 0xcau + 0x1bu);
    if ((uint16_t)guest_fn_16984(index) == 0)
        return 0;
    if (state == 0)
        return 1;
    return state == 0x0d || state == 2 || state == 0x0a || state == 0x0b || state == 0x0c || state == 9;
}

/* 0x16ab4, 87 bytes. Active record whose kind byte +0x1c is one of a fixed set. */
uint32_t record_kind_matches_16ab4(uint32_t index)
{
    ef_addr32 record = EF_U32(0x001a14f8u) + index * 69u;
    uint8_t kind;
    if (EF_U8(record + 0x13u) == 0)
        return 0;
    kind = EF_U8(record + 0x1cu);
    if (kind == 8 || kind == 0x0e || kind == 0x11 || kind == 0x16 || kind == 0x17 || kind == 0x1a)
        return 1;
    return kind >= 0x21 && kind <= 0x25;
}

/* 0x17480, 71 bytes. */
uint32_t cell_blocked_17480(uint32_t index, int32_t x)
{
    int32_t row = EF_S32(EF_U32(0x0019bb58u) + index * 0xcau + 0x15u) >> 16;
    int32_t column = EF_S32(0x001379cau) >> 16;
    int8_t cell = (int8_t)EF_U8(EF_U32(0x001379c8u) + (uint32_t)(column * row + x));
    if (cell > 0)
        return 0;
    return (uint8_t)guest_fn_174f8(index) != 0;
}

/* 0x1cd90, 85 bytes. Adjust word 0x12b366 by the key flags and clamp it to 0x5a..0x7f. */
void adjust_level_1cd90(void)
{
    int16_t value = (int16_t)EF_U16(0x0012b366u);
    if (EF_U8(0x0041f7a5u) != 0) {
        ++value;
        EF_U8(0x00128548u) = 1;
    }
    if (EF_U8(0x0041f7a6u) != 0) {
        --value;
        EF_U8(0x00128548u) = 1;
    }
    if (value > 0x7f)
        value = 0x7f;
    if (value < 0x5a)
        value = 0x5a;
    EF_U16(0x0012b366u) = (uint16_t)value;
}

/* 0x1d3c4, 79 bytes. */
void select_tables_1d3c4(uint16_t code)
{
    uint32_t first = 0;
    uint32_t second = 0;
    if (code == 0x30) {
        first = 0x0001a1d8u;
        second = 0x0001d3b0u;
    } else if (code == 0x46) {
        first = 0x0001a1e4u;
        second = 0x0001d3bcu;
    } else if (code == 0x47) {
        first = 0x0001a1f0u;
        second = 0x0001d3acu;
    }
    EF_U32(0x00198410u) = second;
    EF_U32(0x0019840cu) = first;
}
