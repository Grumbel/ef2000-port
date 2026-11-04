/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Handle table I/O, clock delta and checked lookups, 0xa75cc..0xc34cc.
 * The handle functions take their arguments on the stack with callee cleanup,
 * so four placeholder register parameters precede them. */

/* 0xa75cc, 46 bytes. Copy a name of at most 16 characters into an object. */
void set_object_name_a75cc(ef_addr32 object, ef_addr32 name)
{
    if (object != 0) {
        if (guest_string_length(name) > 0x10)
            guest_report_parser_enum_error(0x0018d290u, name, 0x10u);
        copy_string_9d749(object + 0x3fu, name);
    }
}

/* 0xa78bc, 61 bytes. Find a widget by name in a root or abort. */
ef_addr32 find_widget_checked_a78bc(ef_addr32 root, ef_addr32 name)
{
    ef_addr32 widget;

    if (root == 0)
        guest_fatal_arg(0x0018d4c0u, name);
    widget = guest_fn_a788c(root, name);
    if (widget == 0)
        guest_report_parser_enum_error(0x0018d4f8u, name, root);
    return widget;
}

/* 0xa7990, 65 bytes. Close a handle (100..105) and clear its slot. */
void close_handle_a7990(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t stack_handle)
{
    uint32_t index = stack_handle;

    (void)u0; (void)u1; (void)u2; (void)u3;
    if ((int32_t)index < 0x64 || (int32_t)index > 0x69 || EF_U32(0x00137014u + index * 4u) == 0)
        ui_report_missing_record(0x0018d534u);
    guest_fn_bc421(EF_U32(0x00137014u + index * 4u), index - 0x64u, 0);
    EF_U32(0x001371a4u + (index - 0x64u) * 4u) = 0;
}

/* 0xa79d4, 69 bytes. Read from a handle (100..105). */
uint32_t read_handle_a79d4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3,
                           uint32_t stack_handle, uint32_t stack_count, uint32_t stack_buffer)
{
    (void)u0; (void)u1; (void)u2; (void)u3;
    if ((int32_t)stack_handle < 0x64 || (int32_t)stack_handle > 0x69
        || EF_U32(0x00137014u + stack_handle * 4u) == 0)
        ui_report_missing_record(0x0018d534u);
    return guest_fn_f1d90(stack_buffer, 1, stack_count, EF_U32(0x001371a4u + (stack_handle - 0x64u) * 4u));
}

/* 0xa7a1c, 69 bytes. Write to a handle (100..105). */
uint32_t write_handle_a7a1c(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3,
                            uint32_t stack_handle, uint32_t stack_count, uint32_t stack_buffer)
{
    (void)u0; (void)u1; (void)u2; (void)u3;
    if ((int32_t)stack_handle < 0x64 || (int32_t)stack_handle > 0x69
        || EF_U32(0x00137014u + stack_handle * 4u) == 0)
        ui_report_missing_record(0x0018d534u);
    return guest_fn_f1f5a(stack_buffer, 1, stack_count, EF_U32(0x001371a4u + (stack_handle - 0x64u) * 4u));
}

/* 0xa7a64, 77 bytes. Seek a handle (100..105) and flush it. */
void seek_handle_a7a64(uint32_t first, uint32_t second, uint32_t handle)
{
    if ((int32_t)handle < 0x64 || (int32_t)handle > 0x69 || EF_U32(0x00137014u + handle * 4u) == 0)
        ui_report_missing_record(0x0018d534u);
    guest_fn_f2183(EF_U32(0x001371a4u + (handle - 0x64u) * 4u), first, second);
    guest_fn_f227f(EF_U32(0x001371a4u + (handle - 0x64u) * 4u));
}

/* 0xa7d40, 65 bytes. Clock ticks since the last call, clamped to 1..50. */
int32_t clock_delta_a7d40(void)
{
    uint32_t now = ((uint32_t (*)(void))EF_U32(0x0015b620u))();
    uint32_t last = EF_U32(0x0041915cu);
    int16_t delta = (int16_t)(now - last);
    int32_t value;

    EF_U32(0x0041915cu) = now;
    value = delta < 0x32 ? delta : 0x32;
    if (value <= 1)
        return 1;
    return delta < 0x32 ? delta : 0x32;
}

/* 0xabde4, 61 bytes. Run the callback of every registered 21-byte entry. */
void run_entry_callbacks_abde4(void)
{
    uint16_t i;

    for (i = 0; i < EF_U16(0x0041eeccu); i++) {
        uint32_t offset = (uint32_t)i * 0x15u;

        if (EF_U32(offset + 0x0041e96du) != 0)
            ((void (*)(uint32_t))EF_U32(offset + 0x0041e96du))(0);
    }
}

/* 0xac7c8, 30 bytes. Print a diagnostic and abort with the given argument. */
void report_fatal_ac7c8(uint32_t value)
{
    guest_fn_de648(0x004195d0u, value);
    guest_fatal_arg(0x0018dc60u, value);
}

/* 0xb3d00, 96 bytes. Write a whole file; abort when it cannot be created or written. */
uint8_t write_whole_file_b3d00(ef_addr32 name, uint32_t buffer, uint32_t count)
{
    uint32_t handle = guest_fn_a7948(name);

    if (handle != 0xffffffffu) {
        if (write_handle_a7a1c(0, 0, 0, 0, handle, count, buffer) == count) {
            close_handle_a7990(0, 0, 0, 0, handle);
            return 0;
        }
        close_handle_a7990(0, 0, 0, 0, handle);
        guest_fatal_arg(0x0018e734u, name);
    }
    guest_fatal_arg(0x0018e748u, name);
    return 0;
}

/* 0xb3f58, 74 bytes. Look up a 1-based id in the table at [0x41f370] or abort. */
uint32_t lookup_id_b3f58(int16_t id)
{
    uint32_t index = ((uint32_t)(int32_t)id - 1u) & 0xffffu;

    index -= EF_U32(0x00146e2cu);
    if (index < EF_U16(0x00146e24u))
        return EF_U32(EF_U32(0x0041f370u) + index * 4u);
    guest_report_parser_enum_error(0x0018e814u, (uint32_t)(int32_t)id, EF_U32(0x00146e08u));
    return 0;
}

/* 0xc32dc, 76 bytes. Allocate the work block from either pool depending on its size. */
void alloc_work_block_c32dc(int32_t size)
{
    if ((int32_t)guest_fn_e2b54() > size) {
        EF_U8(0x00146e3cu) = 1;
        EF_U32(0x0041f3bcu) = guest_fn_e2a94((uint32_t)size);
        return;
    }
    EF_U8(0x00146e3cu) = 2;
    EF_U32(0x0041f3bcu) = guest_fn_c3f70((uint32_t)size);
    if (EF_U32(0x0041f3bcu) == 0)
        ui_report_missing_record(0x0018ee18u);
}

/* 0xc34cc, 65 bytes. Start the driver unless it is already busy; abort when it fails. */
uint32_t start_driver_c34cc(uint32_t request)
{
    if ((guest_fn_c3430(request) & 0xffu) != 0)
        return 0;
    if ((((uint32_t (*)(void))EF_U32(0x0041f3c4u))() & 0xffu) == 0)
        guest_fatal_arg(0x0018ee4cu, request);
    ((void (*)(void))EF_U32(0x0041f3ccu))();
    ((void (*)(void))EF_U32(0x0041f3a4u))();
    guest_fn_c3328();
    return 1;
}
