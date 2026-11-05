/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* These are host-tested C models of four original renderer queue entries.
 * The x86 queue has TWO associated 16-bit arrays, indexed from one. A
 * self-modified displacement at c41c4 selects its overflow policy; the C
 * model keeps that byte in guest memory, without writing into executable
 * code in the host. All comparisons below are signed 16-bit comparisons. */

static int16_t key_at(uint32_t index)
{
    return EF_S16(0x146e54u + 2u * index);
}

static void move_pair(uint32_t to, uint32_t from)
{
    EF_U16(0x146e54u + 2u * to) = EF_U16(0x146e54u + 2u * from);
    EF_U16(0x147e54u + 2u * to) = EF_U16(0x147e54u + 2u * from);
}

static void swap_pair(uint32_t a, uint32_t b)
{
    uint16_t key = EF_U16(0x146e54u + 2u * a);
    uint16_t tag = EF_U16(0x147e54u + 2u * a);
    move_pair(a, b);
    EF_U16(0x146e54u + 2u * b) = key;
    EF_U16(0x147e54u + 2u * b) = tag;
}

/* c3fd4, 200 bytes. The source uses the final element as the pivot and
 * skips runs of at most FIVE elements, which the finishing insertion sort
 * handles. The source assumes a sentinel at index zero for insertion sort. */
void renderer_partial_quicksort_c3fd4(int32_t low, int32_t high)
{
    int32_t left, right, pivot;
    if (high - low <= 4)
        return;
    pivot = key_at((uint32_t)high);
    left = low - 1;
    right = high;
    for (;;) {
        do { ++left; } while (pivot > key_at((uint32_t)left));
        do { --right; } while (pivot < key_at((uint32_t)right));
        if (left >= right)
            break;
        swap_pair((uint32_t)left, (uint32_t)right);
    }
    swap_pair((uint32_t)left, (uint32_t)high);
    renderer_partial_quicksort_c3fd4(low, left - 1);
    renderer_partial_quicksort_c3fd4(left + 1, high);
}

/* c409c, 103 bytes. Equal keys move to the right, so the original finishing
 * pass is NOT stable. Assumes key[0] holds a suitable lower-bound sentinel. */
void renderer_insertion_sort_c409c(void)
{
    uint32_t count = EF_U16(0x146e50u);
    uint32_t i;
    for (i = 1u; i < count; ++i) {
        int16_t key = key_at(i + 1u);
        uint16_t tag = EF_U16(0x147e54u + 2u * (i + 1u));
        uint32_t j = i;
        while (key <= key_at(j)) {
            move_pair(j + 1u, j);
            --j;
        }
        EF_U16(0x146e54u + 2u * (j + 1u)) = (uint16_t)key;
        EF_U16(0x147e54u + 2u * (j + 1u)) = tag;
    }
}

/* c4167, 73 bytes: finish any pending sort, then dispatch queued draw
 * handlers and the frame callback in that order. */
void renderer_finish_queue_c4167(void)
{
    if (EF_U8(0xc41c4u) != 0x6eu) {
        uint32_t count = EF_U16(0x146e50u);
        if (count >= 15u)
            renderer_partial_quicksort_c3fd4(1, (int32_t)count);
        if (count >= 2u) {
            renderer_insertion_sort_c409c();
            EF_U8(0xc41c4u) = 0x6eu;
        }
    }
    run_handler_stack_c425a();
    renderer_frame_callback_c4167(EF_U32(0xfc8a8u));
}

/* c41b0, 170 bytes including the patched branch. AX supplies the sort key.
 * The count saturates at
 * 0x800; overflow uses a different code path after the sort is made, by
 * changing the JGE rel8 displacement from 0x1b to 0x6e. */
void renderer_enqueue_c41b0(uint16_t key)
{
    uint16_t count = (uint16_t)(EF_U16(0x146e50u) + 1u);
    EF_U16(0x146e50u) = count;
    if (count < 0x800u) {
        uint16_t slot = EF_U16(0x146e52u);
        EF_U16(0x146e52u) = count;
        EF_U16(0x146e56u + 2u * slot) = key;
        EF_U16(0x147e56u + 2u * slot) = slot;
    } else if (EF_U8(0xc41c4u) == 0x6eu) {
        EF_U16(0x147e54u) = key;
        EF_U16(0x146e50u) = 0x800u;
        renderer_insertion_sort_c409c();
        EF_U16(0x146e52u) = EF_U16(0x148e54u);
    } else {
        uint32_t slot = EF_U16(0x146e52u);
        EF_U16(0x146e56u + 2u * slot) = key;
        EF_U16(0x147e56u + 2u * slot) = 0x7ffu;
        EF_U16(0x146e50u) = 0x800u;
        renderer_partial_quicksort_c3fd4(1, 0x800);
        renderer_insertion_sort_c409c();
        EF_U16(0x146e52u) = EF_U16(0x148e54u);
        EF_U8(0xc41c4u) = 0x6eu;
    }
}
