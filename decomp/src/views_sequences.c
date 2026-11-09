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

/* 0x30214..0x3029c: Bounded ReadGenSeqs() prefix. The original opens the
 * constructed BG\\views\\views2.dat path at 0x9ccb8, reports a missing file
 * through the two-argument fatal callback at 0x132a74, or passes a successful
 * resource handle to the recovered 0x9d114 stream constructor. The original
 * next enters its sequence-line parser at 0x9d1e8, still unrecovered. */
void read_gen_sequences_prefix_30214(ef_addr32 frame)
{
    uint32_t handle;
    ef_addr32 stream;

    init_gen_sequence_slots_30214();
    build_default_path_9c980(frame);
    EF_U16(frame + 0x90u) = 0u;
    strcat_9ca57(frame, 0x00182794u);
    handle = guest_fn_9ccb8(frame);
    EF_U32(frame + 0x84u) = handle;
    if (handle == UINT32_MAX)
        guest_callptr2_x(EF_U32(0x00132a74u), 0x001827a8u, frame);
    else {
        stream = new_stream_9d114(handle);
        EF_U8(stream) = 0x63u;
        guest_fn_9d1e8(stream);
    }
}
