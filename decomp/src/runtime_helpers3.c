/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Heap-block, serial-port, trig-table and MIDI helpers, 0x10d5c7..0x11f4d8. */


/* 0x10f071, 119 bytes. Allocate the buffer of a stream record. */
ef_addr32 alloc_stream_buffer_10f071(ef_addr32 stream)
{
    uint8_t flags;

    update_flags_fa7d2(stream);
    if (EF_U32(stream + 0x14u) == 0) {
        flags = EF_U8(stream + 0xdu);
        if (flags & 2u)
            EF_U32(stream + 0x14u) = 0x86;
        else if (flags & 4u)
            EF_U32(stream + 0x14u) = 1;
        else
            EF_U32(stream + 0x14u) = 0x1000;
    }
    EF_U32(stream + 8u) = guest_fn_fc795(EF_U32(stream + 0x14u));
    if (EF_U32(stream + 8u) == 0) {
        EF_U32(stream + 0x14u) = 1;
        EF_U32(stream + 8u) = stream + 0x18u;
        EF_U8(stream + 0xdu) = (uint8_t)((EF_U8(stream + 0xdu) & 0xf8u) | 4u);
    } else {
        EF_U8(stream + 0xcu) |= 8u;
    }
    EF_U32(stream + 4u) = 0;
    EF_U32(stream) = EF_U32(stream + 8u);
    return EF_U32(stream + 8u);
}

/* 0x111d98, 88 bytes. Number of data bytes of a MIDI status byte (cdecl). */
uint32_t midi_data_length_111d98(uint32_t status)
{
    switch (status & 0xf0u) {
    case 0x80u: case 0x90u: case 0xa0u: case 0xb0u: case 0xe0u:
        return 3;
    case 0xc0u: case 0xd0u:
        return 2;
    default:
        return 0;
    }
}

/* 0x111f50, 52 bytes. Byte-swap a dword (cdecl). */
uint32_t byte_swap_111f50(uint32_t value)
{
    return ((value & 0xffu) << 24) + ((value & 0xff00u) << 8) +
           ((value & 0xff0000u) >> 8) + (value >> 24);
}

/* 0x113c24, 86 bytes. Copy a path to 0x17e1d8 and cut the extension (cdecl). */
void strip_extension_113c24(ef_addr32 path)
{
    uint32_t i;
    uint32_t length;

    guest_fn_9d749(0x0017e1d8u, path);
    length = guest_fn_b1b8b(0x0017e1d8u);
    for (i = length - 1u; i != 0; i--) {
        uint8_t c = EF_U8(0x0017e1d8u + i);

        if (c == 0x5c) {
            length = guest_fn_b1b8b(0x0017e1d8u) - 1u;
            if (i == length)
                EF_U8(0x0017e1d8u + length) = 0;
            return;
        }
        if (c == 0x2e) {
            EF_U8(0x0017e1d8u + i) = 0;
            return;
        }
    }
}

/* 0x114994, 52 bytes. Record slot b of owner a as value c-1 (cdecl). */
void set_owner_slot_114994(ef_addr32 owner, uint32_t slot, uint32_t index)
{
    ef_addr32 entry;

    if (owner == 0)
        return;
    EF_U32(owner + 0x90u + slot * 4u) = index - 1u;
    entry = EF_U32(owner) + index * 4u;
    if (EF_U32(entry + 0x1cu) == 1u && EF_U32(entry + 0x5cu) != owner)
        EF_U32(entry + 0x5cu) = owner;
}

/* 0x117ea8, 44 bytes. Free the buffer of a record and clear the record. */
void clear_buffered_record_117ea8(ef_addr32 record)
{
    ef_addr32 buffer = EF_U32(record + 4u);

    if (buffer != 0) {
        guest_fn_fa9d2(buffer);
        EF_U32(record + 4u) = 0;
    }
    guest_fn_b3fb0(record, 0, 10);
}

