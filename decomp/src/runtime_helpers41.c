/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* MIDI event dispatch. */

/* 0x1149e0, 178 bytes. Dispatch a MIDI event for a sound record (cdecl: record, port, status, data1, data2): with a non-zero port forward it to 0x1123a4; otherwise scale channel volume (controller 7) by the record's master volume at +0x1b0, offer it to the record's callback at +0x1a0 and, unless the callback consumes it, queue it in the event buffer; then flush. */
void dispatch_midi_event_1149e0(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec, uint32_t port, uint32_t status, uint32_t data1, int32_t data2)
{
    (void)u0; (void)u1; (void)u2; (void)u3;
    EF_U32(rec + 0x14u) += 1u;
    if (port == 0) {
        if ((status & 0xf0u) == 0xb0u && data1 == 7u) {
            data2 = (int32_t)EF_U32(rec + 0x1b0u) * data2 / 0x7f;
            if (data2 > 0x7f)
                data2 = 0x7f;
            if (data2 < 0)
                data2 = 0;
        }
        if (EF_U32(rec + 0x1a0u) == 0
            || guest_callptr5_x(EF_U32(rec + 0x1a0u), rec, 0, status, data1, (uint32_t)data2) == 0)
            append_midi_event_111e40(0, 0, 0, 0, rec, status, data1, (uint32_t)data2);
    } else {
        guest_fn_1123a4_x(port, status, data1, (uint32_t)data2, 0);
    }
    send_pending_count_message_111df0(0, 0, 0, 0, rec);
    EF_U32(rec + 0x14u) -= 1u;
}
