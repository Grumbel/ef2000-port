/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Engine setup and message helpers, 0xc4105..0xde4b0. */

/* 0xc4105, 98 bytes. */
void reset_view_c4105(void)
{
    EF_U32(0x004209c0u) = 0;
    EF_U32(0x004209c4u) = 0;
    EF_U32(0x004209c8u) = 0;
    EF_U16(0x00146e50u) = 0;
    EF_U16(0x00146e52u) = 0;
    EF_U8(0x000c41c4u) = 0x1b;
    if (EF_U8(0x00137a00u) != 0) {
        EF_U16(0x0015af80u) = EF_U16(0x0015af84u);
        EF_U16(0x0015af82u) = EF_U16(0x0015af86u);
    }
    EF_U16(0x0015af84u) = EF_U16(0x0015af80u);
    EF_U16(0x0015af86u) = EF_U16(0x0015af82u);
}

/* 0xca17c, 74 bytes. */
void redraw_ca17c(void)
{
    EF_U8(0x0013725cu) = 1;
    guest_fn_dc2c0();
    if (EF_U8(0x0015b5fau) != 0)
        guest_fn_d65e8();
    else
        guest_fn_d663c(0x004195d0u);
    guest_fn_a6038();
    EF_U8(0x0013725du) = 0;
    EF_U8(0x0013725cu) = 0;
    guest_fn_abde4();
    guest_fn_c9fc4();
}

/* 0xca7e0, 88 bytes. */
void rebuild_tables_ca7e0(void)
{
    uint32_t block;
    guest_fn_f8f8();
    guest_fn_cb9f4(0x00137960u, 0x00137964u, 0x00137958u, 0x00137980u, EF_U32(0x001379b4u), EF_U32(0x001379c0u));
    guest_fn_ca874(0x00137970u, 0x00137960u);
    block = EF_U32(0x0041f324u);
    if (block != 0)
        guest_fn_cc768(block);
    perturb_orientation_b0448();
}

/* 0xcc584, 119 bytes. */
uint32_t code_in_set_cc584(uint8_t code)
{
    return code <= 0x0f || code == 0x16 || code == 0x17;
}

/* 0xcc76d, 87 bytes. Same as 0x9d4cd. */
int32_t parse_int_cc76d(ef_addr32 text)
{
    return parse_int(text);
}

/* 0xcc894, 74 bytes. */
uint32_t pick_variant_cc894(uint8_t code)
{
    if (code == 0x31 || code == 0x49)
        return 3;
    if (code == 0x0b || code == 0x56)
        return 2;
    return ((guest_fn_a45b3() & 0xffffu) % 2u) + 1u;
}

/* 0xcd150, 113 bytes. */
uint8_t open_entry_cd150(int16_t number)
{
    uint32_t table;
    uint32_t entry;
    EF_U32(0x0015b628u) = 0x0015b624u;
    table = guest_fn_a1a40();
    if (table == 0)
        return 0;
    entry = guest_fn_a1a90((int32_t)number, table);
    if (entry == 0)
        return 0;
    EF_U32(0x0015b628u) = entry;
    EF_U32(0x0041f700u) = guest_fn_a0de0((int32_t)number, 0);
    EF_U32(0x0041f704u) = guest_fn_a1a10((int32_t)number);
    EF_U16(0x0041f708u) = (uint16_t)number;
    return EF_U16(EF_U32(0x0015b628u)) != 0;
}

/* 0xcd430, 161 bytes. Clear the key state tables and set the default scan codes. */
void init_keys_cd430(void)
{
    uint32_t i = 0;
    do {
        ++i;
        EF_U8(0x0041f78fu + i) = 0;
        EF_U8(0x0041f70fu + i) = 0;
    } while ((int32_t)i < 0x80);
    EF_U8(0x0041f722u) = 0;
    EF_U8(0x0041f72cu) = 0x1c;
    EF_U8(0x0041f72du) = 0x1d;
    EF_U8(0x0041f748u) = 0x38;
    EF_U8(0x0041f758u) = 0x79;
    EF_U8(0x0041f745u) = 0x35;
    EF_U8(0x0041f757u) = 0x73;
    EF_U8(0x0041f75bu) = 0x75;
    EF_U8(0x0041f759u) = 0x77;
    EF_U8(0x0041f75du) = 0x78;
    EF_U8(0x0041f75fu) = 0x72;
    EF_U8(0x0041f761u) = 0x76;
    EF_U8(0x0041f763u) = 0x70;
    EF_U8(0x0041f760u) = 0x71;
    EF_U8(0x0041f762u) = 0x74;
    EF_U8(0x0041f73au) = 0;
    EF_U8(0x0041f746u) = 0;
}

