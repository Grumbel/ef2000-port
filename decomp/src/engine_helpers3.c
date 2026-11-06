/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Mouse-driver wrappers, table and string helpers, 0x9c980..0xdcc54. */

/* 0x9c980, 63 bytes. Build the default path string in buffer. */
void build_default_path_9c980(ef_addr32 buffer)
{
    EF_U8(buffer) = 0;
    if (EF_U32(0x00132aa8u) == 0)
        EF_U32(0x00132aa8u) = guest_fn_9c858_arg(0x0018b154u);
    if (EF_U32(0x00132aa8u) != 0)
        copy_string_9d749(buffer, EF_U32(0x00132aa8u));
    guest_fn_eb478(buffer);
}

/* 0xc3328, 58 bytes. Release the buffer at 0x41f3bc according to its mode byte. */
void release_mode_buffer_c3328(void)
{
    uint8_t mode = EF_U8(0x00146e3cu);

    if (mode == 1)
        guest_fn_e2af4(EF_U32(0x0041f3bcu));
    else if (mode == 2)
        guest_fn_c3f80(EF_U32(0x0041f3bcu));
    EF_U8(0x00146e3cu) = 0;
    EF_U32(0x0041f3bcu) = 0;
}

/* 0xc37ec, 55 bytes. Free the two buffers at 0x146e40/0x146e44. */
void free_buffers_c37ec(void)
{
    if (EF_U32(0x00146e40u) != 0)
        guest_fn_c3f80(EF_U32(0x00146e40u));
    if (EF_U32(0x00146e44u) != 0)
        guest_fn_c3f80(EF_U32(0x00146e44u));
    EF_U32(0x00146e40u) = 0;
    EF_U32(0x00146e44u) = 0;
}

/* 0xca5e4, 60 bytes. Clear the flag bytes of records from the start index upward. */
void clear_record_flags_ca5e4(void)
{
    uint16_t start = EF_U16(0x000abf20u);
    int32_t offset;

    EF_U16(0x0041eee0u) = start;
    if ((int32_t)start >= 0x41)
        return;
    offset = (int32_t)start * 25;
    do {
        offset += 0x19;
        EF_U8(0x00137265u + (uint32_t)offset) = 0;
    } while (offset < 0x659);
}

/* Shared walker for the three lookups below: nonzero if the entry for group has a matching byte. */
static ef_addr32 find_group_entry(ef_addr32 head_addr, uint8_t group)
{
    ef_addr32 p;

    for (p = EF_U32(head_addr); p != 0; p = EF_U32(p + 4u)) {
        if (EF_U8(p + 0xbu) == group)
            return p;
    }
    return 0;
}

/* 0xcc1ac, 61 bytes. Is the flag byte for index negative? */
uint32_t group_flag_negative_cc1ac(uint8_t group, uint32_t index, ef_addr32 head_addr)
{
    ef_addr32 p = find_group_entry(head_addr, group);

    index &= 0xffu;
    if (p == 0 || index == 10 || index == 9)
        return 0;
    return (int8_t)EF_U8(EF_U32(p + 0xcu) + index) < 0;
}

/* 0xcc1ec, 61 bytes. Is the flag byte for index zero? */
uint32_t group_flag_zero_cc1ec(uint8_t group, uint32_t index, ef_addr32 head_addr)
{
    ef_addr32 p = find_group_entry(head_addr, group);

    index &= 0xffu;
    if (p == 0 || index == 10 || index == 9)
        return 0;
    return EF_U8(EF_U32(p + 0xcu) + index) == 0;
}

/* 0xcc22c, 61 bytes. Is the flag byte for index positive? */
uint32_t group_flag_positive_cc22c(uint8_t group, uint32_t index, ef_addr32 head_addr)
{
    ef_addr32 p = find_group_entry(head_addr, group);

    index &= 0xffu;
    if (p == 0 || index == 10 || index == 9)
        return 0;
    return (int8_t)EF_U8(EF_U32(p + 0xcu) + index) > 0;
}

/* 0xcd20c, 63 bytes. Store value into the output table and step the script cursor. */
uint32_t script_store_step_cd20c(uint32_t value)
{
    ef_addr32 cursor = EF_U32(0x0015b628u);
    int32_t slot = EF_S16(cursor + 4u);
    uint16_t next;

    EF_U16(EF_U32(0x0041f700u) + (uint32_t)(slot * 2)) = (uint16_t)value;
    cursor += (uint32_t)((int32_t)EF_S16(cursor) * 2);
    next = EF_U16(cursor);
    EF_U32(0x0015b628u) = cursor;
    return next != 0;
}

