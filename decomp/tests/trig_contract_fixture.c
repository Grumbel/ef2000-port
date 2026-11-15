/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "ef_abi.h"
#include "ef_guest_memory.h"

/* Keep the input opaque to the compiler and use the public declarations. */
uint32_t trig_contract_fixture(ef_addr32 values)
{
    double angle = *(const double *)ef_guest_memory_ptr(values);
    double cosine = cos_b45a2(angle);
    double sine = sin_b45ac(angle);
    *(double *)ef_guest_memory_ptr(values + 8u) = cosine;
    *(double *)ef_guest_memory_ptr(values + 16u) = sine;
    return 17;
}
