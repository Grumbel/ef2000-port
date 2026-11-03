/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_guest_memory.h"

static uint16_t read16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

/* 0x37294, 57 bytes. Only mode 1 copies the word and two marker-selected
 * source bytes. The source addresses can overlap other guest globals. */
uint32_t mirror_reference_state(void)
{
    uint8_t marker;
    if (g_reference_copy_mode != 1) { return g_reference_copy_mode; }
    g_reference_mirror[0] = (uint8_t)g_reference_source_word;
    g_reference_mirror[1] = (uint8_t)(g_reference_source_word >> 8);
    marker = g_world_grid_marker;
    g_reference_mirror[2] = *(uint8_t *)ef_guest_memory_ptr(0x0019bbf2u + marker);
    g_reference_mirror[3] = *(uint8_t *)ef_guest_memory_ptr(0x0019bbfau + marker);
    return g_reference_mirror[3];
}

/* 0x3cc28, 105 bytes. Three consecutive *in-place* passes: remove CR,
 * strip semicolon comments through LF, and collapse adjacent LF. A comment
 * lacking a terminating newline is an original guest overread, not fixed
 * in this provisional transcription. */
void normalize_script_line_endings(ef_addr32 text)
{
    uint8_t *base = (uint8_t *)ef_guest_memory_ptr(text);
    uint8_t *read = base, *write = base;
    uint8_t byte;
    do {
        byte = *read++;
        if (byte != '\r') { *write++ = byte; }
    } while (byte != 0);

    read = write = base;
    do {
        byte = *read;
        if (byte == ';') {
            do { byte = *read++; } while (byte != '\n');
        } else {
            ++read;
        }
        *write++ = byte;
    } while (byte != 0);

    read = write = base;
    do {
        byte = *read;
        if (byte == '\n') {
            while (read[1] == '\n') { ++read; }
            byte = *read;
        }
        *write++ = byte;
        ++read;
    } while (byte != 0);
}

/* 0x41f64, 96 bytes. Signed 16-bit coordinates and a 20-entry ring. */
void append_tactical_ring_event(int16_t x, int16_t y, uint8_t code)
{
    ef_tactical_ring_entry *entry = &g_tactical_ring[g_tactical_ring_cursor];
    entry->enabled = 1;
    entry->x = x;
    entry->y = y;
    entry->kind = code;
    entry->reserved = 0;
    ++g_tactical_ring_cursor;
    if (g_tactical_ring_cursor == 20) { g_tactical_ring_cursor = 0; }
}

/* 0x50ca4, 95 bytes. Spaces and tabs are consumed at the start, while
 * the first LF is *included* in the output and a NUL is appended. */
void copy_next_text_line(void)
{
    uint8_t byte;
    uint32_t count = 0;
    do {
        byte = *(uint8_t *)ef_guest_memory_ptr(g_text_scan_cursor);
        if (byte != ' ' && byte != '\t') { break; }
        ++g_text_scan_cursor;
    } while (1);
    do {
        g_text_line_buffer[count++] = byte;
        ++g_text_scan_cursor;
        byte = *(uint8_t *)ef_guest_memory_ptr(g_text_scan_cursor);
    } while (g_text_line_buffer[count - 1] != '\n');
    g_text_line_buffer[count] = 0;
    g_text_line_column = 0;
    ++g_text_line_count;
}

/* 0x5aa10, 82 bytes. The selected slot number is a signed byte; its code
 * is compared with each *nonzero-strength* slot in the 20-entry array. */
uint32_t count_matching_activity_slots(ef_addr32 object)
{
    const uint8_t *p = (const uint8_t *)ef_guest_memory_ptr(object);
    int32_t selected = (int8_t)p[0x1d7];
    uint8_t target_code = p[0x19b + selected * 3];
    uint32_t count = 0;
    unsigned i;
    for (i = 0; i < 20; ++i) {
        const uint8_t *slot = p + 0x19b + i * 3;
        if (read16(slot + 1) != 0 && slot[0] == target_code) { ++count; }
    }
    return count;
}

/* 0x62548, 61 bytes. Search records from the final active index backward. */
uint8_t active_unit_record_contains(int16_t index)
{
    int32_t i;
    for (i = g_active_unit_record_count - 1; i >= 0; --i) {
        const ef_unit_scan_record *entry = &g_active_unit_records[i];
        if (entry->active_code == 1 && entry->index == index) { return 1; }
    }
    return 0;
}

/* 0x631bc, 61 bytes. AX carries the result and 0xffff denotes no slot.
 * The original EAX upper word is not meaningful to its observed callers. */
uint16_t next_round_robin_record(void)
{
    uint16_t next = (uint16_t)(g_round_robin_cursor + 1u);
    if ((int16_t)next >= g_round_robin_count) { next = 0; }
    g_round_robin_cursor = next;
    if ((int16_t)next >= g_round_robin_count) { return 0xffffu; }
    return g_round_robin_ids[next];
}

/* 0x79524, 87 bytes. Four independent status bytes and six dwords. */
void clear_runtime_transient_counters(void)
{
    g_reset_flag_a = 0;
    g_reset_flag_b = 0;
    g_reset_flag_c = 0;
    g_reset_flag_d = 0;
    g_reset_accumulator_a[0] = g_reset_accumulator_a[1] = 0;
    g_reset_accumulator_b[0] = g_reset_accumulator_b[1] = 0;
    g_reset_accumulator_c[0] = g_reset_accumulator_c[1] = 0;
}
