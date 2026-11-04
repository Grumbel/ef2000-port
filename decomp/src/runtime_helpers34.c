/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Record dumps, file header sizing, stack pool and fopen mode parsing, 0xb2d8c..0xbd5f8. */

/* 0xb2d8c, 155 bytes. Print one shape point record. */
void print_point_b2d8c(ef_addr32 rec, uint32_t out)
{
    if (EF_U8(rec + 3u) == 0) {
        guest_fn_b1bb0(out, 0x0018e3e8u, (uint32_t)EF_U8(rec), (uint32_t)EF_U8(rec + 1u), (uint32_t)EF_U8(rec + 2u),
                       EF_U32(0x00132c2cu + (uint32_t)EF_U8(rec + 3u) * 4u), (uint32_t)(EF_S32(rec + 3u) >> 16),
                       (uint32_t)(EF_S32(rec + 5u) >> 16));
    } else {
        guest_fn_b1bb0(out, 0x0018e418u, (uint32_t)EF_U8(rec), (uint32_t)EF_U8(rec + 1u), (uint32_t)EF_U8(rec + 2u),
                       EF_U32(0x00132c2cu + (uint32_t)EF_U8(rec + 3u) * 4u),
                       EF_U32(0x00132e6cu + (uint32_t)EF_U8(rec + 4u) * 4u), (uint32_t)(EF_S32(rec + 3u) >> 16),
                       (uint32_t)(EF_S32(rec + 5u) >> 16));
    }
}

/* 0xb2e98, 176 bytes. Print an object record and its point list. */
void print_object_b2e98(ef_addr32 rec, uint32_t out)
{
    int16_t i;

    guest_fn_b1bb0(out, 0x0018e454u, EF_U32(0x00132f5cu + (uint32_t)EF_U8(rec + 0xbu) * 4u),
                   EF_U32(0x00132fb4u + (uint32_t)EF_U8(rec + 0xcu) * 4u), EF_U32(rec + 0xdu),
                   (uint32_t)(EF_S32(rec + 7u) >> 16), (uint32_t)(EF_S32(rec + 0x21u) >> 16), EF_U32(rec + 0x27u),
                   EF_U32(rec + 0x2bu), (uint32_t)EF_U8(rec + 0x2fu));
    guest_fn_b2a2c(rec + 0x11u, out);
    guest_fn_b1bb0(out, 0x0018e478u, (uint32_t)(int32_t)(int16_t)guest_fn_b2e64(rec));
    if (EF_S16(rec + 0x48u) > 0) {
        i = 0;
        do {
            ef_addr32 ent = EF_U32(rec + 0x4au) + (uint32_t)(int32_t)i * 9u;
            if (EF_U8(ent + 3u) != 0)
                print_point_b2d8c(ent, out);
            i++;
        } while (i < EF_S16(rec + 0x48u));
    }
    guest_fn_b1bb0(out, 0x0018e488u);
}

/* 0xb3b90, 114 bytes. Return the payload size of a file from its header. */
uint32_t read_file_payload_size_b3b90(ef_addr32 name)
{
    uint32_t handle = guest_fn_a7900(name);
    uint32_t result = 0;

    if (handle != 0xffffffffu) {
        EF_U16(0x0041f360u) = 0;
        guest_fn_a79d4(handle, 8, 0x0041f360u);
        {
            int32_t magic = EF_S32(0x0041f35eu) >> 16;
            if (magic == 0x524a)
                result = EF_U32(0x0041f364u) + 0x10u;
            else if (magic == 0x4152)
                result = EF_U32(0x0041f364u);
            else
                result = guest_fn_a7ae0(handle, 0);
        }
        guest_fn_a7990(handle);
    }
    return result;
}

