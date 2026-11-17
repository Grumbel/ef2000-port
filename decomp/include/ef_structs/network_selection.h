/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */
#ifndef EF_STRUCTS_NETWORK_SELECTION_H
#define EF_STRUCTS_NETWORK_SELECTION_H
#include "ef_types.h"

/* Partial callback record: selection number is inline text at +0x3f. */
typedef struct EF_PACKED ef_network_selection {
    uint8_t unknown_00[0x3f];
    char number[1];
} ef_network_selection;
EF_STATIC_ASSERT(ef_network_selection_number_off, offsetof(ef_network_selection, number) == 0x3f);
#endif
