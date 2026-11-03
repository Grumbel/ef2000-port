/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_GLOBALS_H
#define EF_GLOBALS_H

#include "ef_types.h"
#include "ef_structs/hiscore.h"
#include "ef_structs/spatial.h"
#include "ef_structs/world_grid.h"

/* These describe guest addresses, not linked host storage. The patch linker
 * must resolve each symbol to its original address (symbols/globals.tsv).
 * An unresolved struct layout is deliberately represented as raw bytes. */
extern uint8_t g_difficulty;          /* 0x00121e44 */
extern uint32_t __x32_stack_size;     /* 0x00132a28 */
extern uint32_t _dynend;              /* 0x00132a2c */
extern uint32_t _curbrk;              /* 0x00132a30 */
extern ef_addr32 _LpCmdLine;          /* 0x00132a34 */
extern ef_addr32 _LpPgmName;          /* 0x00132a38 */
extern uint16_t _psp;                 /* 0x00132a3c */
extern uint8_t _Envptr[6];           /* 0x00132a61, overlaps _Envseg */
extern uint16_t _Envseg;              /* 0x00132a65 */
extern int32_t g_hiscore_current;     /* 0x001970a0 */
extern ef_hiscore_entry g_hiscore_table[10]; /* 0x001970a4, 10 packed records */
/* Provisional names; original DS locations established from code reads/writes. */
extern ef_spatial_slot g_spatial_slots[29]; /* 0x0019b2e0 */
extern uint8_t g_spatial_slot_tags[29]; /* 0x0019b358..0x0019b374 */
extern int16_t g_spatial_slot_count; /* 0x0019b376, high word of 0x0019b374 */
extern ef_addr32 g_indexed_object_table[]; /* 0x0020f550, 4-byte guest pointers */
extern int16_t g_indexed_object_count; /* 0x002119f4, signed 16-bit bound */
extern uint32_t g_filtered_object_lookup_calls; /* 0x001a3580 */
extern int16_t g_excluded_object_index; /* 0x001f71d0 */
extern uint8_t g_object_type_flags[]; /* 0x00128636, 0xe5-byte stride */
extern ef_addr32 g_world_grid_base; /* 0x0019bb58, pointer to 0xca-byte records */
extern ef_addr32 g_current_object; /* 0x001f71bc, pointer to active object */
extern uint8_t g_world_grid_mode; /* 0x00127da9, gates alternate grid lookup */
extern uint8_t g_world_grid_marker; /* 0x001a666b, compares neighboring rows */
extern uint8_t g_external_grid_count; /* 0x0012583a, number of external rows */
extern int16_t g_active_grid_index; /* 0x0019bb5e, current selected grid row */
extern ef_addr32 g_external_grid_base; /* 0x001a14f8, pointer to 0x45-byte nodes */
extern double g_external_grid_scale; /* 0x00182512, integer-to-world conversion */
extern ef_grid_type_record g_grid_type_records[]; /* 0x001285d5, stride 0xe5 */
extern double g_grid_limit_scale; /* 0x00182605, type-adjusted limit factor */

#endif
