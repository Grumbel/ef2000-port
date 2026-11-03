/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_ABI_H
#define EF_ABI_H

#include "ef_types.h"

/* The game generally uses Watcom register calling, but individual signatures
 * must be confirmed from callers before adding #pragma aux definitions here.
 * See symbols/signatures.tsv and docs/decompilation-plan.md §3.5. */

/* 0x000432c: no arguments, only AL observed as output (two callers). */
uint8_t hiscore_qualifies(void);

/* 0x000263e0: no arguments or register return; preserves all GPRs. */
void reset_spatial_slot_count(void);

/* 0x000263ec: inputs AX/DX/BL; no observed register return. */
void append_spatial_slot(int16_t x, int16_t y, uint8_t tag);

/* 0x0006a1b4: input AX (signed), output full EAX guest address. */
ef_addr32 lookup_indexed_object(int16_t index);

/* 0x0007489c: AX signed index, EAX pointer/zero; increments a diagnostic
 * counter on every call, including rejected indices. */
ef_addr32 lookup_filtered_indexed_object(int16_t index);

/* Original filtered-lookup wrappers, signed AX input; no return consumed. */
void set_indexed_object_activation(int16_t index);    /* 0x0006a354 */
void clear_indexed_object_activation(int16_t index);  /* 0x0006a378 */
void set_indexed_object_state(int16_t index); /* 0x0006a394 */
void clear_indexed_object_state(int16_t index); /* 0x0006a3d0 */

/* World grid / indexed-object state: drafted only, not guest verified. */
uint8_t grid_slot_matches_object(int16_t index); /* 0x0002b884: AL result */
void set_grid_slot_event(int16_t cell, uint8_t value); /* 0x0002b910: AX,DL */
void enable_grid_slot_state(int16_t cell); /* 0x0002b95c: AX */
void enable_grid_slot_activation(int16_t cell); /* 0x0002ba14: AX */
void set_indexed_object_mode(int16_t index, uint8_t mode); /* 0x0002541c: AX,DL */

/* These are original guest dependencies. Calls are typed from the specific
 * original call sites, not independent recovered implementations. */
uint8_t check_current_object_relation(void); /* 0x00023800, AL */
void dispatch_indexed_object_event(int16_t index, int16_t event,
                                    int32_t aux_b, int32_t aux_c,
                                    int32_t stack_arg); /* 0x00046168, ret 4 */
void process_object_state_29(ef_addr32 object); /* 0x0002a168 */
void process_object_state_30(ef_addr32 object); /* 0x0002a1c4 */
void sync_current_object_slot(void); /* 0x000238a0 */

#endif
