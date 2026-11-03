/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Game-state helpers, 0x1d700..0x39dc4. */

/* 0x1d700, 120 bytes. Dispatch on the kind byte +0x41 of the 0x171-byte record number AX. */
void update_record_1d700(int16_t index)
{
    ef_addr32 record = EF_U32(0x0019af7cu) + (uint32_t)(int32_t)index * 0x171u;
    uint8_t kind = EF_U8(record + 0x41u);
    if (kind == 0) {
        guest_fn_1d778(record);
    } else if (kind == 1) {
        guest_fn_1d8a8(record);
        guest_fn_1d7a4(record);
        guest_fn_1d778(record);
    } else if (kind == 8) {
        guest_fn_1d954(record);
    } else if (kind == 0x0c || kind == 0x10) {
        guest_fn_1d954(record);
        if ((uint8_t)guest_fn_1d8a8(record) != 0)
            guest_fn_1d778(record);
    }
}

/* 0x1f878, 74 bytes. Push an entry onto the ten-entry stack at 0x19b0d0 and request an event. */
void push_event_1f878(uint32_t entry)
{
    int32_t index = EF_S32(0x0019b112u) >> 16;
    if (index >= 10)
        return;
    EF_U32(0x0019b0d0u + (uint32_t)index * 4u) = entry;
    EF_U16(0x0019b114u) = (uint16_t)(EF_U16(0x0019b114u) + 1u);
    if ((uint8_t)guest_fn_121cc(9, 2) == 0)
        guest_fn_12d90(9, 2);
}

/* 0x27858, 104 bytes. Show two panels and wait for key 0x15 or 0x31; true for 0x15. */
uint8_t confirm_dialog_27858(void)
{
    guest_fn_dd370(0, 0x6b, 0x87, 0x001821ecu);
    guest_fn_dd370(0x9c, 0x6a, 0x86, 0x00182224u);
    guest_fn_dc250();
    EF_U8(0x0041f833u) = 0;
    while (*(volatile uint8_t *)ef_guest_memory_ptr(0x0041f833u) != 0x15u &&
           *(volatile uint8_t *)ef_guest_memory_ptr(0x0041f833u) != 0x31u) {
    }
    return EF_U8(0x0041f833u) == 0x15u;
}

/* 0x28830, 90 bytes. Reset the ten 14-byte entries at 0x19b8d0 and chain them. */
void init_chain_28830(void)
{
    uint32_t previous = EF_U32(0x0019b8ccu);
    int16_t i;
    EF_U16(0x0019b8c0u) = (uint16_t)guest_fn_de864(0x001823ccu);
    EF_U16(0x0019b8c2u) = (uint16_t)guest_fn_de864(0x001823d8u);
    previous = 0;
    EF_U32(0x0019b8c4u) = 0;
    EF_U32(0x0019b8c8u) = 0;
    for (i = 0; i < 10; ++i) {
        uint32_t entry = (uint32_t)(int32_t)i * 14u + 0x0019b8d0u;
        EF_U32(entry + 0xau) = previous;
        previous = entry;
    }
    EF_U32(0x0019b8ccu) = previous;
}

/* 0x2a168, 92 bytes. */
void refresh_object_2a168(ef_addr32 object)
{
    if ((uint8_t)guest_fn_2949c(EF_S32(object + 0x14u) >> 16, 0x1d) != 0x14)
        guest_fn_2a1fc(EF_S32(object + 0x14u) >> 16, 0);
    if ((uint8_t)guest_fn_2949c(EF_S32(object + 0x14u) >> 16, 0x1d) != 0x14)
        guest_fn_2a1fc(EF_S32(object + 0x14u) >> 16, 0);
    guest_fn_6b54c(object);
}