/* 0xb3db0, 150 bytes. Take a block from the top of the stack pool and clear it. */
uint32_t take_pool_block_b3db0(uint32_t size)
{
    uint32_t block;

    EF_U32(0x00146e20u) -= size;
    if (EF_S32(0x00146e20u) < 0x20)
        guest_report_parser_enum_error(0x0018e760u, size, 0);
    block = EF_U32(0x00146e28u) + EF_U32(0x00146e20u);
    EF_U32(EF_U32(0x0041f370u) + (uint32_t)EF_U16(0x00146e24u) * 4u) = block;
    EF_U16(0x00146e24u) = (uint16_t)(EF_U16(0x00146e24u) + 1);
    if ((int32_t)EF_U16(0x00146e24u) >= EF_S32(0x00146e10u))
        ui_report_missing_record(0x0018e790u);
    guest_fn_b3fb0(block, 0, size);
    return (uint16_t)(EF_U16(0x00146e24u) + EF_U16(0x00146e2cu));
}

/* 0xbc1d3, 180 bytes. Convert an fopen mode string to open flags. */
uint32_t parse_open_mode_bc1d3(ef_addr32 mode)
{
    uint32_t m;
    uint8_t c = (uint8_t)guest_fn_e7835(EF_U8(mode));
    uint8_t c1 = EF_U8(mode + 1u);
    uint8_t c2 = EF_U8(mode + 2u);

    if (c != 'r' && c != 'w' && c != 'a') {
        guest_fn_ebe6d(9);
        return 0;
    }
    if (c1 == '+') {
        m = 3;
        if (c2 == 'b' || (c2 != 't' && EF_U32(0x0016062du) == 0x200u))
            m |= 0x40;
    } else if (c1 == 'b') {
        m = 0x40;
        if (c2 == '+')
            m |= 3;
    } else if (c1 == 't') {
        m = (c2 == '+') ? 3u : 0u;
    } else {
        m = (EF_U32(0x0016062du) == 0x200u) ? 0x40u : 0u;
    }
    if (c == 'w')
        m |= 2;
    else if (c == 'a')
        m |= 0x82;
    else
        m |= 1;
    return m;
}

/* 0xbd5f8, 144 bytes. Clear the unusable entries of a record's slot table. */
uint8_t clear_unusable_slots_bd5f8(ef_addr32 rec)
{
    int16_t ok = 1;
    int16_t i;

    if (EF_U8(rec + 1u) == 2)
        return 0;
    if (EF_U8(rec + 1u) == 0x19 && (int16_t)guest_fn_bdbc8(rec) < 2)
        return 0;
    for (i = (int16_t)(EF_U8(rec + 5u) - 1); i >= 0 && ok != 0; i--) {
        ef_addr32 ent = EF_U32(rec + 6u) + (uint32_t)(int32_t)i * 10u;
        if (EF_U16(ent) != 0 && (uint16_t)guest_fn_bd5a8(rec) == 0) {
            ok = 0;
            EF_U16(EF_U32(rec + 6u) + (uint32_t)(int32_t)i * 10u) = 0;
        }
    }
    return ok == 0;
}

/* 0xbe320, 210 bytes. Link matching shape entries of two lists to their lookup value. */
void link_matching_entries_be320(ef_addr32 listp, ef_addr32 otherp)
{
    ef_addr32 a;
    ef_addr32 b;

    for (a = EF_U32(listp); a != 0; a = EF_U32(a + 4u)) {
        if (EF_U8(a + 9u) != 0 || EF_U8(a + 0xau) == 0xf || EF_U8(a + 0xau) == 0x11 || EF_U8(a + 0xau) == 0x10)
            continue;
        for (b = EF_U32(otherp); b != 0; b = EF_U32(b + 4u)) {
            int16_t i;
            if (EF_U8(b + 0xbu) != 0 || EF_U16(b + 9u) == 0)
                continue;
            for (i = 0; i < (int16_t)EF_U8(a + 0x12u); i++) {
                ef_addr32 c = EF_U32(a + 0x13u) + (uint32_t)(int32_t)i * 0x7cu;
                uint8_t t = EF_U8(c + 2u);
                if (t != 6 && t != 8 && t != 7 && t != 9 && EF_U16(b + 0x19u) == EF_U16(a + 0x10u)
                    && (uint16_t)i == EF_U16(b + 0x21u)) {
                    uint16_t v = (uint16_t)guest_fn_bb7c0(c);
                    EF_U16(b + 0x53u) = 0;
                    EF_U16(b + 0x51u) = v;
                }
            }
        }
    }
}
