/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* MIDI driver state fields */
#define MDI_DRIVER        0x000u  /* AIL driver descriptor */
#define MDI_BUFFER        0x008u  /* transfer buffer; timbre lists go to +0x100 */
#define MDI_LOCKED        0x014u
#define MDI_TIMBRE_STATUS 0x1a4u  /* (state, bank, patch): nonzero = already installed */

/* sequence record fields */
#define SEQ_DRIVER        0x000u
#define SEQ_STATUS        0x004u  /* 2 = done */
#define SEQ_TIMBRES       0x008u  /* TIMB chunk */
#define SEQ_BRANCHES      0x00cu  /* RBRN chunk */
#define SEQ_EVENTS        0x010u  /* EVNT chunk */
#define SEQ_FIELD_18      0x018u
#define SEQ_FIELD_1C      0x01cu
#define SEQ_FIELD_20      0x020u
#define SEQ_BEAT_CALLBACK 0x024u
#define SEQ_END_CALLBACK  0x028u
#define SEQ_LOOPS         0x02cu
#define SEQ_VOLUME        0x038u
#define SEQ_VOLUME_TARGET 0x03cu
#define SEQ_VOLUME_ACCUM  0x040u
#define SEQ_VOLUME_PERIOD 0x044u
#define SEQ_TEMPO         0x048u  /* percent */
#define SEQ_TEMPO_TARGET  0x04cu
#define SEQ_TEMPO_ACCUM   0x050u
#define SEQ_TEMPO_PERIOD  0x054u
#define SEQ_TEMPO_BALANCE 0x058u
#define SEQ_DONE          2u

/* IFF chunk: four-character tag, big-endian size, data */
#define CHUNK_SIZE        0x04u
#define CHUNK_HEADER      0x08u
#define FORM_FIRST_CHUNK  0x0cu   /* after "FORM", the size and "XMID" */

/* TIMB chunk: word count, then (patch, bank) byte pairs */
#define TIMB_SIZE_HIGH    0x04u   /* the low 16 bits of the big-endian size */
#define TIMB_SIZE_LOW     0x05u
#define TIMB_COUNT        0x08u
#define TIMB_ENTRIES      0x0au
#define TIMB_COPY_SIZE    0x200u

#define AIL_INSTALL_TIMBRES 0x503u

#define TIMB_COPY         0x004ddce0u
#define AIL_ERROR_TEXT    0x004dd6b0u
#define g_xmidi_volume    EF_U32(0x004dd800u)

static void set_ail_error(ef_addr32 message)
{
    ef_addr32 out = AIL_ERROR_TEXT;
    uint8_t c;
    do {
        c = EF_U8(message++);
        EF_U8(out++) = c;
    } while (c != 0);
}

static ef_addr32 next_chunk(ef_addr32 chunk)
{
    return chunk + byte_swap_111f50(EF_U32(chunk + CHUNK_SIZE)) + CHUNK_HEADER;
}

/* Drop timbre I from the list in the TIMB copy. */
static void remove_timbre(ef_addr32 timb, uint32_t i)
{
    uint32_t j;
    uint8_t low;

    for (j = i + 1u; j < EF_U16(timb + TIMB_COUNT); j++)
        EF_U16(timb + TIMB_ENTRIES + (j - 1u) * 2u) = EF_U16(timb + TIMB_ENTRIES + j * 2u);
    EF_U16(timb + TIMB_COUNT) = (uint16_t)(EF_U16(timb + TIMB_COUNT) - 1u);
    low = EF_U8(timb + TIMB_SIZE_LOW);
    EF_U8(timb + TIMB_SIZE_LOW) = (uint8_t)(low - 2u);
    if (low < 2u)
        EF_U8(timb + TIMB_SIZE_HIGH) = (uint8_t)(EF_U8(timb + TIMB_SIZE_HIGH) - 1u);
}

/* 0x113d10, 794 bytes. AIL_init_sequence (cdecl): find sequence NUMBER of
   the XMIDI file FILE, record its TIMB, RBRN and EVNT chunks in SEQ and
   reset the playback state. When the sequence lists timbres, copy the
   list, drop those the driver already has and ask the driver to install
   the rest. Returns 1, 0 for an invalid sequence, or -1 with a message in
   the AIL error buffer when the timbres cannot be installed. */