/* 0x2a720, 113 bytes. Reset counters and link the twenty 22-byte entries at 0x19b980. */
void init_entries_2a720(void)
{
    int16_t i;
    EF_U32(0x00125250u) = 0;
    EF_U32(0x00125254u) = 0;
    EF_U32(0x0019bb40u) = 0;
    EF_U32(0x0019bb3cu) = 0;
    EF_U32(0x0019bb38u) = 0;
    for (i = 0; i < 0x14; ++i) {
        uint32_t entry = (uint32_t)(int32_t)i * 0x16u + 0x0019b980u;
        if (EF_U16(0x00425bc4u) & 1u)
            EF_U16(entry + 8u) = (uint16_t)guest_fn_9ddfc(0x001824d4u, entry);
        EF_U32(entry + 0x12u) = EF_U32(0x0019bb40u);
        EF_U32(0x0019bb40u) = entry;
    }
}

/* 0x2a7ac, 74 bytes. As 0x2a720 but for string EAX into the word at +8+2*EDX. */
void init_entries_2a7ac(ef_addr32 text, uint32_t slot)
{
    int16_t i;
    for (i = 0; i < 0x14; ++i) {
        uint32_t entry = (uint32_t)(int32_t)i * 0x16u + 0x0019b980u;
        EF_U16(entry + slot * 2u + 8u) = (uint16_t)guest_fn_9ddfc(text, entry);
        EF_U32(entry + 0x12u) = EF_U32(0x0019bb40u);
        EF_U32(0x0019bb40u) = entry;
    }
}

/* 0x316b8, 84 bytes. Run 0x32414 and add the three deltas at 0x19e560 to the position at 0x19e5c1. */
void advance_position_316b8(void)
{
    guest_fn_32414(0x0019e56cu, 0x0019e57eu, 0x0019e5c1u);
    EF_U32(0x0019e5c1u) = EF_U32(0x0019e5c1u) + EF_U32(0x0019e560u);
    EF_U32(0x0019e5c5u) = EF_U32(0x0019e5c5u) + EF_U32(0x0019e564u);
    EF_U32(0x0019e5c9u) = EF_U32(0x0019e5c9u) + EF_U32(0x0019e568u);
}

/* 0x31aec, 84 bytes. Identical to 0x316b8. */
void advance_position_31aec(void)
{
    guest_fn_32414(0x0019e56cu, 0x0019e57eu, 0x0019e5c1u);
    EF_U32(0x0019e5c1u) = EF_U32(0x0019e5c1u) + EF_U32(0x0019e560u);
    EF_U32(0x0019e5c5u) = EF_U32(0x0019e5c5u) + EF_U32(0x0019e564u);
    EF_U32(0x0019e5c9u) = EF_U32(0x0019e5c9u) + EF_U32(0x0019e568u);
}

/* 0x31d10, 108 bytes. Update the angle words from mode byte 0x19e55f. */
void update_angles_31d10(void)
{
    uint8_t mode = EF_U8(0x0019e55fu);
    if (mode == 5) {
        EF_U16(0x0019e5d7u) = EF_U16(0x0019e570u);
        EF_U16(0x0019e5d1u) = EF_U16(0x0019e570u);
    }
    if (EF_U8(0x0019e55fu) == 4)
        EF_U16(0x0019e5d7u) = EF_U16(0x0019e570u);
    if (EF_U8(0x0019e55fu) == 3)
        EF_U16(0x0019e5d7u) = 0;
    if (EF_U8(0x0019e55fu) == 0 && EF_U8(0x0019e557u) == 0)
        EF_U16(0x0019e5d1u) = EF_U16(0x0019e5d7u);
}

/* 0x32260, 75 bytes. Save the current position and angles. */
void save_position_32260(void)
{
    EF_U32(0x0019e5b5u) = EF_U32(0x0019e5c1u);
    EF_U32(0x0019e5b9u) = EF_U32(0x0019e5c5u);
    EF_U32(0x0019e5bdu) = EF_U32(0x0019e5c9u);
    EF_U16(0x0019e5cdu) = EF_U16(0x0019e5d3u);
    EF_U16(0x0019e5cfu) = EF_U16(0x0019e5d5u);
    EF_U16(0x0019e5d1u) = EF_U16(0x0019e5d7u);
    EF_U8(0x0019e5e3u) = 0;
}

