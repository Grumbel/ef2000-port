/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Recorder, key-match and slot-assignment helpers, 0x68e84..0xaae6c. */

/* 0x68e84, 120 bytes. Assign an object bit test to each of 20 page words; returns the number set. */
uint32_t assign_page_words_68e84(uint32_t object)
{
    uint32_t count = 0;
    int16_t i;

    if ((uint16_t)object == 0)
        return object;
    for (i = 0; i < 0x14; i++) {
        int16_t page = (int16_t)(EF_U32(0x001282dau) >> 16);
        uint16_t word = get_page_word_e7c90(page, i);

        if ((int16_t)word != -1 && word != EF_U16(0x001f6f42u))
            continue;
        if ((guest_fn_68ccc((int16_t)object, (uint16_t)i) & 0xffu) != 0) {
            count++;
            set_page_word_e7c50(page, (uint16_t)(EF_U32(0x001f6f36u) >> 16), i);
        } else {
            set_page_word_e7c50(page, 0xffffu, i);
        }
    }
    return count;
}

/* 0x7a5f0, 121 bytes. Append a word to the recorder table from the stream or a 73-byte record. */
void record_word_7a5f0(uint32_t index)
{
    if (EF_U32(0x0012afc8u) != 0) {
        uint16_t word = (uint16_t)buffer_read_word();
        ef_addr32 table = EF_U32(0x001f8ea4u);

        EF_U16(table + (uint32_t)((int32_t)(int16_t)EF_U16(table) * 6) + 6u) = word;
    } else {
        ef_addr32 table = EF_U32(0x001f8ea4u);
        ef_addr32 record = EF_U32(0x001a2e78u) + index * 73u;
        int16_t slot = (int16_t)EF_U16(table);

        EF_U16(table + (uint32_t)((int32_t)slot * 6) + 6u) = EF_U16(record + 0x3eu);
        slot = (int16_t)EF_U16(table);
        buffer_write_word((int16_t)((int32_t)EF_U32(table + (uint32_t)((int32_t)slot * 6) + 4u) >> 16));
    }
}

/* 0xaae6c, 124 bytes. Set flag 0x137206 when the default key letter matches a slot in mode 5. */
void match_default_key_aae6c(void)
{
    uint8_t letter = (uint8_t)to_upper_ascii((int32_t)EF_U8(default_path_a7b70()));
    int16_t count;

    EF_U8(0x00137206u) = 0;
    if (EF_U8(0x001371ecu) != 5)
        return;
    count = (int16_t)EF_U16(0x00426128u);
    if (count > 0) {
        int16_t i = 0;

        do {
            int32_t slot = (int32_t)(int16_t)EF_U16(0x0042612au) + (int32_t)i;

            if (slot == (int32_t)letter - 0x41 || slot == (int32_t)EF_U8(0x0041955cu) - 0x41) {
                EF_U8(0x00137206u) = 1;
                return;
            }
            i++;
        } while (i < (int16_t)EF_U16(0x00426128u));
    }
}