int32_t init_sequence_113d10(ef_addr32 seq, ef_addr32 file, int32_t number)
{
    uint16_t regs[4];             /* AX, BX, CX, DX */
    ef_addr32 chunk;
    ef_addr32 end;
    ef_addr32 state;
    uint32_t entry;
    uint32_t i;

    if (seq == 0)
        return 0;
    EF_U32(seq + SEQ_STATUS) = SEQ_DONE;
    chunk = find_chunk_111f84(0, 0, 0, 0, file, number);
    if (chunk == 0) {
        set_ail_error(0x00195554u); /* "Invalid XMIDI sequence\n" */
        return 0;
    }

    end = next_chunk(chunk);
    EF_U32(seq + SEQ_TIMBRES) = 0;
    EF_U32(seq + SEQ_BRANCHES) = 0;
    EF_U32(seq + SEQ_EVENTS) = 0;
    for (chunk += FORM_FIRST_CHUNK; chunk < end; chunk = next_chunk(chunk)) {
        if (compare_strings_e5c03(chunk, 0x0019556cu, 4) == 0)  /* "TIMB" */
            EF_U32(seq + SEQ_TIMBRES) = chunk;
        if (compare_strings_e5c03(chunk, 0x00195574u, 4) == 0)  /* "RBRN" */
            EF_U32(seq + SEQ_BRANCHES) = chunk;
        if (compare_strings_e5c03(chunk, 0x0019557cu, 4) == 0)  /* "EVNT" */
            EF_U32(seq + SEQ_EVENTS) = chunk;
    }
    if (EF_U32(seq + SEQ_EVENTS) == 0) {
        set_ail_error(0x00195554u);
        return 0;
    }

    EF_U32(seq + SEQ_FIELD_18) = 0;
    EF_U32(seq + SEQ_FIELD_1C) = 0;
    EF_U32(seq + SEQ_FIELD_20) = 0;
    EF_U32(seq + SEQ_BEAT_CALLBACK) = 0;
    EF_U32(seq + SEQ_END_CALLBACK) = 0;
    EF_U32(seq + SEQ_LOOPS) = 1;
    reset_table_record_and_link_112bbc(0, 0, 0, 0, seq);
    EF_U32(seq + SEQ_VOLUME_PERIOD) = 0;
    EF_U32(seq + SEQ_VOLUME_ACCUM) = 0;
    EF_U32(seq + SEQ_TEMPO) = 100;
    EF_U32(seq + SEQ_TEMPO_TARGET) = 100;
    EF_U32(seq + SEQ_TEMPO_PERIOD) = 0;
    EF_U32(seq + SEQ_TEMPO_ACCUM) = 0;
    EF_U32(seq + SEQ_TEMPO_BALANCE) = 0;
    EF_U32(seq + SEQ_VOLUME) = g_xmidi_volume;
    EF_U32(seq + SEQ_VOLUME_TARGET) = g_xmidi_volume;
    if (EF_U32(seq + SEQ_TIMBRES) == 0)
        return 1;

    guest_memmove_109e6f(TIMB_COPY, EF_U32(seq + SEQ_TIMBRES), TIMB_COPY_SIZE);
    if (EF_U32(EF_U32(seq + SEQ_DRIVER) + MDI_TIMBRE_STATUS) != 0) {
        i = 0;
        while (i < EF_U16(TIMB_COPY + TIMB_COUNT)) {
            entry = EF_U16(TIMB_COPY + TIMB_ENTRIES + i * 2u);
            state = EF_U32(seq + SEQ_DRIVER);
            if (guest_callptr3_ret_x(EF_U32(state + MDI_TIMBRE_STATUS), state, entry >> 8, entry & 0xffu) == 0)
                i++;
            else
                remove_timbre(TIMB_COPY, i);
        }
    }
    if (EF_U16(TIMB_COPY + TIMB_COUNT) == 0)
        return 1;

    if (tail_jump_102020(0) != 0) {
        set_ail_error(0x00195584u); /* "No timbres loaded\n" */
        return -1;
    }
    state = EF_U32(seq + SEQ_DRIVER);
    EF_U32(state + MDI_LOCKED) += 1u;
    send_pending_count_message_111df0(0, 0, 0, 0, EF_U32(seq + SEQ_DRIVER));
    guest_memmove_109e6f(EF_U32(EF_U32(seq + SEQ_DRIVER) + MDI_BUFFER) + 0x100u, TIMB_COPY, TIMB_COPY_SIZE);
    guest_fn_101f88(EF_U32(EF_U32(seq + SEQ_DRIVER) + MDI_DRIVER), AIL_INSTALL_TIMBRES, 0,
                    (uint32_t)(uintptr_t)regs);
    EF_U32(EF_U32(seq + SEQ_DRIVER) + MDI_LOCKED) -= 1u;
    if (regs[0] != 0)
        return 1;
    /* "Driver could not install timbre bank %u, patch %u\n" */
    guest_sprintf_a4342(AIL_ERROR_TEXT, 0x00195598u,
                        (uint32_t)(int32_t)(int8_t)(regs[1] >> 8), (uint32_t)(regs[1] & 0xffu));
    return -1;
}
