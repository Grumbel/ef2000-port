/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_mem.h"

#include <assert.h>
#include <string.h>

static uint8_t memory[0x430000];
static uint32_t calls[6];
static unsigned call_count;

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(memory));
    return memory + address;
}

void guest_tactcom_unbound_service_8b47c(uint32_t address)
{
    assert(call_count < 6u);
    calls[call_count++] = address;
}

static void check_case(uint8_t page_a, uint8_t page_b, uint8_t device,
                       const uint32_t *expected, size_t length)
{
    memset(memory, 0, sizeof(memory));
    memset(calls, 0, sizeof(calls));
    call_count = 0u;
    EF_U8(0x0015bac6u) = page_a;
    EF_U8(0x0015bac7u) = page_b;
    EF_U8(0x00420a72u) = device;
    EF_U32(0x0015b764u) = 0x12345678u;

    tactcom_after_input_8b47c();

    assert(call_count == length);
    assert(memcmp(calls, expected, length * sizeof(uint32_t)) == 0);
    assert(EF_U8(0x0015bac6u) == page_a);
    assert(EF_U8(0x0015bac7u) == page_b);
    assert(EF_U8(0x00420a72u) == device);
    assert(EF_U32(0x0015b764u) == 0x12345678u);
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
    return 0;
}
