/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Constant-store setters. */

/* 0x1d420, 11 bytes. Reset the 0x132ae0 handler pointer. */
void reset_handler_1d420(void)
{
    EF_U32(0x00132ae0u) = 0x0001d42cu;
}

/* 0x768fc, 10 bytes. Set the 4000 limit word. */
void set_limit_768fc(void)
{
    EF_U16(0x00128554u) = 0x0fa0;
}

/* 0x77c28, 10 bytes. Set the 0x118 timer word. */
void set_timer_77c28(void)
{
    EF_U16(0x001f8d52u) = 0x0118;
}

/* 0x9854c, 11 bytes. Install the 0x984f0 handler. */
void set_handler_9854c(void)
{
    EF_U32(0x0012dd9cu) = 0x000984f0u;
}

/* 0xa45a8, 11 bytes. Reseed the generator. */
void reseed_generator_a45a8(void)
{
    EF_U32(0x000a45a4u) = 0x31415926u;
}

/* 0xe3c60, 11 bytes. Set the 0x159134 pointer to its default. */
void set_pointer_e3c60(void)
{
    EF_U32(0x00159134u) = 0x00431ef0u;
}

/* 0xeb24e, 11 bytes. Set the 0x17d400 limit to 0x8000. */
void set_limit_eb24e(void)
{
    EF_U32(0x0017d400u) = 0x8000u;
}

/* 0xfd371, 11 bytes. Set the 0x17bfea pointer to its default. */
void set_pointer_fd371(void)
{
    EF_U32(0x0017bfeau) = 0x0017cf8eu;
}
