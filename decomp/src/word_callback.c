/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

/* 0xab820, 21 bytes. EAX value, six other registers preserved, plain RET.
 * Callers f1a02/f1a5f pass five and retain EDX/EBX respectively. */
void run_word_callback_ab820(uint32_t value)
{
    volatile uint32_t *callback = (volatile uint32_t *)
        ef_guest_memory_ptr(0x0041ed80u);

    /* Preserve separate original presence check and indirect-target read. */
    if (*callback != 0u)
        ((void (*)(uint32_t))*callback)(value & 0xffffu);
}