/* 0x117f14, 82 bytes. Allocate the 0x1a-byte state block of a serial record. */
uint32_t alloc_serial_state_117f14(ef_addr32 record)
{
    ef_addr32 state = guest_fn_e8733(1, 0x1a);

    EF_U32(record + 0x2cu) = state;
    if (state == 0)
        return 0;
    EF_U16(state + 2u) = EF_U16(record + 0x1cu);
    EF_U16(state + 4u) = 0;
    guest_fn_b3fb0(state + 6u, 0, 10);
    guest_fn_b3fb0(state + 0x10u, 0, 10);
    return 1;
}

/* 0x1184c4, 92 bytes. Pop one byte from the ring buffer of a serial record, -1 if empty. */
int32_t ring_pop_1184c4(ef_addr32 record)
{
    ef_addr32 ring;
    uint32_t value;

    if (EF_U32(record + 0x1cu) == 0)
        return -1;
    ring = EF_U32(record + 0xau);
    if (EF_S16(ring + 4u) <= 0)
        return -1;
    guest_fn_11f2d3();
    value = EF_U8(EF_U32(record + 0xeu) + (uint32_t)(int32_t)EF_S16(ring));
    EF_U16(ring) = (uint16_t)(EF_U16(ring) + 1u);
    if (EF_U16(ring) == EF_U16(ring + 6u))
        EF_U16(ring) = 0;
    EF_U16(ring + 4u) = (uint16_t)(EF_U16(ring + 4u) - 1u);
    guest_fn_11f2d5();
    return (int32_t)value;
}

/* 0x1185ec, 117 bytes. Reset both ring buffers of a serial record and quiesce the port. */
void serial_reset_1185ec(ef_addr32 record)
{
    ef_addr32 ring;
    ef_addr32 state;
    uint32_t port = EF_U32(record + 0x1cu);

    if (port == 0)
        return;
    guest_fn_11f2d3();
    ring = EF_U32(record);
    EF_U16(ring + 4u) = 0;
    EF_U16(ring) = 0;
    EF_U16(ring + 2u) = 0;
    ring = EF_U32(record + 0xau);
    EF_U16(ring + 4u) = 0;
    EF_U16(ring) = 0;
    EF_U16(ring + 2u) = 0;
    state = EF_U32(record + 0x2cu);
    EF_U16(state + 4u) = (uint16_t)(EF_U16(state + 4u) & 0xfffdu);
    (void)guest_fn_9c430(EF_U32(record + 0x1cu) + 5u);
    (void)guest_fn_9c430(EF_U32(record + 0x1cu) + 6u);
    (void)guest_fn_9c430(EF_U32(record + 0x1cu) + 2u);
    (void)guest_fn_9c430(EF_U32(record + 0x1cu));
    guest_fn_11f2d5();
}

/* 0x118664, 47 bytes. Set or clear the hardware-flow flag of a serial record. */
void serial_set_flow_118664(ef_addr32 record, uint32_t enable)
{
    ef_addr32 state;

    if (enable != 0) {
        EF_U32(record + 0x14u) = 1;
        state = EF_U32(record + 0x2cu);
        EF_U16(state + 4u) = (uint16_t)(EF_U16(state + 4u) | 1u);
    } else {
        EF_U32(record + 0x14u) = enable;
        state = EF_U32(record + 0x2cu);
        EF_U16(state + 4u) = (uint16_t)(EF_U16(state + 4u) & 0xfffeu);
    }
}

/* 0x118694, 53 bytes. Set or clear bit 0 of the modem control register. */
void serial_set_dtr_118694(ef_addr32 record, uint32_t enable)
{
    uint32_t port = EF_U32(record + 0x1cu);
    uint32_t value;

    if (port == 0)
        return;
    value = guest_fn_9c430(port + 4u);
    value = enable != 0 ? (value | 1u) & 0xffu : (value & 0xfeu);
    guest_fn_117b0c(EF_U32(record + 0x1cu) + 4u, value);
}

