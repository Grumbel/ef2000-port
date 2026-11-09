/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0x30214..0x3024e: Initialize the 40 generic sequence slots in Views.c.
 * The original copies exactly five bytes (including the name terminator)
 * from the default sequence name, then writes the default index word. */
void init_gen_sequence_slots_30214(void)
{
    uint16_t index = EF_U16(0x00182710u);
    uint32_t i;

    for (i = 0; i < 40u; ++i) {
        ef_addr32 offset = i * 0xa1u;
        EF_U32(0x0019cbf0u + offset) = EF_U32(0x00182708u);
        EF_U8(0x0019cbf4u + offset) = EF_U8(0x0018270cu);
        EF_U16(0x0019cbb0u + offset) = index;
    }
}

/* 0x30214..0x30272: Bounded ReadGenSeqs() prefix. The original then opens
 * the constructed BG\\views\\views2.dat path at 0x9ccb8 and parses a
 * sequence file into its per-slot records. No such parser is recovered here. */
void read_gen_sequences_prefix_30214(ef_addr32 frame)
{
    init_gen_sequence_slots_30214();
    build_default_path_9c980(frame);
    EF_U16(frame + 0x90u) = 0u;
    strcat_9ca57(frame, 0x00182794u);
    (void)guest_fn_9ccb8(frame);
}
