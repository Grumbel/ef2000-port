/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Tiny guarded forwarders. */

/* 0xdd8c0, 14 bytes. Run 0xdd8dc when the saved width at 0x15ee94 is -1 (nothing saved yet); otherwise do nothing. */
void redraw_if_unsaved_dd8c0(void)
{
    if (EF_S16(0x0015ee94u) == -1)
        save_and_draw_block_dd8dc();
}

/* 0x101f78, 14 bytes. Forward a stack argument to 0x1111e5. */
void forward_to_1111e5_101f78(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t value)
{
    (void)u0; (void)u1; (void)u2; (void)u3;
    guest_fn_1111e5_x(value);
}

/* 0x108ffa, 35 bytes. Decrement a use counter (+0xc) and, while it was non-zero, call the object's handler (first dword); returns the handler result, or -1 when the counter was zero or flag 2 (+0x10) is set. */
uint32_t release_ref_108ffa(ef_addr32 obj)
{
    uint32_t count = EF_U32(obj + 0xcu);
    uint32_t result;
    EF_U32(obj + 0xcu) = count - 1u;
    if (count == 0u)
        return 0xffffffffu;
    result = guest_callptr_eax_ret_x(EF_U32(obj), obj);
    if (EF_U8(obj + 0x10u) & 2u)
        return 0xffffffffu;
    return result;
}

/* 0x109328, 36 bytes. Check that the byte at *cursor equals expected (printing a message through 0x132a74 if not) and advance the cursor. */
void expect_byte_109328(ef_addr32 cursor, uint32_t expected)
{
    uint32_t p = EF_U32(cursor);
    if ((uint8_t)expected != EF_U8(p))
        guest_callptr2_x(EF_U32(0x00132a74u), 0x00194ffcu, expected & 0xffu);
    EF_U32(cursor) = p + 1u;
}

/* 0x11b1b0, 36 bytes. Forward four stack arguments to 0x11b134. */
void forward_to_11b134_11b1b0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    (void)u0; (void)u1; (void)u2; (void)u3;
    guest_fn_11b134_x(a0, a1, a2, a3);
}

/* 0x9e094, 45 bytes. Run 0x9e0c4 with the hook at 0x132b14 pointing to 0x9e08c and the stack argument saved at 0x41714c, then restore the default hook 0x16010e. */
void run_with_hook_9e094(uint32_t value, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t arg)
{
    (void)u1; (void)u2; (void)u3;
    EF_U32(0x0041714cu) = arg;
    EF_U32(0x00132b14u) = 0x0009e08cu;
    guest_fn_9e0c4_x((int32_t)(int16_t)value);
    EF_U32(0x00132b14u) = 0x0016010eu;
}

/* 0xb1bf8, 57 bytes. Print the two status lines (0x1264e2; 0x1a364c and 0x1a364b) to a handle. */
void print_status_lines_b1bf8(uint32_t handle)
{
    print_to_handle_b1bb0(handle, 0x0018de34u, (uint32_t)EF_U8(0x001264e2u));
    print_to_handle_b1bb0(handle, 0x0018de44u, (uint32_t)EF_U8(0x001a364cu), (uint32_t)EF_U8(0x001a364bu));
}

/* 0xb3374, 56 bytes. Format a message from the global at 0x197674, the argument and the latch value, and pass it to 0xcc744. */
uint32_t report_message_b3374(uint32_t value)
{
    uint8_t buf[0xc8];
    uint32_t latched = latch_and_forward_b33ac();
    guest_sprintf_a4342((uint32_t)(uintptr_t)buf, 0x0018e5d0u, EF_U32(0x00197674u), value, latched);
    return alloc_checked_cc744((uint32_t)(uintptr_t)buf);
}

/* 0x65a50, 56 bytes. Dispatch on the mode byte at 0x1282c5 (0..8) to the matching refresh routine, then flag the display dirty. */
void refresh_for_mode_65a50(void)
{
    switch (EF_U8(0x001282c5u)) {
    case 0: case 1: case 6: guest_fn_65a88_x(); break;
    case 2: store_631bc_65d18(); break;
    case 7: guest_fn_65d24_x(); break;
    case 8: guest_fn_65c1c_x(); break;
    default: break;
    }
    EF_U8(0x001a8246u) = 1u;
}
