/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Ready-flag tables, history ring and small state helpers, scattered. */

/* 0x56ba8, 52 bytes. True when every ready word in table 0x1a5dd0 is nonzero. */
uint8_t all_ready_56ba8(void)
{
    int16_t count = (int16_t)EF_U16(0x001357b0u);
    int16_t i;

    for (i = 0; i < count; i++) {
        if (EF_U16(0x001a5dd0u + (uint32_t)((int32_t)i * 2)) == 0)
            return 0;
    }
    return 1;
}

/* 0x56cbc, 15 bytes. Mark the ready word of the unit in a message at +1 (table 0x1a5dd0). */
void mark_ready_56cbc(ef_addr32 message)
{
    EF_U16(0x001a5dd0u + (uint32_t)((int32_t)(int16_t)EF_U16(message + 1u) * 2)) = 1;
}

/* 0x56efc, 15 bytes. Mark the ready word of the unit in a message at +1 (table 0x1a5dc0). */
void mark_ready_56efc(ef_addr32 message)
{
    EF_U16(0x001a5dc0u + (uint32_t)((int32_t)(int16_t)EF_U16(message + 1u) * 2)) = 1;
}

/* 0x56f30, 60 bytes. Forward a message to 0x8f22c when its unit has a nonzero slot word. */
void forward_slot_message_56f30(ef_addr32 message)
{
    ef_addr32 unit = guest_fn_7489c((int16_t)EF_U16(message + 1u));
    uint32_t slot;

    if (unit == 0)
        return;
    slot = EF_U8(message + 3u);
    if (EF_U16(unit + slot * 3u + 0x19cu) == 0)
        return;
    guest_fn_8f22c((int16_t)EF_U16(message + 1u), slot);
}

/* 0x56ffc, 52 bytes. True when every ready word in table 0x1a5de0 is nonzero. */
uint8_t all_ready_56ffc(void)
{
    int16_t count = (int16_t)EF_U16(0x001357b0u);
    int16_t i;

    for (i = 0; i < count; i++) {
        if (EF_U16(0x001a5de0u + (uint32_t)((int32_t)i * 2)) == 0)
            return 0;
    }
    return 1;
}

/* 0x570fc, 19 bytes. Mark the ready word indexed by the message byte at +1 (table 0x1a5de0). */
void mark_ready_570fc(ef_addr32 message)
{
    EF_U16(0x001a5de0u + (uint32_t)EF_U8(message + 1u) * 2u) = 1;
}

/* 0x577e8, 56 bytes. Count a local-owner message, then hand the message on. */
void count_and_forward_577e8(ef_addr32 message)
{
    if (EF_U8(message + 3u) == 0 && EF_U8(message + 4u) == 0 && EF_U8(message + 5u) == 0) {
        uint8_t owner = EF_U8(0x001a666bu);

        if (owner == EF_U8(message + 1u) && owner < 8)
            EF_U16(0x001a6388u + (uint32_t)owner * 2u) = (uint16_t)(EF_U16(0x001a6388u + (uint32_t)owner * 2u) + 1u);
    }
    guest_fn_57820(message);
}

/* 0x6319c, 32 bytes. Append a word to the 100-entry history ring. */
void push_history_6319c(uint16_t value)
{
    int32_t count = (int32_t)EF_U32(0x001a81aeu) >> 16;

    if (count < 0x64) {
        EF_U16(0x001a78e0u + (uint32_t)(count * 2)) = value;
        EF_U16(0x001a81b0u) = (uint16_t)(EF_U16(0x001a81b0u) + 1u);
    }
}

/* 0x631fc, 45 bytes. True when a value is present in the history ring. */
uint8_t history_contains_631fc(uint16_t value)
{
    uint16_t i = EF_U16(0x001a81b0u);
    uint8_t found = 0;

    if (i != 0) {
        do {
            i--;
            if (value == EF_U16(0x001a78e0u + (uint32_t)((int32_t)(int16_t)i * 2)))
                found = 1;
        } while (i != 0);
    }
    return found;
}

/* 0x77748, 18 bytes. Count a pending frame while the marker word is -1. */
void count_pending_77748(void)
{
    if (EF_U16(0x001f891eu) == 0xffffu)
        EF_U16(0x001f891cu) = (uint16_t)(EF_U16(0x001f891cu) + 1u);
}

/* 0x789d0, 58 bytes. Store a 122-byte name entry for an owner slot, skipping the local owner. */
void set_owner_name_789d0(uint8_t owner, ef_addr32 name)
{
    uint32_t index = owner;
    uint32_t offset;

    if (owner > EF_U8(0x001a666bu))
        index = (uint8_t)(owner - 1u);
    offset = index * 122u;
    copy_string_9d749(0x001f8930u + offset, name);
    EF_U16(offset + 0x001f89a8u) = 0x118;
}

