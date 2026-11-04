/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* List, clamp and release helpers, 0xa992c..0xbe438. */

/* 0xa992c, 97 bytes. Rotate the saved list state into place; returns the global at 0x146e18. */
uint32_t restore_list_state_a992c(void)
{
    EF_U16(0x004194bcu) = EF_U16(0x004194c2u);
    EF_U32(0x004191d8u) = EF_U32(0x00419200u);
    EF_U32(0x004191e4u) = EF_U32(0x004191f8u);
    EF_U32(0x004191f0u) = EF_U32(0x004191dcu);
    EF_U16(0x004194c2u) = EF_U16(0x004194beu);
    EF_U32(0x00419200u) = EF_U32(0x004191f4u);
    EF_U32(0x004191f8u) = EF_U32(0x004191e0u);
    EF_U32(0x004191dcu) = EF_U32(0x004191e8u);
    EF_U32(0x004191dcu) = EF_U32(EF_U32(0x004191e8u) + 5u);
    return get_global_b3f48();
}

/* 0xac7f0, 97 bytes. Clamp the cursor (0x13725e, 0x137260) into the window rectangle. */
void clamp_cursor_ac7f0(void)
{
    int16_t y = (int16_t)EF_U16(0x00137260u);
    int16_t x = (int16_t)EF_U16(0x0013725eu);

    if (!(x >= (int16_t)EF_U16(0x00137950u)))
        x = (int16_t)EF_U16(0x00137950u);
    if (!(x < (int16_t)EF_U16(0x00137954u)))
        x = (int16_t)(EF_U16(0x00137954u) - 1);
    if (!(y >= (int16_t)EF_U16(0x00137952u)))
        y = (int16_t)EF_U16(0x00137952u);
    if (!(y < (int16_t)EF_U16(0x00137956u)))
        y = (int16_t)(EF_U16(0x00137956u) - 1);
    EF_U16(0x0013725eu) = (uint16_t)x;
    EF_U16(0x00137260u) = (uint16_t)y;
}

/* 0xb0238, 110 bytes. Copy per-column values into every matching node of the list. */
void update_columns_b0238(ef_addr32 list, int16_t count)
{
    int16_t column;

    for (column = 0; column < count; column++) {
        uint16_t first = (uint16_t)guest_fn_b0208(list, (uint8_t)column, 0xf);
        uint16_t second = (uint16_t)guest_fn_b0208(list, (uint8_t)column, 0x11);
        ef_addr32 node = EF_U32(list);

        while (node != 0) {
            if ((uint16_t)EF_U8(node + 0xbu) == (uint16_t)column && EF_U8(node + 9u) == 0) {
                EF_U16(node + 0x17u) = first;
                EF_U16(node + 0x19u) = second;
            }
            node = EF_U32(node + 4u);
        }
    }
}

/* 0xb18b8, 93 bytes. Free every node of the list (and their three owned blocks), then clear both heads. */
void free_node_list_b18b8(ef_addr32 head, ef_addr32 other)
{
    ef_addr32 node = EF_U32(head);

    while (node != 0) {
        ef_addr32 next = EF_U32(node + 4u);

        if (EF_U32(node + 0xcu) != 0)
            guest_fn_cc768(EF_U32(node + 0xcu));
        if (EF_U32(node + 0x10u) != 0)
            guest_fn_cc768(EF_U32(node + 0x10u));
        if (EF_U32(node + 0x2cu) != 0)
            guest_fn_cc768(EF_U32(node + 0x2cu));
        guest_fn_cc768(node);
        node = next;
    }
    EF_U32(head) = 0;
    EF_U32(other) = 0;
}

/* 0xb1918, 91 bytes. Free the owned blocks of a record, then the record. */
void free_record_b1918(ef_addr32 record)
{
    if (EF_U32(record + 0x33u) != 0)
        guest_fn_cc768(EF_U32(record + 0x33u));
    if (EF_U32(record + 0x37u) != 0)
        guest_fn_cc768(EF_U32(record + 0x37u));
    if (EF_U32(record + 0x3eu) != 0)
        guest_fn_cc768(EF_U32(record + 0x3eu));
    if (EF_U32(record + 0x42u) != 0)
        guest_fn_cc768(EF_U32(record + 0x42u));
    if (EF_U16(record + 0x48u) != 0)
        guest_fn_cc768(EF_U32(record + 0x4au));
    guest_fn_cc768(record);
}

