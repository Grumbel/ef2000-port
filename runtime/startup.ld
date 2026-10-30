/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

ENTRY(_start)
PHDRS {
    guest PT_LOAD FLAGS(7);
    boot PT_LOAD FLAGS(5);
    dos PT_LOAD FLAGS(6);
    conventional PT_LOAD FLAGS(6);
    runtime PT_LOAD FLAGS(7);
}
SECTIONS {
    . = 0x10000000;
    .guest : { *(.guest) } :guest
    .guest_bss : { *(.guest_bss) } :guest
    . = 0x14000000;
    .guest_boot : { *(.guest_boot) } :boot
    . = 0x02000000;
    .dos : { *(.dos) } :dos
    . = 0x01000000;
    .conventional : { *(.conventional) } :conventional
    . = 0x08000000;
    .text : { *(.text*) } :runtime
    .rodata : { *(.rodata*) } :runtime
    .data : { *(.data*) } :runtime
    .bss : { *(.bss*) *(COMMON) } :runtime
    /DISCARD/ : { *(.note*) *(.comment) *(.eh_frame*) }
}