/* 0x8b5f0, 38 bytes. Latch a word pointer (+1) and clamp its mode to 1 or 2. */
void latch_mode_word_8b5f0(ef_addr32 pointer)
{
    pointer++;
    EF_U32(0x0012db54u) = pointer;
    EF_U16(0x0012db58u) = 1;
    if ((int16_t)EF_U16(pointer) >= 2)
        EF_U16(0x0012db58u) = 2;
}

/* 0xa45b3, 49 bytes. Step a 32-bit generator held in two words; reseed when it reaches zero. */
uint32_t step_generator_a45b3(void)
{
    uint32_t add = (uint32_t)EF_U16(0x000a45a6u) + (uint32_t)EF_U16(0x000a45a4u);
    uint16_t new_lo = (uint16_t)add;
    uint16_t new_hi = (uint16_t)((uint32_t)EF_U16(0x000a45a4u) + (uint32_t)new_lo + (add >> 16));

    EF_U16(0x000a45a6u) = new_lo;
    EF_U16(0x000a45a4u) = new_hi;
    if ((uint16_t)(new_hi | new_lo) == 0)
        EF_U32(0x000a45a4u) = 0x31415926u;
    return new_lo;
}

/* 0xa4640, 53 bytes. Find a word in the 0x41eda8 id table; returns its index, or a cleared low byte. */
uint32_t find_id_a4640(uint16_t id)
{
    uint16_t count = EF_U16(0x0041eed8u);
    uint16_t i = 0;

    while (i < count) {
        if (id == EF_U16(0x0041eda8u + (uint32_t)i * 2u))
            return i;
        i++;
    }
    return (uint32_t)i & 0xff00u;
}

/* 0xa4e6c, 41 bytes. Clear the 42-entry id table. */
void clear_id_table_a4e6c(void)
{
    uint16_t i;

    for (i = 0; i < 0x2a; i++)
        EF_U16(0x0041eda8u + (uint32_t)i * 2u) = 0;
}

/* 0xa9cd0, 58 bytes. Track a mouse request and refresh the cursor when its value changed. */
void update_mouse_request_a9cd0(void)
{
    uint32_t value;

    if (EF_U16(0x0015ee9au) == 0 || EF_U8(0x00426147u) == 0)
        return;
    value = mouse_request_de2d8();
    EF_U32(0x00419158u) = value;
    if ((int32_t)value != ((int32_t)EF_U32(0x00426122u) >> 16))
        guest_fn_de3b4((int32_t)EF_U32(0x00426124u) >> 16);
}

/* 0xac7a8, 24 bytes. Set the pending flag and copy a name into the message buffer. */
void set_pending_name_ac7a8(ef_addr32 name)
{
    EF_U16(0x0041eed2u) = 1;
    copy_string_9d749(0x0041f262u, name);
}

/* 0xb3ae0, 57 bytes. Table-driven ratio of two unsigned values (a/b or complement). */
uint32_t ratio_lookup_b3ae0(uint32_t a, uint32_t b)
{
    uint32_t quotient;
    uint32_t remainder;

    if (a > b) {
        uint32_t dx;

        quotient = (b << 9) / a;
        remainder = (b << 9) % a;
        dx = (remainder & 0xffff0000u) | EF_U16(0x00137a04u + quotient * 2u);
        return 0x4000u - dx;
    }
    if (b == 0)
        return 0; /* original traps on divide by zero */
    quotient = (a << 9) / b;
    return (quotient & 0xffff0000u) | EF_U16(0x00137a04u + quotient * 2u);
}

/* 0xc58cc, 38 bytes. Reset three clip bounds; both stack arguments are ignored. */
void reset_clip_bounds_c58cc(uint32_t stack_a, uint32_t stack_b)
{
    (void)stack_a;
    (void)stack_b;
    EF_U32(0x0015b258u) = 0;
    EF_U32(0x0015b25cu) = 0x7fff;
    EF_U32(0x0015b260u) = 0;
}

/* 0xdc7bc, 40 bytes. Load a sine/cosine word pair for an angle (8 angle units per table step). */
void load_sincos_dc7bc(uint32_t angle)
{
    uint32_t index = (angle & 0xffffu) >> 3;

    EF_U16(0x00425f92u) = EF_U16(0x00141e08u + index * 2u);
    EF_U16(0x00425f90u) = EF_U16(0x00142e08u + index * 2u);
}
