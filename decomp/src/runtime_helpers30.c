/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Small callback trampolines, unsupported-feature stubs and checks, 0x39db0..0xed6c5. */

/* 0x39db0, 20 bytes. Allocate the four-byte slot table of the given length. */
void alloc_slot_table_39db0(uint8_t count)
{
    EF_U32(0x001a1860u) = ((uint32_t (*)(uint32_t, uint32_t))EF_U32(0x0012dd9cu))((uint32_t)count * 4u, 0);
}

/* 0x8204c, 18 bytes. Allocate a 12-byte node holding a value. */
ef_addr32 make_node_8204c(uint32_t value)
{
    ef_addr32 node = ((uint32_t (*)(uint32_t, uint32_t))EF_U32(0x0012dd9cu))(0xcu, value);

    EF_U32(node) = value;
    return node;
}

/* 0x84650, 17 bytes. Unsupported feature: abort and report failure. */
uint8_t unsupported_84650(void)
{
    ui_report_missing_record(0x00188c4cu);
    return 0;
}

/* 0xa292c, 26 bytes. Abort when the device is not ready; always reports ready. */
uint8_t require_device_ready_a292c(void)
{
    if (EF_U8(0x00135798u) == 0)
        ui_report_missing_record(0x0018cbe8u);
    return 1;
}

/* 0xa41a0, 16 bytes. Run the optional device idle callback. */
void run_idle_callback_a41a0(void)
{
    if (EF_U32(0x00136bb0u) != 0)
        ((void (*)(void))EF_U32(0x00136bb0u))();
}

/* 0xa7d34, 12 bytes. Remember the current clock. */
void remember_clock_a7d34(void)
{
    EF_U32(0x0041915cu) = ((uint32_t (*)(void))EF_U32(0x0015b620u))();
}

/* 0xab820, 21 bytes. Run the optional word callback with the low 16 bits of the argument. */
void run_word_callback_ab820(uint32_t value)
{
    if (EF_U32(0x0041ed80u) != 0)
        ((void (*)(uint32_t))EF_U32(0x0041ed80u))(value & 0xffffu);
}

/* 0xe1110, 29 bytes. Busy-wait for the given number of clock ticks. */
void wait_ticks_e1110(uint32_t ticks)
{
    uint32_t end = (ticks & 0xffffu) + ((uint32_t (*)(void))EF_U32(0x0015b620u))();

    while (((uint32_t (*)(void))EF_U32(0x0015b620u))() < end)
        ;
}

/* 0xe1730, 17 bytes. Unsupported input query: abort and report none. */
uint8_t unsupported_query_e1730(void)
{
    ui_report_missing_record(0x001906ccu);
    return 0;
}

/* 0xe1744, 17 bytes. Unsupported input query: abort and report none. */
uint8_t unsupported_query_e1744(void)
{
    ui_report_missing_record(0x001906f4u);
    return 0;
}

/* 0xe1758, 17 bytes. Unsupported input query: abort and report none. */
uint8_t unsupported_query_e1758(void)
{
    ui_report_missing_record(0x0019071cu);
    return 0;
}

/* 0xe1780, 17 bytes. Unsupported input query: abort and report none. */
uint8_t unsupported_query_e1780(void)
{
    ui_report_missing_record(0x0019075cu);
    return 0;
}

/* 0xe1948, 16 bytes. Abort with a message that includes the argument. */
void fatal_with_arg_e1948(uint32_t value)
{
    guest_fatal_arg(0x00190810u, value);
}

/* 0xeb25a, 24 bytes. Run the two page callbacks, then the page switch for the argument. */
void switch_page_eb25a(uint32_t page)
{
    ((void (*)(void))EF_U32(0x00160788u))();
    ((void (*)(void))EF_U32(0x0016078cu))();
    switch_page_eb272(page);
}

/* 0xeb272, 22 bytes. Run the page callback pair, then hand the argument to 0x20d. */
void switch_page_eb272(uint32_t page)
{
    ((void (*)(void))EF_U32(0x0016078cu))();
    ((void (*)(void))EF_U32(0x00160790u))();
    guest_fn_20d(page);
}

/* 0xeb35c, 24 bytes. Abort with the stored message when the error flag is set. */
void check_error_flag_eb35c(void)
{
    if (EF_U8(0x00160795u) != 0)
        ui_report_missing_record(0x00160796u);
}

/* 0xed6c5, 7 bytes. Forward to the callback at 0x17d498. */
void forward_callback_ed6c5(void)
{
    ((void (*)(void))EF_U32(0x0017d498u))();
}
