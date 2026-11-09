/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

/* Compose the *recovered* pre-main, far-stub, request-pool, keyboard and timer
 * C functions. The DOS/X-32 hardware entry points are strictly test doubles;
 * this is not a replacement for a guest differential or real vector setup. */

#include "ef_abi.h"
#include "ef_mem.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static uint8_t memory[0x500000];
static uint32_t requests[12][3];
static uint32_t exit_handlers[2];
static uint32_t installed_stub;
static unsigned request_count;
static unsigned canceled_count;
static unsigned exit_handler_count;
static unsigned far_stub_count;
static unsigned keyboard_count;
static unsigned timer_count;
static unsigned trap_count;
static int skip_trap;

void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < sizeof(memory));
    return memory + address;
}

static uint32_t packed_dword(ef_addr32 address)
{
    return (uint32_t)EF_U8(address)
         | ((uint32_t)EF_U8(address + 1u) << 8)
         | ((uint32_t)EF_U8(address + 2u) << 16)
         | ((uint32_t)EF_U8(address + 3u) << 24);
}

uint32_t guest_seg_x(uint32_t register_index)
{
    assert(register_index == 1u || register_index == 3u);
    return register_index == 1u ? 0x1234u : 0x5678u;
}

uint32_t guest_fn_a60(void)
{
    assert(EF_U8(0x00132a98u) == 1u);
    assert(far_stub_count == 0u);
    return 0x80u;
}

uint32_t guest_fn_510(uint32_t size)
{
    assert(size == 6u);
    ++far_stub_count;
    return 0x00400100u;
}

uint32_t guest_fn_604_x(uint32_t source)
{
    assert(source == 0x00400100u);
    return 0x00034000u;
}

void guest_fn_a9c(uint32_t first, uint32_t second)
{
    assert(first == 0x80u && second == 0x00034000u);
    assert(far_stub_count == 1u);
    ++installed_stub;
}

void guest_fn_9c3cf(uint32_t handler)
{
    assert(exit_handler_count < 2u);
    exit_handlers[exit_handler_count++] = handler;
}

uint32_t EF_CDECL guest_fn_9c220(uint32_t address, uint32_t selector,
                                 uint32_t length)
{
    uint32_t index;
    uint32_t slot;
    assert(request_count < 12u);
    if (request_count < 11u)
        assert(EF_U32(0x00196144u) == 15u - request_count);
    else
        assert(EF_U32(0x00196144u) == 5u); /* reused after release */
    index = EF_U8(0x00195fb0u + EF_U32(0x00196144u));
    slot = index * 10u;
    assert(packed_dword(0x00196022u + slot) == address);
    assert(EF_U16(0x00196026u + slot) == selector);
    assert(packed_dword(0x00196028u + slot) == length);
    requests[request_count][0] = address;
    requests[request_count][1] = selector;
    requests[request_count][2] = length;
    ++request_count;
    return 0u;
}

uint32_t EF_CDECL guest_fn_9c23d(uint32_t address, uint32_t selector,
                                 uint32_t length)
{
    assert(address == (canceled_count == 0u ? 0x000fe61fu : 0x000fe630u));
    assert(selector == 0x1234u);
    assert(length == (canceled_count == 0u ? 0xd5u : 0x19u));
    ++canceled_count;
    return 0u;
}

ef_addr32 getenv_9c858(ef_addr32 name)
{
    assert(name == 0x0017f3a8u);
    assert(request_count == 9u);
    return skip_trap ? 0x0017f3b0u : 0u;
}

void install_trap_handlers_9c8b4(uint32_t a, uint32_t b, uint32_t c,
                                 uint32_t d, uint32_t value)
{
    assert(a == 0u && b == 0u && c == 0u && d == 0u);
    assert(value == 0xd68u);
    ++trap_count;
}

void set_keyboard_leds_cd414(uint32_t mask)
{
    assert(mask == 0u);
    ++keyboard_count;
}

void init_keys_cd430(void)
{
    assert(keyboard_count == 1u);
    EF_U8(0x0041f831u) = 0x5au; /* caller-visible witness */
}

uint32_t store_selector_fe610(void)
{
    assert(keyboard_count == 1u);
    return 0u;
}

uint32_t guest_fn_324(uint32_t source)
{
    assert(source == 9u);
    return 0x2468u;
}

uint64_t guest_fn_348(uint32_t source)
{
    assert(source == 9u);
    return UINT64_C(0xabcd12345678);
}

