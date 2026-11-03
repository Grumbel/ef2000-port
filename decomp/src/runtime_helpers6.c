/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Small library accessors and wrappers, 0x9ddfc..0xc382c. */

/* 0x9ddfc, 10 bytes. Call 0x9de08 with a zero second argument. */
uint32_t call_9de08_9ddfc(ef_addr32 text)
{
    return guest_fn_9de08(text, 0);
}

/* 0xa28f2, 7 bytes. Absolute value. */
uint32_t abs_a28f2(int32_t value)
{
    if (value < 0)
        return 0u - (uint32_t)value;
    return (uint32_t)value;
}

/* 0xab838, 6 bytes. */
void set_global_ab838(uint32_t value)
{
    EF_U32(0x0041ed80u) = value;
}

/* 0xb3d60, 6 bytes. */
uint32_t get_global_b3d60(void)
{
    return EF_U32(0x00146e20u);
}

/* 0xb3f48, 6 bytes. */
uint32_t get_global_b3f48(void)
{
    return EF_U32(0x00146e18u);
}

/* 0xb3f50, 6 bytes. */
uint32_t get_global_b3f50(void)
{
    return EF_U32(0x00146e1cu);
}

/* 0xc3824, 6 bytes. */
uint32_t get_global_c3824(void)
{
    return EF_U32(0x00146e40u);
}

/* 0xc382c, 6 bytes. */
uint32_t get_global_c382c(void)
{
    return EF_U32(0x00146e44u);
}

/* 0xb55a4, 34 bytes. Random number modulo the signed high word at +0x2 of the record. */
int32_t random_modulo_b55a4(ef_addr32 record)
{
    int32_t divisor = (int32_t)EF_U32(record + 2u) >> 16;
    uint32_t value = guest_fn_a45b3() & 0xffffu;

    /* A zero divisor faults in the original (divide error). */
    return (int32_t)value % divisor;
}
