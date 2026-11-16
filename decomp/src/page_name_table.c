/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

typedef struct {
    uint8_t text[144];
    int32_t identifier;
} ef_page_name_frame;
EF_STATIC_ASSERT(page_name_identifier, offsetof(ef_page_name_frame, identifier) == 144);
EF_STATIC_ASSERT(page_name_frame_size, sizeof(ef_page_name_frame) == 148);

typedef union {
    uint64_t pair;
    uint32_t words[2];
} ef_page_read_registers;

/* EAX=name, EDX=EBX=ECX=buffer at the guest callback; capture its
 * returned ECX/EBX rather than assuming either is the allocation base. */
#ifdef __WATCOMC__
static uint64_t read_page_names(ef_addr32 target, ef_addr32 name, ef_addr32 buffer);
#pragma aux read_page_names parm [eax] [edx] [ebx] value [edx eax] \
    modify [eax ebx ecx edx] = \
    "push esi" \
    "mov esi,eax" \
    "mov eax,edx" \
    "mov edx,ebx" \
    "mov ecx,ebx" \
    "call esi" \
    "pop esi" \
    "mov eax,ecx" \
    "mov edx,ebx"
#else
extern uint64_t read_page_names(ef_addr32 target, ef_addr32 name, ef_addr32 buffer);
#endif

/* 0x9da80,365 bytes. No inputs, preserves all six non-EAX GPRs.
 * Sole caller9d924 ignores EAX. Existing string-table gate skips all work. */
void load_page_name_table_9da80(void)
{
    ef_page_name_frame storage;
    ef_addr32 frame = (ef_addr32)(uintptr_t)&storage;
    ef_addr32 buffer, cursor, output, name;
    ef_page_read_registers returned;
    uint32_t length, pointer_offset, identifier_offset;
    uint8_t byte;

    if (EF_U32(0x00132b08u) != 0u)
        return;
    load_string_table_9d990();
    EF_U32(0x00417184u) = 0u;
    copy_string_9d749(frame, 0x0041719au);
    strcat_9ca57(frame, 0x0018b258u);
    length = guest_callptr_eax_ret_x(EF_U32(0x00132ab4u), frame);
    if (length == 0u)
        return;
    buffer = arena_alloc_e2a94(length + 1u);
    returned.pair = read_page_names(EF_U32(0x00132ab8u), frame, buffer);
    cursor = returned.words[1];
    EF_U8(returned.words[0] + length) = 0u;
    byte = EF_U8(returned.words[0]);
    EF_U32(0x00417184u) = 0u;
    while (byte != 0u) {
        EF_U32(0x00417184u) = EF_U32(0x00417184u) + 1u;
        cursor = skip_line_9da50(cursor);
        byte = EF_U8(cursor);
    }
    EF_U32(0x00417180u) = arena_take_e29e8((EF_U32(0x00417184u) + 1u) * 4u);
    EF_U32(0x00417170u) = arena_take_e29e8((EF_U32(0x00417184u) + 1u) * 2u);
    cursor = buffer;
    if (EF_U8(buffer) != 0u) {
        pointer_offset = 0u;
        identifier_offset = 0u;
        do {
            output = frame;
            byte = EF_U8(cursor);
            while (byte != 0u && EF_U8(cursor) != 0x20u) {
                byte = EF_U8(cursor);
                ++cursor;
                EF_U8(output) = byte;
                byte = EF_U8(cursor);
                ++output;
            }
            EF_U8(output) = 0u;
            while (EF_U8(cursor) != 0u && EF_U8(cursor) == 0x20u) {
                byte = EF_U8(cursor + 1u);
                ++cursor;
                if (byte == 0u)
                    break;
            }
            EF_U32(frame + 144u) = (uint32_t)(int32_t)(int16_t)parse_decimal_9d964(cursor);
            name = arena_take_e29e8(strlen_ds(frame) + 1u);
            copy_string_9d749(name, frame);
            identifier_offset += 2u;
            EF_U32(EF_U32(0x00417180u) + pointer_offset) = name;
            EF_U16(EF_U32(0x00417170u) + identifier_offset - 2u) = (uint16_t)EF_U32(frame + 144u);
            pointer_offset += 4u;
            cursor = skip_line_9da50(cursor);
        } while (EF_U8(cursor) != 0u);
    }
    arena_free_e2af4(buffer);
}