/* 0xcd5f4, 69 bytes. Pop the first byte of the 20-byte queue at 0x41f810, -1 if empty. */
int32_t queue_pop_cd5f4(void)
{
    int32_t first;
    int16_t i;

    if (EF_U16(0x0041f830u) == 0)
        return -1;
    first = EF_U8(0x0041f810u);
    for (i = 1; i < 0x14; i++)
        EF_U8(0x0041f80fu + (uint32_t)i) = EF_U8(0x0041f810u + (uint32_t)i);
    EF_U16(0x0041f830u) = (uint16_t)(EF_U16(0x0041f830u) - 1u);
    return first;
}

/* 0xcdf60, 72 bytes. Query the mouse driver once and cache the button count. */
uint32_t mouse_button_count_cdf60(void)
{
    if (EF_U8(0x0015b760u) == 0) {
        EF_U32(0x004201ccu) = 0;
        EF_U32(0x004201c0u) = 0;
        guest_fn_8b8(0x33, 0x004201b0u);
        if (EF_U32(0x004201ccu) != 0)
            EF_U16(0x0015b762u) = (uint16_t)(EF_U8(0x004201c0u) & 3u);
    }
    return EF_U16(0x0015b762u);
}

/* 0xce02c, 54 bytes. Mouse function 0xb: read motion counters into two outputs. */
void mouse_read_motion_ce02c(ef_addr32 out_x, ef_addr32 out_y)
{
    EF_U32(0x004201ccu) = 0xb;
    guest_fn_8b8(0x33, 0x004201b0u);
    EF_U16(out_x) = EF_U16(0x004201c8u);
    EF_U16(out_y) = EF_U16(0x004201c4u);
}

/* 0xce064, 39 bytes. Mouse function 3: button state. */
uint32_t mouse_read_buttons_ce064(void)
{
    EF_U32(0x004201ccu) = 3;
    guest_fn_8b8(0x33, 0x004201b0u);
    return EF_U8(0x004201c0u) & 7u;
}

/* 0xce328, 44 bytes. Latch four values and the current pair at 0x15b744/0x15b748. */
void latch_values_ce328(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    EF_U32(0x0015bdc0u) = a;
    EF_U32(0x0015bdc4u) = b;
    EF_U32(0x0015b76cu) = EF_U32(0x0015b744u);
    EF_U32(0x0015bdc8u) = c;
    EF_U32(0x0015b770u) = EF_U32(0x0015b748u);
    EF_U32(0x0015bdccu) = d;
}

/* 0xce944, 93 bytes. Apply pending event bits of an object through the 5-entry table at 0x15b798. */
ef_addr32 apply_pending_events_ce944(ef_addr32 object)
{
    uint32_t i;

    EF_U32(0x004208b0u) = 0x004201f0u;
    EF_U32(0x004208acu) = 0x00420508u;
    for (i = 0; i < 0x28u; i += 8u) {
        uint32_t mask = EF_U32(0x0015b798u + i);

        if ((mask & EF_U32(object + 0x2cu)) != 0) {
            guest_fn_ce830(object, EF_U32(0x0015b79cu + i));
            EF_U32(object + 0x2cu) = EF_U32(object + 0x2cu) & ~EF_U32(0x0015b798u + i);
            if (EF_S32(EF_U32(object + 0x4cu)) < 3)
                break;
        }
    }
    return object;
}

/* 0xcf78c, 53 bytes. Forward the five words of the record at 0x4208e4 to cf3a4. */
void forward_record_cf78c(void)
{
    ef_addr32 r = 0x004208e4u;

    guest_fn_cf3a4(EF_U16(r + 2u), EF_U16(r + 4u), EF_U16(r + 6u), EF_U16(r + 8u), EF_U16(r));
}

