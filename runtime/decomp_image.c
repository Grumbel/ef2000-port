/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception */

#include "decomp_image.h"
#include "ef_guest_memory.h"

#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *guest_ram;

static uint16_t read_u16(const uint8_t *src)
{
    return (uint16_t)((uint16_t)src[0] | (uint16_t)src[1] << 8);
}

static uint32_t read_u32(const uint8_t *src)
{
    return (uint32_t)src[0] | (uint32_t)src[1] << 8 |
           (uint32_t)src[2] << 16 | (uint32_t)src[3] << 24;
}

int ef_decomp_decode_layout(const uint8_t *exe, size_t length,
                            ef_decomp_image_info *info)
{
    const uint32_t module = 0x30u;
    uint32_t source, stage2_size, data_offset, code_offset, payload_offset;
    uint32_t initialized_size, bss_end;
    if (!exe || !info || length < 0x60u || memcmp(exe, "MZ", 2) != 0 ||
        read_u16(exe + 8) != 3u || memcmp(exe + 0x28, "B23X", 4) != 0)
        return -1;
    source = read_u16(exe + module);
    stage2_size = read_u16(exe + module + 2);
    data_offset = (uint32_t)read_u16(exe + module + 0x12) * 16u;
    code_offset = (uint32_t)read_u16(exe + module + 8) * 16u;
    payload_offset = source + stage2_size;
    if (source <= module || payload_offset > length ||
        data_offset < 0x0cu || data_offset + 12u > stage2_size ||
        code_offset < data_offset || code_offset >= stage2_size ||
        (uint64_t)source + data_offset + 12u > length)
        return -1;
    initialized_size = read_u32(exe + source + data_offset + 4);
    bss_end = read_u32(exe + source + data_offset + 8);
    if (initialized_size != length - payload_offset ||
        initialized_size <= 0x86u || initialized_size > bss_end ||
        bss_end > EF_DECOMP_RAM_SIZE)
        return -1;
    info->payload_file_offset = payload_offset;
    info->initialized_size = initialized_size;
    info->bss_end = bss_end;
    return 0;
}

int ef_decomp_memory_load(const uint8_t *bytes, size_t length, uint32_t bss_end)
{
    uint8_t *replacement;
    if (!bytes || length == 0 || length > bss_end || bss_end > EF_DECOMP_RAM_SIZE)
        return -1;
    replacement = (uint8_t *)calloc(EF_DECOMP_RAM_SIZE, 1);
    if (!replacement)
        return -1;
    memcpy(replacement, bytes, length);
    free(guest_ram);
    guest_ram = replacement;
    return 0;
}

void ef_decomp_memory_release(void)
{
    free(guest_ram);
    guest_ram = NULL;
}

void *ef_decomp_memory_span(uint32_t address, size_t length)
{
    if (!guest_ram || address > EF_DECOMP_RAM_SIZE ||
        length > (size_t)EF_DECOMP_RAM_SIZE - address)
        return NULL;
    return guest_ram + address;
}

void *ef_guest_memory_ptr(ef_addr32 address)
{
    /* EF_U32's lvalue access needs four bytes. Reject the final few bytes
     * even for byte accesses; current game data resides far below this. */
    void *pointer = ef_decomp_memory_span(address, 8u);
    if (!pointer) {
        fprintf(stderr, "ef2000-decomp: invalid guest RAM offset 0x%08x\n", address);
        abort();
    }
    return pointer;
}

static void set_reason(char *reason, size_t capacity, const char *message)
{
    if (reason && capacity)
        snprintf(reason, capacity, "%s", message);
}

int ef_decomp_load_executable(const char *path, ef_decomp_image_info *info,
                              char *reason, size_t reason_capacity)
{
    static const char expected_digest[] =
        "f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd";
    FILE *input;
    long file_size;
    uint8_t *bytes;
    unsigned char digest[SHA256_DIGEST_LENGTH];
    char actual_digest[SHA256_DIGEST_LENGTH * 2 + 1];
    ef_decomp_image_info layout;
    int result = -1;
    unsigned int i;
    if (!path || !info) {
        set_reason(reason, reason_capacity, "missing image path or result");
        return -1;
    }
    input = fopen(path, "rb");
    if (!input) {
        set_reason(reason, reason_capacity, "cannot open original executable");
        return -1;
    }
    if (fseek(input, 0, SEEK_END) != 0 ||
        (file_size = ftell(input)) < 0 || file_size > 8 * 1024 * 1024 ||
        fseek(input, 0, SEEK_SET) != 0) {
        set_reason(reason, reason_capacity, "cannot determine executable size");
        fclose(input);
        return -1;
    }
    bytes = (uint8_t *)malloc((size_t)file_size);
    if (!bytes) {
        set_reason(reason, reason_capacity, "out of memory reading executable");
        fclose(input);
        return -1;
    }
    if (fread(bytes, 1, (size_t)file_size, input) != (size_t)file_size ||
        ferror(input)) {
        set_reason(reason, reason_capacity, "cannot read original executable");
        goto finish;
    }
    SHA256(bytes, (size_t)file_size, digest);
    for (i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        snprintf(actual_digest + i * 2, 3, "%02x", digest[i]);
    if (strcmp(actual_digest, expected_digest) != 0) {
        set_reason(reason, reason_capacity, "unsupported executable SHA-256");
        goto finish;
    }
    if (ef_decomp_decode_layout(bytes, (size_t)file_size, &layout) != 0 ||
        ef_decomp_memory_load(bytes + layout.payload_file_offset,
                              layout.initialized_size, layout.bss_end) != 0) {
        set_reason(reason, reason_capacity, "invalid X-32 layout or memory allocation");
        goto finish;
    }
    *info = layout;
    result = 0;
finish:
    free(bytes);
    fclose(input);
    return result;
}
