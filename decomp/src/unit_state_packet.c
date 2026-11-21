/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

#define UNIT_TABLE          0x0019bb58u /* pointer to 0xca-byte unit records */
#define UNIT_RECORD_SIZE    0xcau
#define LOCAL_OWNER         0x001a666bu /* byte: owner id of this machine */
#define WORLD_SCALE         0x00185bc4u /* double: position/velocity scale */
#define HEADING_SCALE_A     0x00185bccu /* double */
#define HEADING_SCALE_B     0x00185bd4u /* double */
#define THROTTLE_BIAS       0x00185bdcu /* double */
#define THROTTLE_SCALE_A    0x00185be4u /* double */
#define THROTTLE_SCALE_B    0x00185becu /* double */

#define UNIT_STATE_PACKET   0x14u
#define UNIT_STATE_SIZE     0x39u

static void put_word(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static void put_dword(uint8_t *bytes, uint32_t value)
{
    put_word(bytes, (uint16_t)value);
    put_word(bytes + 2, (uint16_t)(value >> 16));
}

static int32_t scaled(double value, double scale)
{
    return (int32_t)truncate_9d72c(value * scale);
}

/* 0x54c10, 426 bytes. Broadcast the full state of unit INDEX as a 0x39-byte
 * packet 0x14, but only for units with no owner (0xff) or owned by this
 * machine. Positions (+0x61/+0x69/+0x71) and velocities (+0x27/+0x2f/+0x37)
 * go out as truncated fixed-point dwords; +0xc1 and +0x81 are rescaled into
 * a word and a byte. Bytes 0x1c..0x1d are never written by the original
 * (stack garbage); they are zero here. */
void send_unit_state_packet_54c10(uint32_t index)
{
    uint8_t bytes[UNIT_STATE_SIZE] = { 0 };
    ef_addr32 unit = EF_U32(UNIT_TABLE) + (int32_t)(int16_t)index * UNIT_RECORD_SIZE;
    double k;
    uint32_t i;
    uint8_t owner = EF_U8(unit);

    if (owner != 0xffu && owner != EF_U8(LOCAL_OWNER))
        return;

    k = EF_F64(WORLD_SCALE);
    bytes[0x00] = UNIT_STATE_PACKET;
    put_word(bytes + 0x01, (uint16_t)index);
    put_dword(bytes + 0x03, (uint32_t)scaled(EF_F64(unit + 0x69u), k));
    put_dword(bytes + 0x07, (uint32_t)scaled(EF_F64(unit + 0x71u), k));
    put_dword(bytes + 0x0b, (uint32_t)scaled(EF_F64(unit + 0x61u), k));
    bytes[0x0f] = EF_U8(unit + 0x1au);
    for (i = 0; i < 6u; i++)
        put_word(bytes + 0x10 + 2u * i, EF_U16(unit + 0x50u + 2u * i));
    bytes[0x1e] = EF_U8(unit + 0x5fu);
    bytes[0x1f] = EF_U8(unit + 0x60u);
    put_word(bytes + 0x20, EF_U16(unit + 0x24u));
    bytes[0x22] = EF_U8(unit + 0x26u);
    put_word(bytes + 0x23, EF_U16(unit + 0xb9u));
    bytes[0x25] = EF_U8(unit + 0xbbu);
    bytes[0x26] = EF_U8(unit + 0x20u);
    bytes[0x27] = EF_U8(unit + 0x8eu);
    bytes[0x28] = EF_U8(unit + 0x5eu);
    put_dword(bytes + 0x29, (uint32_t)scaled(EF_F64(unit + 0x27u), k));
    put_dword(bytes + 0x2d, (uint32_t)scaled(EF_F64(unit + 0x2fu), k));
    put_dword(bytes + 0x31, (uint32_t)scaled(EF_F64(unit + 0x37u), k));
    put_word(bytes + 0x35, (uint16_t)(int32_t)truncate_9d72c(
        EF_F64(unit + 0xc1u) * EF_F64(HEADING_SCALE_A) * EF_F64(HEADING_SCALE_B)));
    bytes[0x37] = EF_U8(unit + 0xbcu);
    bytes[0x38] = (uint8_t)(int32_t)truncate_9d72c(
        (EF_F64(unit + 0x81u) + EF_F64(THROTTLE_BIAS))
        * EF_F64(THROTTLE_SCALE_A) * EF_F64(THROTTLE_SCALE_B));
    emit_command_packet(bytes, UNIT_STATE_SIZE);
}
