/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
 */

#ifndef EF2000_X32_H
#define EF2000_X32_H
typedef unsigned int u32;
struct x32_allocation { u32 heap_end, stack_end, free_bytes, minimum_stack, heap_ceiling; };
struct x32_stack_result { u32 low_limit, flags; };
int x32_stack_reserve(struct x32_allocation *, u32 sp, u32 size, u32 flags,
                      struct x32_stack_result *);
struct x32_heap_result { u32 heap_end, stack_low, flags; };
int x32_heap_grow(struct x32_allocation *, u32 requested, u32 flags,
                  struct x32_heap_result *);
#endif
