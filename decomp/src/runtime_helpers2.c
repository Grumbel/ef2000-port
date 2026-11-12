/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Sound-setup, script-parser and display-mode helpers, 0xe0870..0xfc4e4. */

/* 0xe0870, 111 bytes. Fill the channel record selected by byte 0x15f07f. */
void fill_channel_e0870(void)
{
    ef_addr32 channel = EF_U32(0x0015f69cu + (uint32_t)EF_U8(0x0015f07fu) * 4u);
    EF_U8(channel + 0xau) = EF_U8(0x0015f07du);
    EF_U8(channel + 9u) = EF_U8(0x0015f07cu);
    EF_U8(channel + 4u) = 0;
    EF_U8(channel + 0xbu) = EF_U8(0x0015f07eu) != 0;
    if (EF_U8(0x0015f099u) != 0)
        EF_U8(channel + 4u) = (uint8_t)(((EF_U8(0x0015f09au) & 3u) << 6) | 1u);
    EF_U32(0x0015f0a4u) = EF_U8(0x000df2a8u + (uint32_t)EF_U8(0x0015f09au));
}

/* 0xe1194, 101 bytes. */
uint8_t setup_sound_e1194(void)
{
    fill_channel_e0870();
    guest_fn_e06ac();
    guest_fn_e1110(1);
    reset_table_15f5fd();
    guest_fn_e10a0(0x00190638u);
    guest_fn_e115c(2);
    guest_fn_e0b88(0x0015f5bdu);
    if (guest_fn_e0b88(0x0019063cu) != 0)
        return 1;
    guest_fn_e0bcc();
    guest_fn_e0b88(0x0015f5bdu);
    return guest_fn_e0b88(0x0019063cu) != 0;
}

/* 0xe1260, 87 bytes. Is any of the six devices present? */
uint8_t any_device_e1260(void)
{
    static const uint32_t devices[6] = {0x190640u, 0x190648u, 0x190654u, 0x19065cu, 0x190668u, 0x190674u};
    uint32_t i;
    for (i = 0; i < 6; ++i) {
        if ((uint8_t)guest_fn_e0b10(devices[i]) != 0)
            return 1;
    }
    return 0;
}

/* 0xea4c0, 75 bytes. Rewrite a "$...:...$" template in place. */
void rewrite_template_ea4c0(void)
{
    ef_addr32 start = find_char(0x001329ecu, 0x24);
    ef_addr32 colon;
    ef_addr32 dollar;
    if (start == 0)
        return;
    colon = find_char(start, 0x3a);
    if (colon == 0)
        return;
    guest_fn_9d749(start, colon + 2u);
    dollar = find_char(start, 0x24);
    if (dollar == 0)
        return;
    guest_fn_9d749(dollar - 1u, dollar + 1u);
}

/* 0xeb9f8, 84 bytes. */
uint32_t resolve_entry_eb9f8(uint32_t key, ef_addr32 object)
{
    ef_addr32 entry;
    uint32_t result;
    int32_t kind;
    if (object == 0)
        return 0;
    entry = guest_fn_eb520(key, object);
    if (entry == 0)
        return 0;
    result = EF_U32(entry + 8u);
    guest_fn_eb698(EF_U32(entry + 4u), object);
    kind = EF_S32(object + 0x146u) >> 16;
    if (kind == 0x524a)
        return EF_U32(object + 0x14cu) + 0x10u;
    if (kind == 0x4152)
        return EF_U32(object + 0x14cu);
    return result;
}

/* 0xed4e7, 91 bytes. Parse printf flag characters into the spec at EDX (+0x14 flags, +0x16 fill). */
void parse_format_flags(ef_addr32 text, ef_addr32 spec)
{
    EF_U8(spec + 0x14u) = 0;
    for (;;) {
        uint8_t c = EF_U8(text);
        if (c == 0x2d) {
            EF_U8(spec + 0x14u) |= 8;
        } else if (c == 0x23) {
            EF_U8(spec + 0x14u) |= 1;
        } else if (c == 0x2b) {
            EF_U8(spec + 0x14u) = (uint8_t)((EF_U8(spec + 0x14u) | 4u) & 0xfdu);
        } else if (c == 0x20) {
            if (!(EF_U8(spec + 0x14u) & 4u))
                EF_U8(spec + 0x14u) |= 2;
        } else if (c == 0x30) {
            EF_U8(spec + 0x16u) = c;
        } else {
            return;
        }
        ++text;
    }
}

