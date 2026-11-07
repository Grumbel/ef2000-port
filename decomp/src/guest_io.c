/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"

/* Watcom's register ABI maps these arguments to DX and AL for OUT DX,AL. */
static void output_port(uint32_t port, uint32_t value);
#pragma aux output_port parm [edx] [eax] modify [] = "out dx,al"

void guest_outp_x(uint32_t port, uint32_t value)
{
    output_port(port, value);
}

static uint32_t input_port(uint32_t port);
#pragma aux input_port parm [edx] value [eax] modify [eax] = "in al,dx"

uint32_t guest_inp_x(uint32_t port)
{
    return input_port(port);
}

/* The hybrid keeps these DOS dispatchers in the original image: they execute
 * the X-32 interrupt thunk and preserve its flags/register-block quirks. */
void guest_dos_int_x(uint32_t intno, uint32_t in, uint32_t out,
                     uint32_t sregs)
{
    int386x_core_9c47a(intno, in, out, sregs);
}

void guest_dos_intdos_x(uint32_t in, uint32_t out, uint32_t sregs)
{
    intdos_2b0(in, out);
    (void)sregs;
}
