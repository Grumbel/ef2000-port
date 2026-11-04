/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* memmove, MIDI event buffering, direction angles and reactivation test. */

/* 0x109e6f, 76 bytes. memmove: copy n bytes from src to dst, copying backwards when the ranges overlap with dst above src. */
void guest_memmove_109e6f(ef_addr32 dst, ef_addr32 src, uint32_t n)
{
    uint32_t i;

    if (src == dst)
        return;
    if (src < dst && src + n > dst) {
        for (i = n; i != 0; i--)
            EF_U8(dst + i - 1u) = EF_U8(src + i - 1u);
    } else {
        for (i = 0; i < n; i++)
            EF_U8(dst + i) = EF_U8(src + i);
    }
}

/* 0x111e40, 146 bytes. Append a MIDI event (status, data1 and, for three-byte events, data2) to the record's buffer at +8 (offset 0x100, fill count at +0x1ac), flushing first when it would overflow 0x200 bytes (cdecl, 4 stack args). */
void append_midi_event_111e40(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec, uint32_t status, uint32_t data1, uint32_t data2)
{
    uint32_t length;
    (void)u0; (void)u1; (void)u2; (void)u3;
    length = midi_data_length_111d98(status);
    if (length + EF_U32(rec + 0x1acu) > 0x200u)
        send_pending_count_message_111df0(0, 0, 0, 0, rec);
    EF_U8(EF_U32(rec + 8u) + EF_U32(rec + 0x1acu) + 0x100u) = (uint8_t)status;
    EF_U32(rec + 0x1acu) += 1u;
    EF_U8(EF_U32(rec + 8u) + EF_U32(rec + 0x1acu) + 0x100u) = (uint8_t)data1;
    EF_U32(rec + 0x1acu) += 1u;
    if (length == 3u) {
        EF_U8(EF_U32(rec + 8u) + EF_U32(rec + 0x1acu) + 0x100u) = (uint8_t)data2;
        EF_U32(rec + 0x1acu) += 1u;
    }
    EF_U32(rec + 0x1a8u) += 1u;
}

/* 0x111ed4, 76 bytes. Queue a raw MIDI block (at most 0x200 bytes) into the record's buffer, flushing before and after (cdecl: record, data, length). */
void append_midi_block_111ed4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec, ef_addr32 data, uint32_t length)
{
    (void)u0; (void)u1; (void)u2; (void)u3;
    send_pending_count_message_111df0(0, 0, 0, 0, rec);
    guest_memmove_109e6f(EF_U32(rec + 8u) + 0x100u, data, length > 0x200u ? 0x200u : length);
    EF_U32(rec + 0x1a8u) += 1u;
    send_pending_count_message_111df0(0, 0, 0, 0, rec);
}

/* 0xdc91c, 180 bytes. Store the elevation and heading angles of the vector from point b to point a (three dwords each) into two words at out: halve the deltas to fit 16 bits, elevation from the 2D length, heading from the unscaled deltas. */
void direction_angles_dc91c(ef_addr32 out, ef_addr32 a, ef_addr32 b)
{
    int32_t d[3];
    int32_t len;

    d[0] = EF_S32(a) - EF_S32(b);
    d[1] = EF_S32(a + 4u) - EF_S32(b + 4u);
    d[2] = EF_S32(a + 8u) - EF_S32(b + 8u);
    if (d[0] < -0x8000 || d[0] > 0x7fff || d[1] < -0x8000 || d[1] > 0x7fff || d[2] < -0x8000 || d[2] > 0x7fff)
        halve_triple_to_fit16_dc890((uint32_t)(uintptr_t)&d[0], (uint32_t)(uintptr_t)&d[1], (uint32_t)(uintptr_t)&d[2]);
    len = (int32_t)length_2d_decb0(d[2], d[0]);
    EF_U16(out) = (uint16_t)angle_of_b3b1c(d[1], len);
    EF_U16(out + 2u) = (uint16_t)angle_of_b3b1c(EF_S32(a) - EF_S32(b), EF_S32(a + 8u) - EF_S32(b + 8u));
}

/* 0xc117c, 124 bytes. Whether the record may be reactivated: its cell holds more than 12 records and reactivating at most 0x30 - count of them fails, with the count between 7 and 24. */
int32_t can_reactivate_c117c(ef_addr32 rec, ef_addr32 list_a, ef_addr32 list_b)
{
    int16_t count = (int16_t)count_records_at_cell_bfb2c(EF_S16(rec + 0x11u), EF_S16(rec + 0x15u), list_a);
    int32_t need;

    if (count > 0xc) {
        need = 0x30 - count;
        /* ORIGINAL BUG: a positive remainder is zeroed instead of being negated. */
        if (need > 0)
            need = 0;
        if ((uint16_t)reactivate_records_at_cell_c10c4(EF_S16(rec + 0x11u), EF_S16(rec + 0x15u), (uint32_t)(int32_t)(int16_t)need,
                                                       EF_U8(rec + 0xbu), list_b, list_a) == 0
            && count <= 0x18 && count > 6)
            return 1;
    }
    return 0;
}
