/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

typedef union {
    uint64_t pair;
    uint32_t words[2];
} ef_string_read_registers;

#ifdef __WATCOMC__
static uint64_t read_string_file(ef_addr32 target, ef_addr32 name, ef_addr32 buffer);
#pragma aux read_string_file parm [eax] [edx] [ebx] value [edx eax] \
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
#pragma aux guest_callptr1_x parm [eax] [edx] modify [eax ecx edx]
#else
extern uint64_t read_string_file(ef_addr32 target, ef_addr32 name, ef_addr32 buffer);
#endif

/* 0x9d990, 191 bytes. No inputs, six registers preserved, plain RET.
 * Sole caller 9da8d ignores EAX. Original unchecked 144-byte path frame. */
void load_string_table_9d990(void)
{
    uint8_t path_storage[144];
    ef_addr32 path = (ef_addr32)(uintptr_t)path_storage;
    ef_addr32 buffer, cursor, table;
    ef_string_read_registers returned;
    uint32_t length;
    uint32_t count;
    uint32_t index = 0u;

    copy_string_9d749(path, 0x0041719au);
    strcat_9ca57(path, 0x0018b23cu);
    length = guest_callptr_eax_ret_x(EF_U32(0x00132ab4u), path);
    if (length == 0u)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x0018b248u);
    buffer = arena_take_e29e8(length);
    EF_U32(0x00132b0cu) = buffer;
    returned.pair = read_string_file(EF_U32(0x00132ab8u), path, buffer);
    cursor = returned.words[1];
    count = parse_decimal_9d964(returned.words[0]);
    table = arena_take_e29e8((uint32_t)(uint16_t)count * 4u);
    if ((uint16_t)count != 0u) {
        do {
            while (EF_U8(cursor) != 0x22u)
                cursor++;
            cursor++;
            EF_U32(table + (uint32_t)(uint16_t)index * 4u) = cursor;
            index++;
            while (EF_U8(cursor) != 0x22u)
                cursor++;
            EF_U8(cursor) = 0u;
            cursor++;
        } while ((uint16_t)index < (uint16_t)count);
    }
    EF_U32(0x00132b08u) = table;
}
