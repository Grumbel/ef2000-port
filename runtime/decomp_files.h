/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#ifndef EF2000_DECOMP_FILES_H
#define EF2000_DECOMP_FILES_H

#include <stdint.h>

/* The GCC-only file adapters use a caller-chosen read-only data directory.
 * NULL disables file access, preserving the previous unbound probe boundary. */
int ef_decomp_files_begin(const char *directory);
int ef_decomp_files_enabled(void);
void ef_decomp_files_end(void);

/* Probe-only uncompressed read: 1 read, 0 absent, -1 compressed/unsupported. */
int ef_decomp_files_read_plain(uint32_t path, uint32_t dest, uint32_t length);

#endif