void guest_fn_484(uint32_t source, uint32_t result_hi,
                  uint32_t flags, uint32_t selector)
{
    assert(source == 9u && result_hi == 0xabcdu);
    assert(flags == 0x000fe61fu && selector == 0x1234u);
    assert(EF_U32(0x00196140u) == 7u);
    assert(packed_dword(0x001960c4u + 7u * 14u) == 9u);
    assert(packed_dword(0x001960c8u + 7u * 14u) == 0x2468u);
    assert(packed_dword(0x001960ccu + 7u * 14u) == 0x12345678u);
    assert(EF_U16(0x001960d0u + 7u * 14u) == 0xabcdu);
    ++timer_count;
}

void guest_fatal_arg(ef_addr32 format, uint32_t value)
{
    (void)format;
    (void)value;
    abort();
}

void ui_report_missing_record(ef_addr32 message)
{
    (void)message;
    abort();
}

static void run_case(int env_present)
{
    static const uint32_t expected[11][3] = {
        { 0x000002b0u, 0x1234u, 0xb70u },
        { 0x0009be0du, 0x1234u, 0x1fu },
        { 0x0009c430u, 0x1234u, 0x1fu },
        { 0x0009c438u, 0x1234u, 0x1fu },
        { 0x0009c446u, 0x1234u, 0x1fu },
        { 0x0009c461u, 0x1234u, 0x1fu },
        { 0x0009c47au, 0x1234u, 0x1ffu },
        { 0x00000222u, 0x1234u, 0xfu },
        { 0x00120cd4u, 0x5678u, 2u },
        { 0x000fe61fu, 0x1234u, 0xd5u },
        { 0x0041f833u, 0x5678u, 0x118u },
    };
    unsigned i;
    memset(memory, 0, sizeof(memory));
    memset(requests, 0, sizeof(requests));
    memset(exit_handlers, 0, sizeof(exit_handlers));
    request_count = canceled_count = exit_handler_count = far_stub_count = 0;
    installed_stub = keyboard_count = timer_count = trap_count = 0;
    skip_trap = env_present;

    initialize_x32_services_e20();
    assert(EF_U8(0x00400100u) == 0x9au);
    assert(EF_U8(0x00400105u) == 0xcfu);
    assert(EF_U32(0x00196144u) == 7u);
    assert(installed_stub == 1u && far_stub_count == 1u);
    assert(exit_handler_count == 1u && exit_handlers[0] == 0xad8u);
    assert(trap_count == (unsigned)!env_present);

    initialize_input_services_cd4d4();
    assert(memcmp(requests, expected, sizeof(expected)) == 0);
    assert(EF_U32(0x00196144u) == 5u);
    assert(EF_U32(0x00196140u) == 7u);
    assert(EF_U32(0x0041f828u) == 6u);
    assert(EF_U32(0x0041f82cu) == 5u);
    assert(EF_U32(0x0041f824u) == 7u);
    assert(EF_U8(0x0041f831u) == 0x5au);
    assert(keyboard_count == 1u && timer_count == 1u);
    assert(exit_handler_count == 2u && exit_handlers[1] == 0xcd554u);
    assert(EF_U8(0x0015b72eu) == 1u);
    for (i = 0; i < 16u; ++i)
        assert(EF_U8(0x00195fb0u + i) == i);
    for (i = 0; i < 8u; ++i)
        assert(EF_U8(0x00196134u + i) == i);

    initialize_input_services_cd4d4(); /* warm branch is idempotent */
    assert(request_count == 11u);
    assert(timer_count == 1u);
    assert(keyboard_count == 1u);
    assert(exit_handler_count == 2u);

    /* Real 0x6b8 returns a ten-byte request to the free list; the next
     * real 0x628 must reuse that exact slot without touching the timer. */
    finish_request_6b8(6u);
    assert(canceled_count == 1u);
    assert(EF_U32(0x00196144u) == 6u);
    assert(EF_U8(0x00195fb0u + 5u) == 6u);
    assert(packed_dword(0x00196028u + 6u * 10u) == 0u);
    finish_request_6b8(6u);
    assert(canceled_count == 1u); /* cleared request is a no-op */
    assert(start_request_628(0x000fe630u, 0x1234u, 0x19) == 6u);
    assert(request_count == 12u);
    assert(EF_U32(0x00196144u) == 5u);
    assert(packed_dword(0x00196022u + 6u * 10u) == 0x000fe630u);
    assert(packed_dword(0x00196028u + 6u * 10u) == 0x19u);
    finish_request_6b8(6u);
    assert(canceled_count == 2u && EF_U32(0x00196144u) == 6u);
}

int main(void)
{
    run_case(0);
    run_case(1);
    return 0;
}
