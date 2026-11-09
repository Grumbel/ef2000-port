/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x8b274, 100 bytes. The original game's entry after Watcom __CMain.
 * Reconstructed from the actual x86 call sequence at 0x8b274..0x8b2d7.
 * Unrecovered callees remain explicit boundaries; the Linux startup probe
 * stops at the first one, 0x8b39c, rather than executing original bytes. */
uint32_t candidate_game_main(uint32_t argc, ef_addr32 argv)
{
    if (argc == 2u &&
        ascii_stricmp_9d524(0x0018a7e8u, EF_U32(argv + 4u)) == 0) {
        guest_fn_8b2d8();
        return 0;
    }

    init_arena_e29a0();
    guest_fn_8b39c();
    guest_fn_ea470(1u);
    guest_fn_ea270(0x001603f4u);
    guest_fn_ea270(0x0016040cu);
    guest_fn_9c3cf(0x0008b260u);
    return guest_fn_aaee8(argc, argv);
}
