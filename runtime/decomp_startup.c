/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

/* Minimal Linux bindings for the first game-owned startup call at 0xe29a0.
 * These are narrowly scoped host services; do not treat them as a complete
 * implementation of the original X-32 heap and Watcom C runtime. */

#include "decomp_startup.h"
#include "decomp_image.h"
#include "ef_abi.h"
#include "ef_mem.h"

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t next_allocation;
static uint32_t requested_arena;
static uint32_t release_hook;
static uint32_t hook_registrations;
static int probing;
static int arena_allocated;
static jmp_buf main_boundary;
static uint32_t main_boundary_address;
static int main_probe_active;

static void fatal_unexpected(const char *service)
{
    fprintf(stderr, "ef2000-decomp: unexpected startup service: %s\n", service);
    abort();
}

void guest_fatal_arg(ef_addr32 format, uint32_t value)
{
    fprintf(stderr, "ef2000-decomp: guest startup fatal message at 0x%08x (arg 0x%08x)\n",
            format, value);
    abort();
}

/* Original 0xc3f70 is a jump to the extender-backed allocator. During this
 * narrowly bounded probe only its single arena allocation is expected. */
uint32_t guest_fn_c3f70(uint32_t size)
{
    if (!probing || arena_allocated || size != requested_arena ||
        !ef_decomp_memory_span(next_allocation, size))
        fatal_unexpected("arena allocation (0xc3f70)");
    arena_allocated = 1;
    return next_allocation;
}

/* The original registers 0xe2a50 as a cleanup handler for the arena. Keep
 * the guest offset, not a truncated host function pointer. */
void guest_fn_9c3cf(uint32_t handler)
{
    if (!probing || handler != 0xe2a50u || hook_registrations)
        fatal_unexpected("cleanup registration (0x9c3cf)");
    release_hook = handler;
    hook_registrations++;
}

/* Bind the original call to the actually recovered C implementation. */
void guest_fn_e2a1c(void)
{
    if (!probing)
        fatal_unexpected("arena reset (0xe2a1c)");
    reset_buffer_e2a1c();
}

void guest_fn_b3fb0(uint32_t destination, uint32_t value, uint32_t count)
{
    void *span;
    if (!probing || !(span = ef_decomp_memory_span(destination, count)))
        fatal_unexpected("bounded guest fill (0xb3fb0)");
    memset(span, (int)(uint8_t)value, count);
}

static void stop_at_guest_boundary(uint32_t address)
{
    if (!main_probe_active)
        fatal_unexpected("game entry boundary outside a main probe");
    main_boundary_address = address;
    longjmp(main_boundary, 1);
}

void guest_fn_8b39c(void) { stop_at_guest_boundary(0x8b39cu); }
void guest_fn_8b2d8(void) { stop_at_guest_boundary(0x8b2d8u); }

/* These later entry points are deliberately *not* implemented as native
 * services. Reaching them before TACTCOM has been recovered is an error. */
void guest_fn_ea470(uint32_t mode)
{
    (void)mode;
    fatal_unexpected("exit handler before TACTCOM (0xea470)");
}

void guest_fn_ea270(ef_addr32 descriptor)
{
    (void)descriptor;
    fatal_unexpected("resource loader before TACTCOM (0xea270)");
}

uint32_t guest_fn_aaee8(uint32_t argc, ef_addr32 argv)
{
    (void)argc;
    (void)argv;
    fatal_unexpected("main control before TACTCOM (0xaaee8)");
    return 0;
}

static int prepare_arena(uint32_t bss_end, char *reason, size_t reason_capacity)
{
    uint32_t arena_size, base;
    if (bss_end < 0x0015f76cu ||
        !ef_decomp_memory_span(0x001264d4u, sizeof(uint32_t))) {
        if (reason && reason_capacity)
            snprintf(reason, reason_capacity, "guest RAM is not initialized");
        return -1;
    }
    arena_size = EF_U32(0x001264d4u);
    base = (bss_end + 0xfffu) & ~0xfffu;
    if (EF_U32(0x0015f75cu) != 0 || arena_size == 0 ||
        arena_size > EF_DECOMP_RAM_SIZE - base) {
        if (reason && reason_capacity)
            snprintf(reason, reason_capacity, "arena startup preconditions not met");
        return -1;
    }
    next_allocation = base;
    requested_arena = arena_size;
    hook_registrations = 0;
    release_hook = 0;
    arena_allocated = 0;
    probing = 1;
    return 0;
}

static int check_arena(ef_decomp_arena_probe *probe,
                       char *reason, size_t reason_capacity)
{
    uint32_t base = next_allocation;
    uint32_t arena_size = requested_arena;
    if (!probe || !arena_allocated || hook_registrations != 1 ||
        release_hook != 0xe2a50u ||
        EF_U32(0x0015f75cu) != base || EF_U32(0x0015f760u) != arena_size ||
        EF_U32(0x0015f754u) != base || EF_U32(0x0015f758u) != arena_size ||
        EF_U32(0x0015f764u) != 0 || EF_U32(0x0015f768u) != 0) {
        if (reason && reason_capacity)
            snprintf(reason, reason_capacity, "recovered arena initialization mismatch");
        return -1;
    }
    probe->base = base;
    probe->size = arena_size;
    probe->release_hook = release_hook;
    probe->hook_registrations = hook_registrations;
    return 0;
}

int ef_decomp_probe_arena(uint32_t bss_end, ef_decomp_arena_probe *probe,
                          char *reason, size_t reason_capacity)
{
    int result;
    if (!probe || prepare_arena(bss_end, reason, reason_capacity) != 0)
        return -1;
    init_arena_e29a0();
    probing = 0;
    result = check_arena(probe, reason, reason_capacity);
    return result;
}

int ef_decomp_probe_main(uint32_t bss_end, uint32_t argc, uint32_t argv,
                         ef_decomp_arena_probe *probe, uint32_t *stop_address,
                         char *reason, size_t reason_capacity)
{
    uint32_t stopped;
    int result;
    if (!probe || !stop_address || prepare_arena(bss_end, reason, reason_capacity) != 0)
        return -1;
    main_probe_active = 1;
    main_boundary_address = 0;
    if (setjmp(main_boundary) == 0)
        (void)candidate_game_main(argc, argv);
    stopped = main_boundary_address;
    main_probe_active = 0;
    probing = 0;
    if (stopped != 0x8b39cu && stopped != 0x8b2d8u) {
        if (reason && reason_capacity)
            snprintf(reason, reason_capacity, "game main did not reach an expected boundary");
        return -1;
    }
    *stop_address = stopped;
    if (stopped == 0x8b2d8u) {
        /* The original playvideo branch skips arena initialization. */
        if (arena_allocated || hook_registrations)
            return -1;
        memset(probe, 0, sizeof(*probe));
        return 0;
    }
    result = check_arena(probe, reason, reason_capacity);
    if (result != 0)
        return -1;
    return 0;
}
