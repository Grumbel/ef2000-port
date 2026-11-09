/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_mem.h"

#include <assert.h>
#include <string.h>

static uint8_t memory[0x1a0000];
static uint32_t requests[9][3];
static unsigned request_count;
static unsigned far_stub_count;
static unsigned env_lookup_count;
static unsigned trap_count;
static int have_trap_environment;

void *ef_guest_memory_ptr(ef_addr32 addr)
{
    assert(addr < sizeof(memory));
    return memory + addr;
}

void init_far_stub_b04(void)
{
    assert(++far_stub_count == 1);
    assert(EF_U8(0x00132a98u) == 1u);
    assert(EF_U32(0x00196144u) == 0xfeedf00du);
}

uint32_t guest_seg_x(uint32_t selector)
{
    assert(selector == 1u || selector == 3u);
    return selector == 1u ? 0x1234u : 0x5678u;
}

uint32_t start_request_628(uint32_t address, uint32_t selector, int32_t length)
{
    uint32_t remaining;
    uint32_t slot;
    assert(far_stub_count == 1u);
    assert(request_count < 9u);
    assert(EF_U32(0x00196140u) == 8u);
    remaining = EF_U32(0x00196144u) - 1u;
    assert(remaining == 15u - request_count);
    EF_U32(0x00196144u) = remaining;
    slot = EF_U8(0x00195fb0u + remaining);
    assert(slot == remaining);
    requests[request_count][0] = address;
    requests[request_count][1] = selector;
    requests[request_count][2] = (uint32_t)length;
    ++request_count;
    return slot;
}

ef_addr32 getenv_9c858(ef_addr32 name)
{
    assert(name == 0x0017f3a8u);
    assert(request_count == 9u);
    ++env_lookup_count;
    return have_trap_environment ? 0x001a0000u : 0u;
}

void install_trap_handlers_9c8b4(uint32_t a, uint32_t b, uint32_t c,
                                 uint32_t d, uint32_t address)
{
    assert(a == 0u && b == 0u && c == 0u && d == 0u);
    assert(address == 0xd68u);
    assert(env_lookup_count == 1u);
    ++trap_count;
}

static void run_case(int present)
{
    static const uint32_t expected[9][3] = {
        { 0x000002b0u, 0x1234u, 0xb70u },
        { 0x0009be0du, 0x1234u, 0x1fu },
        { 0x0009c430u, 0x1234u, 0x1fu },
        { 0x0009c438u, 0x1234u, 0x1fu },
        { 0x0009c446u, 0x1234u, 0x1fu },
        { 0x0009c461u, 0x1234u, 0x1fu },
        { 0x0009c47au, 0x1234u, 0x1ffu },
        { 0x00000222u, 0x1234u, 0xfu },
        { 0x00120cd4u, 0x5678u, 2u },
    };
    unsigned i;
    memset(memory, 0xa5, sizeof(memory));
    memset(requests, 0, sizeof(requests));
    request_count = far_stub_count = env_lookup_count = trap_count = 0;
    have_trap_environment = present;
    EF_U32(0x00196144u) = 0xfeedf00du;

    initialize_x32_services_e20();

    assert(EF_U8(0x00132a98u) == 1u);
    assert(far_stub_count == 1u);
    assert(request_count == 9u);
    assert(EF_U32(0x00196144u) == 7u);
    assert(EF_U32(0x00196140u) == 8u);
    assert(memcmp(requests, expected, sizeof(expected)) == 0);
    for (i = 0; i < 16u; ++i)
        assert(EF_U8(0x00195fb0u + i) == i);
    for (i = 0; i < 8u; ++i)
        assert(EF_U8(0x00196134u + i) == i);
    assert(EF_U8(0x00195fc0u) == 0xa5u);
    assert(EF_U8(0x0019613cu) == 0xa5u);
    assert(env_lookup_count == 1u);
    assert(trap_count == (unsigned)!present);
}

int main(void)
{
    run_case(0);
    run_case(1);
    return 0;
}
