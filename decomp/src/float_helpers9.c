/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#include <math.h>

/* Ninth x87 batch: scaled trig wrappers. */

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

/* 0xf8278, 29 bytes. sin of the argument scaled by the two constants used in 0xf8250 (0x1932a4 and 0x1932ac). */
double sin_scaled_f8278(double angle)
{
    return sin(angle * EF_F64(0x001932a4u) * EF_F64(0x001932acu));
}

/* 0xf8298, 29 bytes. cos of the same scaled argument. */
double cos_scaled_f8298(double angle)
{
    return cos(angle * EF_F64(0x001932a4u) * EF_F64(0x001932acu));
}
