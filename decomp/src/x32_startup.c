/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* 0xe20, 292 bytes. Pre-main X-32 vector, request and timer setup.
 *
 * This is game-owned startup code, not an implementation of X-32 interrupts.
 * Every request still goes through the original 0x628 allocator and its
 * extender call at 0x9c220. The separate GCC --probe-x32 path uses bounded
 * selector/far-allocation doubles and stops at that first real extender
 * request. --probe-x32-vectors uses an inert host request registry to get
 * through all nine registrations, stopping before trap installation.
 * --probe-x32-traps additionally models the saved CPU context and eight
 * interrupt descriptions, stopping at the return from this initializer.
 * Neither mode installs interrupts or implements a working CRT. */
void initialize_x32_services_e20(void)
{
    uint32_t i;
    uint32_t cs;
    uint32_t ds;

    EF_U8(0x00132a98u) = 1u;
    init_far_stub_b04();

    for (i = 0u; i < 16u; ++i)
        EF_U8(0x00195fb0u + i) = (uint8_t)i;
    EF_U32(0x00196144u) = 16u;

    for (i = 0u; i < 8u; ++i)
        EF_U8(0x00196134u + i) = (uint8_t)i;
    EF_U32(0x00196140u) = 8u;

    cs = guest_seg_x(1u);
    ds = guest_seg_x(3u);
    (void)start_request_628(0x000002b0u, cs, 0xe20 - 0x2b0);
    (void)start_request_628(0x0009be0du, cs, 0x1f);
    (void)start_request_628(0x0009c430u, cs, 0x1f);
    (void)start_request_628(0x0009c438u, cs, 0x1f);
    (void)start_request_628(0x0009c446u, cs, 0x1f);
    (void)start_request_628(0x0009c461u, cs, 0x1f);
    (void)start_request_628(0x0009c47au, cs, 0x1ff);
    (void)start_request_628(0x00000222u, cs, 0xf);

    /* CS:0x222 begins with 66 2e 8e 1d d4 0c 12 00 (mov ds,
     * cs:[0x120cd4]). The original checks the first four code bytes and
     * reads the following displacement at CS:0x226 via GS. For this exact
     * executable both are immutable, hence the final request uses 0x120cd4.
     * A different binary needs its own verified displacement. */
    (void)start_request_628(0x00120cd4u, ds, 2);

    if (getenv_9c858(0x0017f3a8u) == 0u) {
#if defined(EF_DECOMP_NATIVE)
        /* The strict virtual-registry mode stops here. A separate opt-in
         * trap probe supplies synthetic CPU context and a virtual table. */
        extern void ef_decomp_x32_before_traps(void);
        ef_decomp_x32_before_traps();
#endif
        install_trap_handlers_9c8b4(0u, 0u, 0u, 0u, 0xd68u);
    }
#if defined(EF_DECOMP_NATIVE)
    {
        extern void ef_decomp_x32_after_initializer(void);
        ef_decomp_x32_after_initializer();
    }
#endif
}