/* 0x1186cc, 53 bytes. Set or clear bit 1 of the modem control register. */
void serial_set_rts_1186cc(ef_addr32 record, uint32_t enable)
{
    uint32_t port = EF_U32(record + 0x1cu);
    uint32_t value;

    if (port == 0)
        return;
    value = guest_fn_9c430(port + 4u);
    value = enable != 0 ? (value | 2u) & 0xffu : (value & 0xfdu);
    guest_fn_117b0c(EF_U32(record + 0x1cu) + 4u, value);
}

/* 0x11c020, 76 bytes. Allocate and clear n records of 0xd5 bytes at 0x4ddee0. */
void alloc_records_11c020(uint32_t count)
{
    uint32_t size;

    count &= 0xffu;
    if (EF_U8(0x0017e5d8u) != 0)
        free_records_11c06c();
    size = count * 0xd5u;
    EF_U32(0x004ddee0u) = guest_fn_c3f70(size);
    guest_fn_b3fb0(EF_U32(0x004ddee0u), 0, size);
    EF_U8(0x0017e5d8u) = 1;
}

/* 0x11c06c, 56 bytes. Free the five records at 0x4ddee0 and their 0x69 sub-buffers. */
void free_records_11c06c(void)
{
    int16_t i;

    for (i = 0; i < 5; i++) {
        ef_addr32 sub = EF_U32(EF_U32(0x004ddee0u) + (uint32_t)i * 0xd5u + 0x69u);

        if (sub != 0)
            guest_fn_c3f80(sub);
    }
    guest_fn_c3f80(EF_U32(0x004ddee0u));
}

/* 0x11f308, 79 bytes. Interpolated table lookup (6-bit fraction, 0x17ea2c table), result >> 14. */
int32_t trig_lookup_11f308(int32_t x)
{
    uint32_t ax = x < 0 ? (uint32_t)-x : (uint32_t)x;
    int32_t index = (int32_t)ax >> 6;
    int32_t result;

    if (index >= 0x100) {
        result = EF_S32(0x0017ee2cu);
    } else {
        int32_t lo = EF_S32(0x0017ea2cu + (uint32_t)index * 4u);
        int32_t hi = EF_S32(0x0017ea30u + (uint32_t)index * 4u);
        int64_t product = (int64_t)(hi - lo) * (int32_t)(ax & 0x3fu);

        result = (int32_t)(product >> 6) + (int32_t)((product >> 5) & 1) + lo;
    }
    result >>= 14;
    return x < 0 ? -result : result;
}

/* 0x11f460, 117 bytes. Table lookup of x * 2^14 / 0x6487 (0x17e628 table), result >> 14. */
int32_t trig_lookup_11f460(int32_t x)
{
    int32_t q = (int32_t)(((int64_t)x << 14) / 0x6487);
    int32_t ax;
    int32_t index;
    int32_t result;

    if (x < 0)
        q = -q;
    ax = q;
    index = ax >> 6;
    if (index >= 0x100) {
        result = EF_S32(0x0017ea28u);
    } else {
        int32_t lo = EF_S32(0x0017e628u + (uint32_t)index * 4u);
        int32_t hi = EF_S32(0x0017e62cu + (uint32_t)index * 4u);
        int64_t product = (int64_t)(hi - lo) * (q & 0x3f);

        result = (int32_t)(product >> 6) + (int32_t)((product >> 5) & 1) + lo;
    }
    result >>= 14;
    return x < 0 ? -result : result;
}

/* 0x11f4d8, 94 bytes. Full-circle (0x1921f units) wrapper around trig_lookup_11f460. */
int32_t trig_full_circle_11f4d8(int32_t angle)
{
    int32_t r = angle % 0x1921f;

    if (r < 0)
        r += 0x1921f;
    if (r <= 0x6487)
        return trig_lookup_11f460(r);
    if (r <= 0xc90f)
        return trig_lookup_11f460(0xc90f - r);
    if (r <= 0x12d97)
        return -trig_lookup_11f460(r - 0xc90f);
    return -trig_lookup_11f460(0x1921f - r);
}