/* 0xd0858, 55 bytes. Copy a 9-dword record. */
void copy_record9_d0858(ef_addr32 destination, ef_addr32 source)
{
#if defined(__WATCOMC__)
    volatile uint32_t *dst = (volatile uint32_t *)(uintptr_t)destination;
    volatile uint32_t *src = (volatile uint32_t *)(uintptr_t)source;
    uint32_t row, column;

    for (row = 0; row < 3u; ++row)
        for (column = 0; column < 3u; ++column)
            dst[column * 3u + row] = src[column * 3u + row];
#else
    EF_U32(destination) = EF_U32(source);
    EF_U32(destination + 0xcu) = EF_U32(source + 0xcu);
    EF_U32(destination + 0x18u) = EF_U32(source + 0x18u);
    EF_U32(destination + 4u) = EF_U32(source + 4u);
    EF_U32(destination + 0x10u) = EF_U32(source + 0x10u);
    EF_U32(destination + 0x1cu) = EF_U32(source + 0x1cu);
    EF_U32(destination + 8u) = EF_U32(source + 8u);
    EF_U32(destination + 0x14u) = EF_U32(source + 0x14u);
    EF_U32(destination + 0x20u) = EF_U32(source + 0x20u);
#endif
}

/* 0xd0b68, 63 bytes. verified: exact-body+replay. Reset a 9-dword bounds record (0x7fff limits). */
void init_bounds_d0b68(ef_addr32 record)
{
#if defined(__WATCOMC__)
    volatile uint32_t *bounds = (volatile uint32_t *)(uintptr_t)record;

    bounds[3] = 0;
    bounds[6] = 0;
    bounds[1] = 0;
    bounds[7] = 0;
    bounds[2] = 0;
    bounds[5] = 0;
    bounds[0] = 0x7fffu;
    bounds[4] = 0x7fffu;
    bounds[8] = 0x7fffu;
#else
    EF_U32(record + 0xcu) = 0;
    EF_U32(record + 0x18u) = 0;
    EF_U32(record + 4u) = 0;
    EF_U32(record + 0x1cu) = 0;
    EF_U32(record + 8u) = 0;
    EF_U32(record + 0x14u) = 0;
    EF_U32(record) = 0x7fff;
    EF_U32(record + 0x10u) = 0x7fff;
    EF_U32(record + 0x20u) = 0x7fff;
#endif
}

/* 0xd65a8, 62 bytes. Remap a 256-entry RGB palette through three 64-byte tables. */
void remap_palette_d65a8(ef_addr32 destination, ef_addr32 source)
{
    uint32_t i;

    for (i = 0; i < 0x100u; i++) {
        EF_U8(destination + i * 3u) = EF_U8(0x0015b96cu + EF_U8(source + i * 3u));
        EF_U8(destination + i * 3u + 1u) = EF_U8(0x0015b9acu + EF_U8(source + i * 3u + 1u));
        EF_U8(destination + i * 3u + 2u) = EF_U8(0x0015b9ecu + EF_U8(source + i * 3u + 2u));
    }
}

/* 0xd6dc4, 43 bytes. One-time module init; registers the exit handler at 0xd6da0. */
void init_module_d6dc4(void)
{
    if (EF_U8(0x0015bac5u) == 0) {
        guest_fn_101c54();
        EF_U8(0x0015bac5u) = 1;
        guest_fn_d6df0();
        push_handler_9c3cf(0xd6da0u);
    }
}

/* 0xd7040, 47 bytes. Build "<default path><first><second>" with a fixed separator. */
void build_path_d7040(ef_addr32 buffer, ef_addr32 second)
{
    build_default_path_9c980(buffer);
    guest_fn_9ca57(buffer, 0x0018f984u);
    guest_fn_9ca57(buffer, second);
    guest_fn_9ca57(buffer, 0x0018f990u);
}

/* 0xdcc10, 67 bytes. Reset the 320x200 clip state. */
void reset_clip_dcc10(void)
{
    guest_fn_cdf48(0, 0x13f, 0, 0xc7);
    EF_U8(0x0015ee5cu) = 0;
    EF_U16(0x00425fa4u) = 1;
    EF_U16(0x0015ee5au) = 3;
    EF_U16(0x00425fa6u) = 0;
}

/* 0xdcc54, 67 bytes. Same as dcc10 with the stores in a different order. */
void reset_clip_dcc54(void)
{
    guest_fn_cdf48(0, 0x13f, 0, 0xc7);
    EF_U16(0x00425fa4u) = 1;
    EF_U16(0x0015ee5au) = 3;
    EF_U8(0x0015ee5cu) = 0;
    EF_U16(0x00425fa6u) = 0;
}