/* 0xf2c78, 87 bytes. Read the next script line (after leading blanks) from dword 0x485924 into 0x485956. */
void read_script_line_f2c78(void)
{
    ef_addr32 p = EF_U32(0x00485924u);
    uint32_t i = 0;
    uint8_t c;
    while (EF_U8(p) == 9 || EF_U8(p) == 0x20)
        ++p;
    do {
        c = EF_U8(p);
        ++p;
        EF_U8(0x00485956u + i) = c;
        ++i;
    } while (EF_U8(p - 1u) != 0x0a);
    EF_U8(0x00485956u + i) = 0;
    EF_U16(0x001616eau) = 0;
    EF_U8(0x001616ecu) = (uint8_t)(EF_U8(0x001616ecu) + 1u);
    EF_U32(0x00485924u) = p;
}

/* 0xf2cd0, 131 bytes. Copy the next token of the line at 0x485956 to 0x48592c. */
void read_script_token_f2cd0(void)
{
    int16_t position = (int16_t)EF_U16(0x001616eau);
    uint32_t i = 0;
    uint8_t c;
    for (;;) {
        c = EF_U8(0x00485956u + (uint32_t)(int32_t)position);
        if (c == 0x20 || c == 0x28 || c == 9 || c == 0x2c || c == 0x29 || c == 0x3d)
            ++position;
        else
            break;
    }
    do {
        ++i;
        EF_U8(0x0048592bu + i) = EF_U8(0x00485956u + (uint32_t)(int32_t)position);
        c = EF_U8(0x0048592bu + i);
        ++position;
    } while (!(c == 0x20 || c == 9 || c == 0x0d || c == 0x2c || c == 0x29 || c == 0x3d));
    EF_U8(0x0048592bu + i) = 0;
    EF_U16(0x001616eau) = (uint16_t)position;
}

/* 0xf45d8, 104 bytes. Read one character from the buffer at 0x485e59 (see 0xe774). */
uint8_t read_buffered_char_f45d8(void)
{
    uint32_t position;
    uint8_t c;
    for (;;) {
        position = EF_U32(0x00161eb4u);
        if ((int32_t)position >= EF_S32(0x00161eb0u)) {
            uint32_t got;
            EF_U32(0x00161eb4u) = 0;
            got = guest_fn_9cde8(0x00485e59u, 0x100u, EF_U32(0x00485de4u));
            position = EF_U32(0x00161eb4u);
            EF_U32(0x00161eb0u) = got;
            if (got == 0)
                EF_U8(0x00485e59u) = 0;
        }
        c = EF_U8(0x00485e59u + position);
        ++position;
        /* ORIGINAL BUG: as in 0xe774, a carriage return re-reads the same position. */
        if (c == 0x0d)
            continue;
        break;
    }
    if (c == 0x1a)
        c = 0;
    EF_U32(0x00161eb4u) = position;
    return c;
}

/* 0xf57d4, 78 bytes. Take a node from the free list at 0x485df8 (or allocate one) and push it on the list at 0x485df4. */
ef_addr32 push_node_f57d4(void)
{
    ef_addr32 node = EF_U32(0x00485df8u);
    ef_addr32 head;
    if (node != 0) {
        ef_addr32 next = EF_U32(node + 4u);
        EF_U32(0x00485df8u) = next;
        if (next != 0)
            EF_U32(next) = 0;
    } else {
        node = guest_fn_b3f58((int16_t)guest_fn_b3db0(0x6b));
    }
    head = EF_U32(0x00485df4u);
    EF_U32(node) = 0;
    EF_U32(node + 4u) = head;
    if (head != 0)
        EF_U32(head) = node;
    EF_U32(0x00485df4u) = node;
    return node;
}

