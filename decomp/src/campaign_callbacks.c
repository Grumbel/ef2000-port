/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* These entry models intentionally expose unresolved GUI calls rather than
 * implying any original function has already passed guest differential. */
static uint32_t guest(uint32_t entry, uint32_t eax, uint32_t edx)
{
    return gc_ui_guest_call_085(entry, eax, edx);
}

static void copy_campaign_key(uint32_t source)
{
    unsigned i;
    /* Three MOVSD instructions with DF clear; original source is 12 bytes. */
    for (i = 0; i < 3u; ++i)
        EF_U32(0x41f30cu + i * 4u) = EF_U32(source + i * 4u);
}

/* Shared suffix of c40e50 and c40f9c. The EAX returned by the first
 * a78bc call feeds a72d8; the second EAX is reused after a7380. */
static void select_campaign_dialog(void)
{
    uint32_t root = guest(0xa46ccu, 0x184374u, 0x184384u);
    uint32_t child = guest(0xa78bcu, root, 0x184384u);
    uint32_t detail;
    guest(0xa72d8u, child, 0x184384u);
    detail = guest(0xa78bcu, root, 0x184388u);
    guest(0xa7380u, detail, detail);
    /* The original writes DH=6 without replacing the rest of EDX. */
    guest(0xa72d8u, detail, (detail & 0xffff00ffu) | 0x600u);
    EF_U8(0x12584au) = 6u;
}

/* 0x40e50, 107 bytes: start new campaign and enter difficulty selector. */
void gc_set_difficulty_40e50(void)
{
    EF_U8(0x1a364bu) = 0x32u;
    EF_U8(0x1a364cu) = 1u;
    guest(0xa4c84u, 0u, 0u);
    guest(0xa4d90u, 0x18436cu, 0u);
    guest(0xa4d90u, 0x184374u, 0u);
    select_campaign_dialog();
}

/* 0x40ebc, 8 bytes: set the campaign-performance flag. */
void gc_start_new_campaign_40ebc(void)
{
    EF_U8(0x1a364cu) = 1u;
}

/* 0x40ec4, 9 bytes: clear the campaign-performance flag. */
uint32_t gc_performance_yes_40ec4(uint32_t eax)
{
    /* XOR AH,AH also returns the incoming EAX with bits 8..15 cleared. */
    eax &= 0xffff00ffu;
    EF_U8(0x1a364cu) = 0u;
    return eax;
}

/* 0x40ed0, 19 bytes: parse the widget string at +0x3f and mirror AL. */
void gc_performance_no_40ed0(ef_addr32 widget)
{
    uint8_t value = (uint8_t)guest(0x9d4cdu, widget + 0x3fu, 0u);
    EF_U8(0x1264e2u) = value;
    EF_U8(0x12584au) = value;
}

/* 0x40ee4, 52 bytes: reset old-campaign window, default selector, and
 * copy the three original dwords to the 0x41f30c campaign key. */
void gc_load_old_campaign_40ee4(void)
{
    guest(0xa4c84u, 0u, 0u);
    guest(0xa4d90u, 0x18438cu, 1u);
    guest(0xc9fc4u, 0u, 1u);
    EF_U16(0x137992u) = 1u;
    copy_campaign_key(0x184358u);
}

/* 0x40f18, 73 bytes: accept existing campaign. */
void gc_loadfile_accepted_40f18(void)
{
    guest(0xa4db0u, 0x18436cu, 0u);
    EF_U16(0x137992u) = 0u;
    EF_U16(0x137994u) = 1u;
    copy_campaign_key(0x18439cu);
    guest(0xac7a8u, 0x184364u, 0u);
    guest(0xaca88u, 0u, 0u);
    guest(0xc9fc4u, 0u, 0u);
}

/* 0x40f64, 53 bytes: initialize menu with the current campaign key. */
void gc_init_main_menu_40f64(void)
{
    EF_U16(0x137992u) = 0u;
    EF_U16(0x137994u) = 1u;
    copy_campaign_key(0x18439cu);
    guest(0xac7a8u, 0x184364u, 0u);
}

/* 0x40f9c, 140 bytes: result of the indirect availability callback
 * selects the new-campaign path or a two-widget fallback. */
void gc_set_campaign_difficulty_40f9c(void)
{
    uint32_t decision;
    guest(0xa4c84u, 0u, 0u);
    decision = guest(EF_U32(0x132ab4u), 0x18439cu, 0u);
    if (decision != 0u) {
        guest(0xa4d90u, 0x18436cu, 0u);
        guest(0xa4d90u, 0x1843a8u, 0u);
    } else {
        EF_U8(0x1a364bu) = 0x32u;
        EF_U8(0x1a364cu) = 1u;
        guest(0xa4d90u, 0x184374u, 0u);
        select_campaign_dialog();
        guest(0xc9fc4u, 0u, 0u);
    }
}
