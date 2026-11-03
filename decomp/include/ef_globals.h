/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#ifndef EF_GLOBALS_H
#define EF_GLOBALS_H

#include "ef_types.h"

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
extern uint8_t g_hiscore_table[250]; /* 0x001970a4, 10 packed records */

#endif