/* 0x34390, 76 bytes. */
void show_notice_34390(void)
{
    ef_addr32 dialog;
    if (EF_U8(0x0019f5aeu) != 0 || EF_U8(0x00125902u) != 0)
        return;
    guest_fn_e1730();
    if (EF_U32(0x00428cf4u) != 0) {
        guest_fn_a4d90(0x00182db8u);
        return;
    }
    dialog = guest_fn_a46cc(0x00182df0u);
    guest_fn_a4cb4(EF_U8(dialog + 0x10u));
    guest_fn_ac9a0(dialog);
}

/* 0x343dc, 109 bytes. Check both data files; show an error dialog and return 0 on failure. */
uint8_t check_files_343dc(void)
{
    ef_addr32 dialog;
    if (guest_fn_a4380(0x00182b34u, 0x00125937u) == 0) {
        dialog = guest_fn_a46cc(0x00182dd0u);
        guest_fn_a4cb4(EF_U8(dialog + 0x10u));
        guest_fn_ac9a0(dialog);
        return 0;
    }
    if (guest_fn_a4380(0x00182b34u, 0x0019f566u) == 0) {
        dialog = guest_fn_a46cc(0x00182e00u);
        guest_fn_a4cb4(EF_U8(dialog + 0x10u));
        guest_fn_ac9a0(dialog);
        return 0;
    }
    return 1;
}

/* 0x3458c, 71 bytes. Compact the four-entry dword table at 0x13577c: an empty entry shifts the later ones down. */
void compact_table_3458c(void)
{
    int32_t i;
    for (i = 0; i < 4; ++i) {
        if (EF_U32(0x0013577cu + (uint32_t)i * 4u) == 0) {
            if (i < 3) {
                uint32_t offset = (uint32_t)i * 4u + 4u;
                do {
                    EF_U32(0x00135778u + offset) = EF_U32(0x0013577cu + offset);
                    offset += 4u;
                } while ((int32_t)offset < 0xc);
            }
            EF_U32(0x00135788u) = 0;
        }
    }
}

/* 0x34f30, 97 bytes. */
void scan_entries_34f30(void)
{
    uint32_t i;
    for (i = 0; i < EF_U8(EF_U32(0x00136b78u)); ++i) {
        uint32_t type = EF_U8(EF_U32(0x00136b74u) + i);
        uint16_t state = EF_U16(EF_U32(0x00135878u + (type << 5)) + 7u);
        if (state == 5) {
            guest_fn_ac7a8(0x00182f2cu);
            EF_U8(0x0019f5a4u) = 1;
            guest_fn_35218();
        } else if (state == 7) {
            guest_fn_34d94(0);
        }
    }
}

/* 0x35144, 85 bytes. */
void reset_flags_35144(void)
{
    EF_U8(0x0019f5a9u) = 0;
    EF_U8(0x0019f5a6u) = 0x43;
    EF_U8(0x0019f5a7u) = 0;
    EF_U8(0x0019f5abu) = 0;
    EF_U8(0x0019f59eu) = 0;
    EF_U8(0x0019f5a3u) = 0;
    EF_U8(0x0019f5a1u) = 0;
    EF_U8(0x001f6f4fu) = 0;
    EF_U8(0x0019f5a4u) = 1;
    EF_U8(0x00125839u) = 1;
    EF_U8(0x00127da9u) = 1;
    guest_fn_a4d90(0x00182f4cu);
}

/* 0x35ab8, 113 bytes. */
void select_screen_35ab8(void)
{
    uint8_t mode = EF_U8(0x00125846u);
    uint32_t text;
    if (mode == 2) {
        guest_fn_e1e6c(0x00130fc8u, 0);
    } else if (mode == 1) {
        uint8_t variant = EF_U8(0x0012584bu);
        if (variant == 1)
            text = 0x00132137u;
        else if (variant == 2)
            text = 0x00132155u;
        else
            text = 0x00132171u;
        guest_fn_e1e6c(text, 0);
        EF_U16(0x00125835u) = EF_U8(0x001a666bu);
        EF_U8(0x00125837u) = 0;
        EF_U8(0x00125838u) = 0;
    }
}