/* 0xf88e8, 78 bytes. As 0xbd6d8 for the layout with count byte +4 and array +5. */
void count_slots_f88e8(ef_addr32 object)
{
    int16_t i;
    EF_U8(object + 2u) = 0;
    EF_U8(object + 3u) = 0;
    for (i = 0; i < (int16_t)EF_U8(object + 4u); ++i) {
        ef_addr32 slot = EF_U32(object + 5u) + (uint32_t)(int32_t)i * 10u;
        if (EF_U16(slot) != 0) {
            EF_U8(object + 3u) = (uint8_t)(EF_U8(object + 3u) + 1u);
            EF_U8(object + 2u) = (uint8_t)(EF_U8(object + 2u) + EF_U8(slot + 3u));
        }
    }
}

/* 0xf8938, 80 bytes. As 0xbd688 for the layout with count byte +4 and array +5. */
void reset_slots_f8938(ef_addr32 object)
{
    int16_t i;
    EF_U8(object + 2u) = 0;
    EF_U8(object + 3u) = EF_U8(object + 4u);
    for (i = 0; i < (int16_t)EF_U8(object + 4u); ++i) {
        ef_addr32 slot = EF_U32(object + 5u) + (uint32_t)(int32_t)i * 10u;
        EF_U16(slot) = 1;
        EF_U8(object + 2u) = (uint8_t)(EF_U8(object + 2u) + EF_U8(slot + 3u));
        EF_U8(slot + 3u) = EF_U8(slot + 2u);
    }
}

/* 0xf8a78, 81 bytes. Decrement byte +3 while it exceeds 2 (modes 0 and 2) or 3 (other modes). */
uint32_t decrement_f8a78(ef_addr32 object)
{
    uint8_t mode = EF_U8(object + 9u);
    uint32_t threshold = (mode == 0 || mode == 2) ? 2u : 3u;
    if (EF_U8(object + 3u) > threshold) {
        EF_U8(object + 3u) = (uint8_t)(EF_U8(object + 3u) - 1u);
        return 1;
    }
    return 0;
}

/* 0xfc08c, 93 bytes. Map a mode code (and a flag word) to a table index. */
uint32_t map_mode_fc08c(uint8_t mode, uint16_t flag)
{
    if (mode < 0x0d)
        return 3;
    if (mode <= 0x0e)
        return 9;
    if (mode == 0x0f)
        return flag == 0 ? 7 : 0;
    if (mode == 0x10)
        return flag == 0 ? 6 : 0;
    if (mode == 0x11)
        return flag == 0 ? 8 : 0;
    if (mode == 0x19)
        return 0;
    return 3;
}

/* 0xfc478, 106 bytes. VESA call 0x4f01 for mode ECX; true when the BIOS answers 0x4f. */
uint8_t query_video_mode_fc478(uint32_t mode)
{
    uint32_t regs;
    guest_fn_b3fb0(EF_U32(0x00164610u), 0, 0x100);
    EF_U32(0x00164630u) = mode;
    EF_U32(0x00164634u) = 0x4f01;
    regs = guest_fn_604_x(EF_U32(0x00164610u));
    EF_U32(0x00164618u) = regs & 0xffffu;
    EF_U16(0x0016463au) = (uint16_t)(regs >> 16);
    guest_fn_8b8(0x10, 0x00164618u);
    return (EF_U32(0x00164634u) & 0xffffu) == 0x4f;
}

/* 0xfc4e4, 80 bytes. Find the first VESA mode in the list whose info has bit 7 set; returns dword +0x28 of its info. */
uint32_t find_linear_mode_fc4e4(void)
{
    ef_addr32 mode = EF_U32(EF_U32(0x0016460cu) + 0xeu);
    while (EF_U16(mode) != 0 && EF_U16(mode) != 0xffffu) {
        ef_addr32 info;
        query_video_mode_fc478(EF_U16(mode));
        info = EF_U32(0x00164610u);
        if ((EF_U16(info) & 0x80u) != 0)
            return EF_U32(info + 0x28u);
        mode += 2u;
        if (EF_U16(mode) == 0)
            break;
    }
    return 0;
}
