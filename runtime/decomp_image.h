/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#ifndef EF2000_DECOMP_IMAGE_H
#define EF2000_DECOMP_IMAGE_H

#include <stddef.h>
#include <stdint.h>

#define EF_DECOMP_RAM_SIZE (64u * 1024u * 1024u)

typedef struct ef_decomp_image_info {
    uint32_t payload_file_offset;
    uint32_t initialized_size;
    uint32_t bss_end;
} ef_decomp_image_info;

/* Structural parser for this version's FlashTek X-32 envelope, useful for
 * synthetic tests. Production loading separately requires the pinned hash. */
int ef_decomp_decode_layout(const uint8_t *exe, size_t length,
                            ef_decomp_image_info *info);

/* Install initialized guest bytes at offset zero and zero all remaining RAM.
 * The real original image is supplied by the user, never embedded in code. */
int ef_decomp_memory_load(const uint8_t *bytes, size_t length, uint32_t bss_end);
void ef_decomp_memory_release(void);

/* SHA-pinned, read-only loader. Returns zero on success and sets a reason on
 * failure; does not execute original machine code. */
int ef_decomp_load_executable(const char *path, ef_decomp_image_info *info,
                              char *reason, size_t reason_capacity);

#endif