/* 0xb4c20, 91 bytes. Find a kind-0x31 child (following the first link) that passes the check. */
ef_addr32 find_child_b4c20(ef_addr32 key, uint8_t kind, ef_addr32 list, uint32_t context)
{
    ef_addr32 parent = EF_U32(list);

    while (parent != 0) {
        if (EF_U16(parent + 9u) == EF_U16(key + 9u)) {
            for (;;) {
                ef_addr32 child = EF_U32(parent);

                if (child == 0)
                    return 0;
                if (EF_U8(child + 0xcu) == 0x31
                    && (guest_fn_cc1ac(kind, EF_U8(child + 0xbu), context) & 0xffffu) != 0)
                    return child;
                parent = EF_U32(parent);
            }
        }
        parent = EF_U32(parent + 4u);
    }
    return 0;
}

/* 0xb4c7c, 93 bytes. Find a kind-0x31 sibling (following the second link) that passes the check. */
ef_addr32 find_sibling_b4c7c(ef_addr32 key, uint8_t kind, ef_addr32 list, uint32_t context)
{
    ef_addr32 parent = EF_U32(list);

    while (parent != 0) {
        if (EF_U16(parent + 9u) == EF_U16(key + 9u)) {
            for (;;) {
                ef_addr32 child = EF_U32(parent + 4u);

                if (child == 0)
                    return 0;
                if (EF_U8(child + 0xcu) == 0x31
                    && (guest_fn_cc1ac(kind, EF_U8(child + 0xbu), context) & 0xffffu) != 0)
                    return child;
                parent = EF_U32(parent + 4u);
            }
        }
        parent = EF_U32(parent + 4u);
    }
    return 0;
}

/* 0xb56d0, 57 bytes. Frame index scaled by the per-row count: (column * 60 + row) / divisor. */
int32_t frame_index_b56d0(int16_t column, int16_t row, ef_addr32 object)
{
    int16_t divisor = (int16_t)guest_fn_b549c((int16_t)EF_U16(object + 0x48u));
    int16_t offset = (int16_t)(column - (int16_t)EF_U16(0x001379a0u));

    return ((int32_t)offset * 60 + (int32_t)row) / (int32_t)divisor;
}

/* 0xbd508, 75 bytes. Count the 10-byte entries of the record that are special and non-empty. */
uint32_t count_special_entries_bd508(ef_addr32 record)
{
    uint32_t count = 0;
    int16_t i;

    for (i = 0; i < (int16_t)EF_U8(record + 5u); i++) {
        ef_addr32 entry = EF_U32(record + 6u) + (uint32_t)((int32_t)i * 10);

        if ((slot_type_is_special_bd490(EF_U8(entry + 9u), EF_U8(record)) & 0xffffu) != 0 && EF_U16(entry) != 0)
            count++;
    }
    return count;
}

/* 0xbd554, 82 bytes. Count the special entries of a kind-0x15 record, 1 for other kinds. */
uint32_t count_entries_bd554(ef_addr32 record)
{
    uint32_t count = 0;
    int16_t i;

    if (EF_U8(record + 1u) != 0x15)
        return 1;
    for (i = 0; i < (int16_t)EF_U8(record + 5u); i++) {
        ef_addr32 entry = EF_U32(record + 6u) + (uint32_t)((int32_t)i * 10);

        if ((slot_type_is_special_bd490(EF_U8(entry + 9u), EF_U8(record)) & 0xffffu) != 0)
            count++;
    }
    return count;
}

/* 0xbe438, 69 bytes. Find the kind-2 node of the list whose target matches the two keys and is valid. */
ef_addr32 find_target_be438(uint32_t key_a, uint32_t key_b, ef_addr32 list)
{
    ef_addr32 node = EF_U32(list);

    while (node != 0) {
        ef_addr32 target = EF_U32(node + 0x13u);

        if (key_a == EF_U32(target + 8u) && key_b == EF_U32(target + 0xcu) && EF_U8(node + 9u) == 2
            && (is_kind_bcbc0(EF_U8(target)) & 0xffffu) != 0)
            return node;
        node = EF_U32(node + 4u);
    }
    return 0;
}