/* 0x35bb8, 78 bytes. */
void copy_counts_35bb8(void)
{
    EF_U8(0x0012583au) = EF_U8(0x001357b0u);
    EF_U8(0x00125839u) = 1;
    if (EF_U8(0x00125846u) != 1)
        EF_U8(0x00125838u) = EF_U8(0x001357aeu);
    EF_U8(0x001a666bu) = EF_U8(0x001357aeu);
    EF_U8(0x00127da9u) = 1;
    if (EF_U8(0x00125846u) == 1)
        EF_U8(0x00127da8u) = 1;
}

/* 0x363c4, 80 bytes. Walk the list from dword 0x19f320 (next at +0x21), toggling each node by the flag bytes at 0x19f318. */
void sync_list_363c4(void)
{
    ef_addr32 node = EF_U32(0x0019f320u);
    int32_t i;
    for (i = 0; i < (EF_S32(0x001357aeu) >> 16); ++i) {
        if (EF_U8(0x0019f318u + (uint32_t)i) == 0) {
            if (!(EF_U8(node + 0x1du) & 1u))
                guest_fn_a72d8(node);
        } else if (EF_U8(node + 0x1du) & 1u) {
            guest_fn_a7380(node);
        }
        node = EF_U32(node + 0x21u);
    }
}

/* 0x37d3c, 76 bytes. True when 0x37cdc, or 0x37c64 for any subitem, accepts the index. */
uint8_t any_accepts_37d3c(int16_t index)
{
    uint8_t result = (uint8_t)((uint8_t)guest_fn_37cdc((int32_t)index) != 0);
    if (EF_U8(0x00127da9u) != 0) {
        uint8_t i;
        for (i = 0; i < EF_U8(0x0012583au); ++i) {
            if ((uint8_t)guest_fn_37c64((int32_t)index, i) != 0)
                result = 1;
        }
    }
    return result;
}

/* 0x37e60, 76 bytes. True when 0x37e00 and 0x37d88 for every subitem accept the index. */
uint8_t all_accept_37e60(int16_t index)
{
    uint8_t result = 1;
    if ((uint8_t)guest_fn_37e00((int32_t)index) == 0)
        result = 0;
    if (EF_U8(0x00127da9u) != 0) {
        uint8_t i;
        for (i = 0; i < EF_U8(0x0012583au); ++i) {
            if ((uint8_t)guest_fn_37d88((int32_t)index, i) == 0)
                result = 0;
        }
    }
    return result;
}

/* 0x39204, 78 bytes. */
void redraw_screen_39204(void)
{
    guest_fn_d1e50(2);
    guest_fn_d65e8();
    guest_fn_c3990();
    if (EF_U8(0x0012ddb4u) <= 2)
        guest_fn_c34cc(0x00183504u);
    guest_fn_dc250();
    guest_fn_de580(0x00425c8cu);
    guest_fn_391d0();
}

/* 0x394e0, 104 bytes. Consume the pending request at 0x1a3608 and apply it through 0x1d5cc. */
void apply_pending_394e0(void)
{
    int32_t x, y;
    uint32_t type, size;
    int16_t value;
    uint32_t extra;
    if (EF_U8(0x001a3608u) == 0)
        return;
    x = EF_S32(0x001a360bu) >> 16;
    y = EF_S32(0x001a3613u) >> 16;
    size = EF_U16(0x001a3611u);
    EF_U8(0x001a3608u) = 0;
    type = EF_U16(0x001a3609u);
    value = (int16_t)guest_fn_39744(y, type, x, size);
    if (value >= 0x70 && value <= 0x8f)
        extra = EF_U32(0x00425bd8u);
    else
        extra = EF_U32(0x00425bdau);
    guest_fn_1d5cc(0x001a3609u, (int32_t)extra >> 16);
}

/* 0x39dc4, 72 bytes. Reset the ten-entry table at dword 0x1a1860. */
void reset_table_39dc4(void)
{
    uint8_t count;
    uint8_t i;
    EF_U8(0x001a1864u) = 10;
    guest_fn_39db0(10);
    count = EF_U8(0x001a1864u);
    for (i = 0; i < count; ++i) {
        ef_addr32 table = EF_U32(0x001a1860u);
        EF_U16(table + (uint32_t)i * 4u + 2u) = 0xffu;
        EF_U16(table + (uint32_t)i * 4u) = 0;
    }
    EF_U8(0x001a1864u) = count;
}
