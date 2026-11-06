/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Small runtime wrappers. */

/* 0x106e2c, 19 bytes. Run 106d00 on the object, then 106dc4 on it with the second argument. */
void run_pair_106e2c(uint32_t object, uint32_t u1, uint32_t second)
{
    (void)u1;
    guest_fn_106d00_x(object);
    guest_fn_106dc4_x(object, second);
}

/* 0x112bbc, 25 bytes. Reset a table record and point its field 0x14 8 bytes past field 0x10. */
void reset_table_record_and_link_112bbc(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec)
{
    (void)u0; (void)u1; (void)u2; (void)u3;
    reset_table_record_112b1c(0, 0, 0, 0, rec);
    EF_U32(rec + 0x14u) = EF_U32(rec + 0x10u) + 8u;
}

/* 0x113cf4, 27 bytes. Trace-call a non-null record and mark its field 4 as 1. */
void trace_and_mark_113cf4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3, ef_addr32 rec)
{
    (void)u0; (void)u1; (void)u2; (void)u3;
    if (rec != 0) {
        trace_call_103148(rec);
        EF_U32(rec + 4u) = 1;
    }
}

/* 0x1113d4, 39 bytes. Between the 101e70/101e78 bracket clear one table dword (index from the stack) unless it is -1. */
void EF_CDECL clear_table_entry_1113d4(uint32_t index)
{
    guest_fn_101e70();
    if (index != 0xffffffffu)
        EF_U32(0x0017d664u + index) = 0;
    guest_fn_101e78();
}
