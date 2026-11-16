/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

typedef struct {
    uint8_t conditional, variable[4], comparison, value[4], subroutine;
    uint8_t reserved[2], target[4];
} ef_script_branch_prefix;

EF_STATIC_ASSERT(script_branch_conditional, offsetof(ef_script_branch_prefix, conditional) == 0);
EF_STATIC_ASSERT(script_branch_variable, offsetof(ef_script_branch_prefix, variable) == 1);
EF_STATIC_ASSERT(script_branch_comparison, offsetof(ef_script_branch_prefix, comparison) == 5);
EF_STATIC_ASSERT(script_branch_value, offsetof(ef_script_branch_prefix, value) == 6);
EF_STATIC_ASSERT(script_branch_subroutine, offsetof(ef_script_branch_prefix, subroutine) == 0x0a);
EF_STATIC_ASSERT(script_branch_target, offsetof(ef_script_branch_prefix, target) == 0x0d);

typedef struct {
    uint8_t message[0x64], variable[0x18], target[0x14];
    uint8_t action[0x0c], comparison[0x0c];
} ef_script_branch_frame;

EF_STATIC_ASSERT(script_branch_frame_variable, offsetof(ef_script_branch_frame, variable) == 0x64);
EF_STATIC_ASSERT(script_branch_frame_target, offsetof(ef_script_branch_frame, target) == 0x7c);
EF_STATIC_ASSERT(script_branch_frame_action, offsetof(ef_script_branch_frame, action) == 0x90);
EF_STATIC_ASSERT(script_branch_frame_comparison, offsetof(ef_script_branch_frame, comparison) == 0x9c);
EF_STATIC_ASSERT(script_branch_frame_size, sizeof(ef_script_branch_frame) == 0xa8);

/* Keep each original DS -> ES string-copy shape and the live DF. */
#ifdef __WATCOMC__
static void branch_copy3(ef_addr32 destination, ef_addr32 source);
#pragma aux branch_copy3 = "movsw" "movsb" parm [edi] [esi] modify [edi esi]
static void branch_copy6(ef_addr32 destination, ef_addr32 source);
#pragma aux branch_copy6 = "movsd" "movsw" parm [edi] [esi] modify [edi esi]
static void branch_copy7(ef_addr32 destination, ef_addr32 source);
#pragma aux branch_copy7 = "movsd" "movsw" "movsb" parm [edi] [esi] modify [edi esi]
static void branch_copy8(ef_addr32 destination, ef_addr32 source);
#pragma aux branch_copy8 = "movsd" "movsd" parm [edi] [esi] modify [edi esi]
static void branch_copy9(ef_addr32 destination, ef_addr32 source);
#pragma aux branch_copy9 = "movsd" "movsd" "movsb" parm [edi] [esi] modify [edi esi]
#else
extern void branch_copy3(ef_addr32 destination, ef_addr32 source);
extern void branch_copy6(ef_addr32 destination, ef_addr32 source);
extern void branch_copy7(ef_addr32 destination, ef_addr32 source);
extern void branch_copy8(ef_addr32 destination, ef_addr32 source);
extern void branch_copy9(ef_addr32 destination, ef_addr32 source);
#endif

static uint8_t branch_key(void)
{
    /* The IRQ handler changes this byte while the original busy-waits. */
    return *(volatile uint8_t *)ef_guest_memory_ptr(0x0041f833u);
}

static void take_script_branch(ef_addr32 record, uint32_t prompted)
{
    if (EF_U8(record + 0x0au) == 1u) {
        uint32_t index = (uint32_t)(EF_S32(0x004194aau) >> 16);

        /* Preserve the two original store orders, including overflow aliasing
         * between the unchecked return table and its count word. */
        if (prompted == 0u) {
            EF_U32(0x00418e38u + index * 4u) = EF_U32(0x004191dcu);
            EF_U16(0x004194acu) = (uint16_t)(EF_U16(0x004194acu) + 1u);
        } else {
            uint16_t count = EF_U16(0x004194acu);
            ef_addr32 instruction = EF_U32(0x004191dcu);

            EF_U16(0x004194acu) = (uint16_t)(count + 1u);
            EF_U32(0x00418e38u + index * 4u) = instruction;
        }
    }
    EF_U32(0x004191ecu) = EF_U32(record + 0x0du);
}