/* 0xd1f10, 73 bytes. Install the four handler pairs. */
void install_handlers_d1f10(void)
{
    EF_U32(0x0015b88cu) = 0x000d3c50u;
    EF_U32(0x0015b87cu) = 0x000d3c50u;
    EF_U32(0x0015b890u) = 0x000ff498u;
    EF_U32(0x0015b880u) = 0x000ff498u;
    EF_U32(0x0015b894u) = 0x000d4250u;
    EF_U32(0x0015b884u) = 0x000d4250u;
    EF_U32(0x0015b898u) = 0x000ff81cu;
    EF_U32(0x0015b888u) = 0x000ff81cu;
}

/* 0xd37f4, 80 bytes. Count a hit when the cell under the object equals dword 0x159144 + 0xe0. */
void count_hit_d37f4(ef_addr32 object)
{
    ef_addr32 cell = EF_U32(EF_U32(object + 0x4cu) + 4u);
    uint32_t hit = (uint8_t)guest_fn_c3f50(EF_S32(cell + 0x18u) >> 16, EF_S32(cell + 0x1cu) >> 16) ==
                   EF_U32(0x00159144u) + 0xe0u;
    uint32_t total = EF_U32(0x00420a54u) + hit;
    EF_U32(0x004209ccu) = EF_U32(0x004209ccu) + total;
    EF_U32(0x00420a54u) = total;
}

/* 0xd7c40, 118 bytes. */
void copy_block_d7c40(void)
{
    uint32_t i;
    for (i = 0; i < 8; ++i)
        EF_U16(0x00420e92u + i * 2u) = EF_U16(0x0015baf2u + i * 2u);
    guest_fn_c4a50(0x00420e80u);
    guest_fn_d7bf0();
}

/* 0xd7e94, 118 bytes. */
uint32_t permute_block_d7e94(void)
{
    EF_U16(0x0015bb02u) = EF_U16(0x0015baeeu);
    EF_U16(0x0015bb06u) = EF_U16(0x0015bafeu);
    EF_U16(0x0015bb08u) = EF_U16(0x0015bb00u);
    EF_U16(0x0015bb0au) = EF_U16(0x0015bafau);
    EF_U16(0x0015bb0cu) = EF_U16(0x0015bafcu);
    EF_U16(0x0015bb0eu) = EF_U16(0x0015baf6u);
    EF_U16(0x0015bb10u) = EF_U16(0x0015baf8u);
    EF_U16(0x0015bb12u) = EF_U16(0x0015baf2u);
    EF_U16(0x0015bb14u) = EF_U16(0x0015baf4u);
    return guest_fn_d8170(0x0015bb02u);
}

/* 0xd8358, 96 bytes. Advance the wrapping position at word 0x15bb3c by delta/4. */
void advance_position_d8358(int16_t delta)
{
    int16_t limit = EF_U16(0x0015bb40u) == 0 ? 0x55f : 0xabe;
    int16_t position = (int16_t)((int16_t)EF_U16(0x0015bb3cu) + ((int32_t)delta >> 2));
    if (position > limit)
        position = (int16_t)(position - limit);
    if (position < 0)
        position = (int16_t)(position + limit);
    EF_U16(0x0015bb3cu) = (uint16_t)position;
}

