/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Linked record list walkers. */

/* 0xbe3f4, 68 bytes. For each flag-less record of the first list copy the id of every record of the second list that shares its position. */
void copy_ids_of_colocated_records_be3f4(ef_addr32 first_list, ef_addr32 second_list)
{
    ef_addr32 rec = EF_U32(first_list);

    while (rec != 0) {
        if (EF_U8(rec + 0xdu) == 0) {
            ef_addr32 other = EF_U32(second_list);
            while (other != 0) {
                if ((uint16_t)records_share_position(other, rec) != 0)
                    EF_U16(rec + 0x31u) = EF_U16(other + 9u);
                other = EF_U32(other + 4u);
            }
        }
        rec = EF_U32(rec + 4u);
    }
}

/* 0xbe77c, 104 bytes. Find the record whose id matches the object's and move its entries' weights from field 0x23 to field 0x25. */
void transfer_entry_weights_be77c(ef_addr32 object, ef_addr32 list)
{
    ef_addr32 rec = EF_U32(list);
    int16_t pending = 1;

    while (rec != 0 && pending != 0) {
        if (EF_U16(rec + 9u) == EF_U16(object + 0xeu)) {
            int16_t i = 0;
            while (i < (int16_t)EF_U8(object + 0x12u)) {
                uint8_t w = EF_U8(EF_U32(object + 0x13u) + (uint32_t)(int32_t)i * 0x7cu + 1u);
                EF_U16(rec + 0x23u) = (uint16_t)(EF_U16(rec + 0x23u) - w);
                EF_U16(rec + 0x25u) = (uint16_t)(EF_U16(rec + 0x25u) + w);
                i++;
            }
            pending = 0;
        }
        rec = EF_U32(rec + 4u);
    }
}

/* 0xbd2f4, 79 bytes. Instantiate every valid record of a list into a destination list, numbering them; returns the count. */
uint32_t instantiate_list_records_bd2f4(ef_addr32 list, uint32_t arg_b, uint32_t dest_head, uint32_t dest_tail, uint32_t extra)
{
    ef_addr32 rec = EF_U32(list);
    uint16_t n = 0;

    while (rec != 0) {
        if ((uint16_t)guest_fn_bcc2c_x(rec) != 0) {
            ef_addr32 made = guest_fn_bcf44_x(rec, arg_b, extra);
            EF_U16(made + 9u) = n;
            list_insert_head(made, dest_head, dest_tail);
            n++;
        }
        rec = EF_U32(rec + 4u);
    }
    return n;
}
