/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_TYPES_H
#define EF_TYPES_H

#include <stdint.h>
#include <stddef.h>

/* Guest memory is 32-bit, even when host-side tools are 64-bit. */
typedef uint32_t ef_addr32;

/* OpenWatcom uses -zp1 (or explicit #pragma pack(1)) for proven packed
 * structures. Other compilers can annotate those declarations directly. */
#if defined(__WATCOMC__)
#define EF_PACKED
#elif defined(__GNUC__) || defined(__clang__)
#define EF_PACKED __attribute__((packed))
#else
#define EF_PACKED
#endif

/* C89-compatible compile-time assertion, with an explicit unique name. */
#define EF_STATIC_ASSERT(name, condition) typedef char name[(condition) ? 1 : -1]

#endif