/* 0xdc3d5, 91 bytes. Reset the scale factors and the patched shift counts to 8. */
void reset_scale_dc3d5(void)
{
    EF_U32(0x0015bdd4u) = 0x100;
    EF_U32(0x0015bdd8u) = 0x100;
    EF_U8(0x000c5a77u) = 8;
    EF_U8(0x000c5aa3u) = 8;
    EF_U8(0x000c59fdu) = 8;
    EF_U8(0x000c5a29u) = 8;
    EF_U8(0x000dc5e8u) = 8;
    EF_U8(0x000dc620u) = 8;
    EF_U8(0x00164660u) = 8;
    EF_U8(0x00164664u) = 8;
    EF_U8(0x000c738bu) = 8;
    EF_U8(0x000c73a0u) = 8;
    EF_U8(0x0015c090u) = 8;
    EF_U8(0x0015c0a4u) = 8;
    EF_U8(0x000c682cu) = 8;
}

/* 0xddc88, 96 bytes. */
uint32_t query_mouse_ddc88(void)
{
    EF_U32(0x004260d0u) = 0x1500;
    guest_fn_8b8(0x2f, 0x004260b4u);
    if (EF_U32(0x004260c4u) == 0) {
        EF_U8(0x0015ee99u) = 0;
    } else {
        EF_U8(0x0015ee99u) = 1;
        EF_U16(0x0042611eu) = EF_U16(0x004260c4u);
        EF_U16(0x00426128u) = EF_U16(0x004260c4u);
        EF_U16(0x00426120u) = EF_U16(0x004260ccu);
        EF_U16(0x0042612au) = EF_U16(0x004260ccu);
    }
    return EF_U32(0x004260c4u);
}

/* Common tail of the three mouse-request builders. */
static void mouse_request_tail(uint32_t cursor)
{
    EF_U16(0x004260d6u) = (uint16_t)(cursor >> 16);
    EF_U32(0x004260c4u) = cursor & 0xffffu;
    EF_U32(0x004260d0u) = 0x1510;
    EF_U32(0x004260ccu) = (uint32_t)(EF_S32(0x0042611eu) >> 16);
    EF_U32(0x004260c8u) = 0;
    guest_fn_8b8(0x2f, 0x004260b4u);
}

/* 0xde12c, 141 bytes. */
void mouse_request_de12c(void)
{
    uint32_t cursor = guest_fn_604();
    ef_addr32 reply = EF_U32(0x004260a4u);
    ef_addr32 request;
    EF_U8(reply) = 2;
    request = EF_U32(0x00426098u);
    EF_U8(request) = 0x1a;
    EF_U8(request + 2u) = 0x0c;
    EF_U16(request + 0x12u) = 0x0b;
    EF_U8(request + 1u) = EF_U8(0x0015ee98u);
    EF_U16(request + 0x10u) = (uint16_t)(cursor >> 16);
    EF_U16(request + 0xeu) = (uint16_t)cursor;
    mouse_request_tail(guest_fn_604());
}

/* 0xde2d8, 153 bytes. */
uint32_t mouse_request_de2d8(void)
{
    uint32_t cursor = guest_fn_604();
    ef_addr32 reply = EF_U32(0x004260a4u);
    ef_addr32 request;
    EF_U8(reply) = 0x0c;
    request = EF_U32(0x00426098u);
    EF_U8(request) = 0x1a;
    EF_U16(request + 0x12u) = 0x0b;
    EF_U8(request + 2u) = 3;
    EF_U8(request + 1u) = EF_U8(0x0015ee98u);
    EF_U16(request + 0x10u) = (uint16_t)(cursor >> 16);
    EF_U16(request + 0xeu) = (uint16_t)cursor;
    mouse_request_tail(guest_fn_604());
    return EF_U8(EF_U32(0x004260a4u));
}

/* 0xde4b0, 125 bytes. */
void mouse_request_de4b0(void)
{
    ef_addr32 request;
    EF_U8(0x00426147u) = 0;
    guest_fn_b3fb0(EF_U32(0x0042609cu), 0, 0x16);
    request = EF_U32(0x0042609cu);
    EF_U8(request + 2u) = 0x85;
    EF_U8(request) = 0x16;
    EF_U8(request + 1u) = EF_U8(0x0015ee98u);
    mouse_request_tail(guest_fn_604());
}
