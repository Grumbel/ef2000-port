/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_guest_memory.h"

static uint8_t read8(ef_addr32 address)
{
    return *(const uint8_t *)ef_guest_memory_ptr(address);
}

static uint16_t read16(ef_addr32 address)
{
    return (uint16_t)(read8(address) | (uint16_t)(read8(address + 1u) << 8));
}

static uint32_t read32(ef_addr32 address)
{
    return (uint32_t)read16(address) | ((uint32_t)read16(address + 2u) << 16);
}

/* Keep the single original x86 dword load without misaligned C pointer
 * access in host builds. GCC folds builtin memcpy to MOV; other host
 * compilers use the equivalent little-endian byte reader above. */
#if !defined(__WATCOMC__)
static uint32_t read32_unaligned(ef_addr32 address)
{
#if defined(__GNUC__) && defined(__BYTE_ORDER__) && \
    (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    uint32_t value;
    __builtin_memcpy(&value, ef_guest_memory_ptr(address), sizeof(value));
    return value;
#else
    return read32(address);
#endif
}
#endif

static void write8(ef_addr32 address, uint8_t value)
{
    *(uint8_t *)ef_guest_memory_ptr(address) = value;
}

static void write16(ef_addr32 address, uint16_t value)
{
    write8(address, (uint8_t)value);
    write8(address + 1u, (uint8_t)(value >> 8));
}

#if defined(__WATCOMC__)
static void encoder_timing_nop(void);
#pragma aux encoder_timing_nop = 0x90 __modify []
#endif

/* 0x604, 36 bytes. Encode a byte pointer relative to the 0x120cc6
 * X-32 base as a segment:offset pair with a 16-bit segment and 4-bit offset. */
uint32_t encode_runtime_segment_offset(ef_addr32 pointer)
{
#if defined(__WATCOMC__)
#pragma pack(push, 1)
    struct unaligned_u32 { uint32_t value; };
#pragma pack(pop)
    uint32_t base = ((const struct unaligned_u32 *)(uintptr_t)0x00120cc6u)->value;
#else
    uint32_t base = read32_unaligned(0x00120cc6);
#endif
    uint32_t delta = pointer - base;
    uint32_t encoded = delta & 0x0fu;
    encoded += ((delta >> 4) & 0xffffu) << 16;
#if defined(__WATCOMC__)
    /* The original EBX save/restore occupies two instruction slots. */
    encoder_timing_nop();
    encoder_timing_nop();
#endif
    return encoded;
}

/* 0x12160, 84 bytes. Visit all 54 callback records (stride 29),
 * clear inactive flags, and re-evaluate each slot through 0x11e7c. */
void reinitialize_runtime_slots(void)
{
    write8(0x00122882, 1);
    write16(0x0012288a, 0);
    for (int16_t i = 0; i < 54; ++i) {
        ef_addr32 record = 0x00121fb4u + (uint32_t)i * 29u;
        ef_addr32 callback = read32(record + 25u);
        if (callback != 0) {
            write16(0x00122880, (uint16_t)i);
            guest_invoke_runtime_slot(callback);
        } else {
            write8(record, 0);
        }
        guest_refresh_runtime_slot(i);
    }
    write8(0x00122882, 0);
}

/* 0x121cc, 33 bytes. A signed AX subindex and the signed high word
 * of an overlapping dword at 0x121ec6 + 2*DX select a 29-byte slot. */
uint8_t read_runtime_slot_flag(int16_t subindex, int16_t group)
{
    uint32_t packed = read32(0x00121ec6u + (uint32_t)(int32_t)group * 2u);
    int32_t base = (int32_t)packed >> 16;
    int32_t index = base + subindex;
    return read8(0x00121fb4u + (uint32_t)index * 29u);
}

/* 0x121f0, 29 bytes. Guest dependency writes two scratch dwords;
 * the original discards them, retaining only the global latch write. */
void initialize_runtime_probe_latch(void)
{
    uint32_t output_at_esp_plus_4;
    uint32_t output_at_esp;
    guest_query_runtime_pair(&output_at_esp_plus_4, &output_at_esp);
    write16(0x00121ed0, 0x96);
}
