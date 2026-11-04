/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Status request selection and range-limited redraw. */

/* 0xa32e8, 98 bytes. Request the state matching the current limit, or a status string for a lookup. */
void request_status_for_a32e8(uint16_t key)
{
    if ((EF_S32(0x001357aeu) >> 16) > (int32_t)EF_U16(0x001357b4u)) {
        issue_state_request_a32c8(3);
        return;
    }
    if (EF_U8(0x00136dd0u) != 0) {
        issue_state_request_a32c8(2);
        return;
    }
    {
        int16_t r = (int16_t)guest_fn_a312c_w(key);

        if (r == -1) {
            issue_state_request_a32c8(1);
            return;
        }
        issue_state_request_a32c8(0);
        request_status_string_a3280((uint16_t)r);
    }
}

/* 0xd3c50, 84 bytes. Redraw when the tracked span is large enough and the two edge values differ by more than 0x800. */
void redraw_wide_span_d3c50(void)
{
    int32_t a;
    int32_t b;
    int32_t d;

    if ((int16_t)(EF_U16(0x00485fd2u) - EF_U16(0x00485feau)) < 0x10)
        return;
    a = EF_S32(0x00485ffcu) >> 8;
    b = EF_S32(0x00485fe4u) >> 8;
    EF_S32(0x0015b8f4u) = a;
    EF_S32(0x0015b8f8u) = b;
    if (a >= 0x20000 || b >= 0x20000)
        return;
    d = a - b;
    if (d < 0)
        d = -d;
    if (d > 0x800)
        guest_fn_d3900_w();
}
