#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Experimental portable execution of the original X-32 protected-mode image.

This is deliberately a *bounded startup probe*, not a playable backend yet.
The first X-32 requests are modeled here so we can compare the original
instruction stream against the native harness before porting its services.
The CPU uses a synthetic ring-0 LDT with flat segments. Real X-32 ring-3
selectors, segment limits, fault recovery, interrupts and hardware are not
implemented by this probe.
"""

import argparse
import json
from pathlib import Path
import struct

IMAGE_SIZE = 0x4000000
INITIAL_SP = IMAGE_SIZE - 0x1000
ENTRY = 0x86
DEFAULT_INSTRUCTION_LIMIT = 6000000


class UnsupportedGuestOperation(RuntimeError):
    """Guest requested an interface not implemented by this probe."""


def startup_memory(image):
    """Return the memory writes performed by the Linux harness before entry."""
    if len(image) > IMAGE_SIZE or len(image) < ENTRY + 2:
        raise ValueError('invalid protected-mode image length')
    return (
        (0x120cc4, struct.pack('<H', 0x1f)),
        # The probe uses a zero-based flat data segment (DS=0), so DOS memory
        # has a positive offset. This must change with real selector support.
        (0x120cc6, struct.pack('<I', 0x01000000)),
        (0x132a24, struct.pack('<H', 0x27)),
        (0x195fb0, b'\xa5' * (0x4de2f0 - 0x195fb0)),
    )


def serve_startup_int21(ip, regs):
    """Model only the two verified early X-32/DOS service requests.

    Return a new register dictionary, leaving unrelated registers intact.
    All subsequent services must be implemented using the shared host model.
    """
    result = dict(regs)
    ax = regs['ax'] & 0xffff
    if ip == 0x93 and ax == 0x3505 and regs['bx'] & 0xffff == 4 and regs['cx'] == 0x260:
        # The original stores DS into BX. The native harness uses DS=7;
        # this probe uses ring-0 selector 4 until CPL3 is implemented.
        result['cx'] = 0x260
        result['flags'] = (regs['flags'] | 0x40) & ~1
    elif ip == 0xa0 and ax == 0x3005:
        result['ax'] = 5
        result['bx'] &= ~255
        result['cx'] &= 0xffff0000
    else:
        raise UnsupportedGuestOperation(f'unimplemented INT 21h AX={ax:04x} at IP={ip:08x}')
    return result


def run(image, instruction_limit=DEFAULT_INSTRUCTION_LIMIT):
    """Run a bounded original-code probe; report the last guest interaction."""
    if instruction_limit <= 0:
        raise ValueError('instruction limit must be positive')
    from unicorn import Uc, UcError, UC_ARCH_X86, UC_MODE_32, UC_HOOK_INTR, UC_HOOK_CODE
    from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
        UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP,
        UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EFLAGS,
        UC_X86_REG_GDTR, UC_X86_REG_LDTR, UC_X86_REG_CS,
        UC_X86_REG_DS, UC_X86_REG_ES, UC_X86_REG_SS,
        UC_X86_REG_FS, UC_X86_REG_GS)

    registers = {'ax': UC_X86_REG_EAX, 'bx': UC_X86_REG_EBX,
                 'cx': UC_X86_REG_ECX, 'dx': UC_X86_REG_EDX,
                 'si': UC_X86_REG_ESI, 'di': UC_X86_REG_EDI,
                 'bp': UC_X86_REG_EBP, 'sp': UC_X86_REG_ESP,
                 'ip': UC_X86_REG_EIP, 'flags': UC_X86_REG_EFLAGS}
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(0, IMAGE_SIZE)
    # Guest selectors reference a synthetic LDT. Establish the descriptor
    # tables explicitly: unlike the Linux backend there is no modify_ldt().
    table_base = IMAGE_SIZE
    cpu.mem_map(table_base, 0x1000)
    def segment(base, limit, access, flags=0xc):
        return struct.pack('<HHBBBB', limit & 0xffff, base & 0xffff,
            (base >> 16) & 0xff, access, ((limit >> 16) & 0xf) | (flags << 4),
            (base >> 24) & 0xff)
    gdt = bytearray(0x20)
    gdt[8:16] = segment(table_base + 0x100, 0xff, 0x82, 0)
    cpu.mem_write(table_base, bytes(gdt))
    ldt = bytearray(0x100)
    for index in range(6):
        ldt[index * 8:index * 8 + 8] = segment(0, 0xffff, 0x9a if index == 5 else 0x92)
    cpu.mem_write(table_base + 0x100, bytes(ldt))
    cpu.reg_write(UC_X86_REG_GDTR, (0, table_base, len(gdt) - 1, 0))
    cpu.reg_write(UC_X86_REG_LDTR, (8, table_base + 0x100, len(ldt) - 1, 0))
    for reg, selector in ((UC_X86_REG_DS, 4),
                          (UC_X86_REG_ES, 4), (UC_X86_REG_SS, 4),
                          (UC_X86_REG_FS, 0xc), (UC_X86_REG_GS, 0x14),
                          (UC_X86_REG_CS, 0x2c)):
        try:
            cpu.reg_write(reg, selector)
        except UcError as exc:
            raise RuntimeError(f'cannot initialize selector {selector:#x}: {exc}; '
                f'LDTR={cpu.reg_read(UC_X86_REG_LDTR)}') from exc
    cpu.mem_write(0, image)
    for address, payload in startup_memory(image):
        cpu.mem_write(address, payload)
    cpu.reg_write(UC_X86_REG_ESP, INITIAL_SP)
    cpu.reg_write(UC_X86_REG_EIP, ENTRY)
    cpu.reg_write(UC_X86_REG_EFLAGS, 0x202)
    events = []
    result = {'status': 'instruction_limit', 'events': events,
              'instruction_limit': instruction_limit}

    def interrupt(uc, vector, _user):
        ip = uc.reg_read(UC_X86_REG_EIP) - 2
        regs = {name: uc.reg_read(reg) for name, reg in registers.items()}
        events.append({'ip': ip, 'vector': vector, 'ax': regs['ax'],
                       'bx': regs['bx'], 'cx': regs['cx'], 'sp': regs['sp']})
        if ip == 0x19a and vector == 0x21:
            # The original image, not the host, clears this initialized region.
            # The exact register values are useful for native/Unicorn comparison.
            result['startup_checkpoint'] = {
                'bss_zero': uc.mem_read(0x195fb0, 16) == b'\0' * 16,
                'si': regs['si'], 'di': regs['di'],
                'sp': regs['sp'], 'cx': regs['cx'],
            }
        try:
            if vector != 0x21:
                raise UnsupportedGuestOperation(f'unimplemented INT {vector:02x}h at IP={ip:08x}')
            next_regs = serve_startup_int21(ip, regs)
        except UnsupportedGuestOperation as exc:
            result['status'] = 'unsupported_interface'
            result['detail'] = str(exc)
            uc.emu_stop()
            return
        for name, reg in registers.items():
            if regs[name] != next_regs[name]:
                uc.reg_write(reg, next_regs[name])

    def instruction(uc, address, size, _user):
        if not ENTRY <= address < IMAGE_SIZE:
            result['status'] = 'unexpected_execution_address'
            result['detail'] = f'IP={address:08x}'
            uc.emu_stop()

    cpu.hook_add(UC_HOOK_INTR, interrupt)
    cpu.hook_add(UC_HOOK_CODE, instruction)
    try:
        cpu.emu_start(ENTRY, IMAGE_SIZE, count=instruction_limit)
    except UcError as exc:
        result['status'] = 'cpu_fault'
        result['detail'] = str(exc)
    result['ip'] = cpu.reg_read(UC_X86_REG_EIP)
    result['ax'] = cpu.reg_read(UC_X86_REG_EAX)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path, help='original _EF2000.EXE')
    parser.add_argument('--instruction-limit', type=int, default=DEFAULT_INSTRUCTION_LIMIT)
    parser.add_argument('--report', type=Path, help='optional JSON diagnostic output')
    args = parser.parse_args()
    try:
        from .extract_image import extract
    except ImportError:
        from extract_image import extract
    _manifest, images = extract(args.executable.read_bytes())
    report = run(images['payload.bin'], args.instruction_limit)
    print(json.dumps(report, indent=2))
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + '\n')
    # An unsupported guest request is the expected diagnostic boundary, not
    # successful full game execution.
    return 20 if report['status'] == 'unsupported_interface' else 21


if __name__ == '__main__':
    raise SystemExit(main())