/* 0xa9130, 794 bytes. EAX=record, preserve all other GPRs, plain RET.
 * Sole direct caller: opcode3 operand at a9f73/a9f76; EAX result unused. */
void execute_script_branch_a9130(ef_addr32 record)
{
    ef_script_branch_frame frame;
    ef_addr32 local = (ef_addr32)(uintptr_t)&frame;
    ef_addr32 target;
    uint32_t index;
    uint8_t mode;

    if (EF_U8(record) == 0u) {
        take_script_branch(record, 0u);
        return;
    }
    if (EF_U8(record + 5u) == 0u && EF_U8(0x001371f9u) != 0u) {
        script_if_equal_a8f80(record);
        return;
    }
    if (EF_U8(record + 5u) == 1u && EF_U8(0x001371f9u) != 0u) {
        script_if_not_equal_a8fcc(record);
        return;
    }
    if (EF_U8(record + 5u) == 3u && EF_U8(0x001371f9u) != 0u) {
        script_if_not_greater_a9014(record);
        return;
    }
    guest_callptr_eaxedx_x(EF_U32(0x00132ab8u), 0x0018d698u, 0x00464060u);
    toggle_page_dc250();
    clear_buffer_dc2c0();
    copy_string_9d749(local + 0x64u, EF_U32(record + 1u) + 9u);
    mode = EF_U8(record + 5u);
    switch (mode) {
    case 0: branch_copy3(local + 0x9cu, 0x0018d6d4u); break;
    case 1: branch_copy3(local + 0x9cu, 0x0018d6d8u); break;
    case 2: branch_copy3(local + 0x9cu, 0x0018d6dcu); break;
    case 3: branch_copy3(local + 0x9cu, 0x0018d6e0u); break;
    case 4: EF_U16(local + 0x9cu) = EF_U16(0x0018d6e4u); break;
    case 5: EF_U16(local + 0x9cu) = EF_U16(0x0018d6e8u); break;
    default: break; /* Original leaves this local buffer unwritten. */
    }
    if (EF_U8(record + 0x0au) == 0u)
        branch_copy6(local + 0x90u, 0x0018d6ecu);
    else
        branch_copy7(local + 0x90u, 0x0018d6f4u);

    target = EF_U32(record + 0x0du);
    switch (EF_U8(target)) {
    case 0: case 1: case 6:
        copy_string_9d749(local + 0x7cu, EF_U32(target + 9u));
        break;
    case 2: branch_copy9(local + 0x7cu, 0x0018d6fcu); break;
    case 3: branch_copy9(local + 0x7cu, 0x0018d708u); break;
    case 5: branch_copy8(local + 0x7cu, 0x0018d714u); break;
    case 8: branch_copy8(local + 0x7cu, 0x0018d71cu); break;
    case 15: branch_copy8(local + 0x7cu, 0x0018d724u); break;
    default: break; /* Original leaves this local buffer unwritten. */
    }
    guest_sprintf_a4342(local, 0x0018d72cu,
                       local + 0x64u, local + 0x9cu, EF_U32(record + 6u));
    strcat_9ca57(local, local + 0x90u);
    strcat_9ca57(local, local + 0x7cu);
    index = 0u;
    /* Re-evaluate strlen each iteration, as in the original. */
    while (index < guest_fn_b1b8b(local)) {
        uint32_t c = EF_U8(local + index);

        index++;
        EF_U8(local + index - 1u) = (uint8_t)to_upper_ascii((int32_t)c);
    }
    draw_text_f29ec(0xffu, 0u, 0u, local);
    draw_text_f23ac(0xffu, 0, 10, 0x0018d740u);
    guest_sprintf_a4342(local, 0x0018d764u,
                       EF_U32(lookup_key_a7ec4(local + 0x64u)));
    draw_text_f23ac(0xffu, 0, 20, local);
    toggle_page_dc250();
    apply_palette_d663c(0x00464060u);
    EF_U8(0x0041f833u) = 0u;
    while (branch_key() != 0x39u && branch_key() != 0x1cu) {
    }
    if (branch_key() == 0x39u)
        take_script_branch(record, 1u);
    set_black_palette_d65e8();
}
