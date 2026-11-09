/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_mem.h"

#include <assert.h>
#include <setjmp.h>
#include <string.h>

static uint8_t memory[0x470000];
static uint32_t calls[8];
static unsigned call_count;
static unsigned gameport_calls;
static uint32_t stop_at;
static jmp_buf stopped;

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(memory));
    return memory + address;
}

void guest_tactcom_unbound_service_8b47c(uint32_t address)
{
    assert(call_count < 8u);
    calls[call_count++] = address;
    if (address == stop_at)
        longjmp(stopped, 1);
}

uint32_t guest_fn_d6694(void)
{
    gameport_calls++;
    return 0u;
}

static void check_case(uint8_t page_a, uint8_t page_b, uint8_t device,
                       const uint32_t *expected, size_t length)
{
    memset(memory, 0, sizeof(memory));
    memset(calls, 0, sizeof(calls));
    call_count = 0u;
    gameport_calls = 0u;
    stop_at = 0xe28e8u;
    EF_U8(0x0015bac6u) = page_a;
    EF_U8(0x0015bac7u) = page_b;
    EF_U8(0x00420a72u) = device;
    EF_U32(0x0015b764u) = 0x12345678u;

    if (setjmp(stopped) == 0) {
        tactcom_after_input_8b47c();
        assert(!"should stop at uninitialized timer boundary");
    }

    assert(call_count == length);
    assert(memcmp(calls, expected, length * sizeof(uint32_t)) == 0);
    assert(EF_U8(0x0015bac6u) == page_a);
    assert(EF_U8(0x0015bac7u) == page_b);
    assert(EF_U8(0x00420a72u) == device);
    assert(EF_U32(0x0015b764u) == 0x12345678u);
    assert(gameport_calls == 0u);
}

/* Treat timer and hardware probes as services which have completed: verify
 * the original 0x8b4ce dispatch and real 0x1d3c4 table selection, ending
 * after 0x1c60c observes a synthetic disconnected gameport. */
static void check_mode(uint16_t mode, uint16_t result,
                       uint32_t first, uint32_t second)
{
    memset(memory, 0, sizeof(memory));
    call_count = 0u;
    gameport_calls = 0u;
    stop_at = 0u;
    EF_U32(0x00464470u) = 0x00005a5au;
    EF_U16(0x00464472u) = mode;

    tactcom_after_input_8b47c();

    assert(call_count == 2u);
    assert(calls[0] == 0xe28e8u);
    assert(calls[1] == 0xe9ac0u);
    assert(gameport_calls == 1u);
    assert(EF_U16(0x00464472u) == result);
    assert(EF_U16(0x00464470u) == 0x5a5au);
    assert(EF_U32(0x0019840cu) == first);
    assert(EF_U32(0x00198410u) == second);
}

int main(void)
{
    static const uint32_t plain[] = { 0xe28e8u };
    static const uint32_t page_a[] = { 0x7feb0u, 0xe28e8u };
    static const uint32_t page_b[] = { 0x81110u, 0xe28e8u };
    static const uint32_t both[] = { 0x7feb0u, 0x81110u, 0xe28e8u };
    static const uint32_t input[] = { 0x97f90u, 0x98000u, 0xe28e8u };
    static const uint32_t input2[] = {
        0x982a0u, 0x97f90u, 0x98000u, 0xe28e8u
    };
    static const uint32_t all[] = {
        0x7feb0u, 0x81110u, 0x982a0u, 0x97f90u, 0x98000u, 0xe28e8u
    };

    check_case(0u, 0u, 0u, plain, 1u);
    check_case(1u, 0u, 0u, page_a, 2u);
    check_case(0u, 1u, 0u, page_b, 2u);
    check_case(1u, 1u, 0u, both, 3u);
    check_case(0u, 0u, 1u, input, 3u);
    check_case(0u, 0u, 2u, input2, 4u);
    check_case(0u, 0u, 3u, input, 3u);
    check_case(1u, 1u, 2u, all, 6u);

    check_mode(0u, 0u, 0u, 0u);
    check_mode(1u, 0x30u, 0x1a1d8u, 0x1d3b0u);
    check_mode(2u, 0x46u, 0x1a1e4u, 0x1d3bcu);
    check_mode(3u, 0x47u, 0x1a1f0u, 0x1d3acu);
    check_mode(4u, 0u, 0u, 0u);
    check_mode(0xffffu, 0u, 0u, 0u);
    return 0;
}
