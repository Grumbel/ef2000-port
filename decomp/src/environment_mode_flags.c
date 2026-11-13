/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xdb8a8, 8 bytes: set horizon-fix flag. */
void env_unfix_horizon_db8a8(void)
{
    EF_U8(0x425c86u) = 1u;
}

/* 0xdb8b0, 9 bytes: clear the flag via AH=0 (EAX returned with AH cleared). */
uint32_t env_load_horizon_db8b0(uint32_t eax)
{
    EF_U8(0x425c86u) = 0u;
    return eax & ~0xff00u;
}

/* 0xdbe28, 8 bytes: accumulate corridor bit, not assignment. */
void env_corridor_dbe28(void)
{
    EF_U8(0x425bc4u) |= 2u;
}

/* 0xdbe30, 33 bytes: update both feature masks. The original writes the
 * new first mask into AH without modifying AL or the upper 16 EAX bits. */
uint32_t env_planet2_dbe30(uint32_t eax)
{
    uint8_t bits = (uint8_t)(EF_U8(0x425bc4u) | 4u);
    EF_U8(0x425bc4u) = bits;
    EF_U8(0x159130u) |= 4u;
    return (eax & ~0xff00u) | ((uint32_t)bits << 8);
}

/* 0xdbe54, 8 bytes: the paired 0x40/0x02 flags are set together. */
void env_space_dbe54(void)
{
    EF_U8(0x425bc4u) |= 0x42u;
}

/* The seven adjacent named mode selectors were decoded from the original
 * 0xdbe5c..0xdbf20 instruction stream. Each is a separate entry point.
 * Their EAX models preserve everything except AH where the guest writes AH. */
uint32_t env_spacecorr_dbe5c(uint32_t eax)
{
    uint8_t mask = (uint8_t)(EF_U8(0x425bc4u) | 0x08u);
    EF_U8(0x425bc4u) = mask;
    EF_U8(0x159130u) |= 0x84u;
    return (eax & ~0xff00u) | ((uint32_t)mask << 8);
}

uint32_t env_rings_dbe80(uint32_t eax)
{
    uint8_t mask = (uint8_t)(EF_U8(0x425bc4u) | 0x08u);
    EF_U8(0x159130u) |= 0x84u;
    EF_U8(0x425bc4u) = mask;
    EF_U8(0x425c89u) = 1u;
    return (eax & ~0xff00u) | ((uint32_t)mask << 8);
}

uint32_t env_moon_dbeac(uint32_t eax)
{
    uint8_t mask = (uint8_t)(EF_U8(0x159130u) | 0x80u);
    EF_U16(0x425bc4u) |= 0x0108u;
    EF_U8(0x159130u) = mask;
    EF_U8(0x425c89u) = 1u;
    return (eax & ~0xff00u) | ((uint32_t)mask << 8);
}

void env_inferno_dbedc(void)
{
    EF_U8(0x425bc4u) |= 0x10u;
}

void env_shield_dbee4(void)
{
    EF_U8(0x425bc4u) |= 0x01u;
}

void env_city_dbeec(void)
{
    EF_U8(0x425bc4u) |= 0x22u;
}

uint32_t env_day_dbef8(uint32_t eax)
{
    uint8_t mask = (uint8_t)(EF_U8(0x159130u) | 0x80u);
    EF_U8(0x15bb5fu) = 0u;
    EF_U8(0x159130u) = mask;
    EF_U8(0x425bc5u) |= 0x08u;
    return (eax & ~0xff00u) | ((uint32_t)mask << 8);
}
