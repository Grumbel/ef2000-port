#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Experimental portable execution of the original X-32 protected-mode image.

This is deliberately a *bounded startup probe*, not a playable backend yet.
The first X-32 requests are modeled here so we can compare the original
instruction stream against the native harness before porting its services.
The CPU now enters ring 3 with original X-32 selectors through an IRETD
trampoline and a synthetic LDT. The image base remains zero rather than the
Linux native backend's 0x10000000. Fault recovery, IRQ delivery, and complete
port/device emulation are not implemented by this probe.
"""

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import struct

try:
    from .x32_allocation import Allocation
    from .unicorn_files import DosFileError, DosFiles
except ImportError:
    from x32_allocation import Allocation
    from unicorn_files import DosFileError, DosFiles

IMAGE_SIZE = 0x4000000
INITIAL_SP = IMAGE_SIZE - 0x1000
ENTRY = 0x86
DEFAULT_INSTRUCTION_LIMIT = 30000000


class UnsupportedGuestOperation(RuntimeError):
    """Guest requested an interface not implemented by this probe."""


class KeyboardController:
    """Keyboard-controller ACK handshake already modeled by the Linux harness.

    This covers writing a device command to port 60h, polling port 64h for
    output data, and consuming the device's 0FAh acknowledgement. It does not
    claim to emulate controller commands or queued physical key events.
    """

    def __init__(self):
        self.ack_pending = False

    def read(self, port):
        if port == 0x64:
            return int(self.ack_pending)
        if port == 0x60:
            if self.ack_pending:
                self.ack_pending = False
                return 0xfa
            return 0
        raise UnsupportedGuestOperation(f'unsupported keyboard port {port:04x}')

    def write(self, port, value):
        if port != 0x60:
            raise UnsupportedGuestOperation(f'unsupported keyboard controller command {value:02x}')
        self.ack_pending = True


def startup_memory(image):
    """Return the memory writes performed by the Linux harness before entry."""
    if len(image) > IMAGE_SIZE or len(image) < ENTRY + 2:
        raise ValueError('invalid protected-mode image length')
    return (
        # The X-32 loader presents PSP, command tail and environment via
        # LDT selectors. Without these the game follows a different boot path
        # than the working Linux native harness (notably DID archive lookup).
        (0x02000080, b'\x04 gui\r'),
        (0x02001000, b'DID=.\0quick_flag=1\0\0\x01\0C:\\EF2000\\_EF2000.EXE\0'),
        (0x120cc4, struct.pack('<H', 0x1f)),
        # Both probe modes map DS to base zero, so DOS RAM offsets are positive.
        (0x120cc6, struct.pack('<I', 0x01000000)),
        (0x132a24, struct.pack('<H', 0x27)),
        (0x195fb0, b'\xa5' * (0x4de2f0 - 0x195fb0)),
    )


def serve_startup_int21(ip, regs, allocation=None, now=None):
    """Model the verified early X-32/DOS memory service requests.

    Return a new register dictionary, leaving unrelated registers intact.
    All subsequent services must be implemented using the shared host model.
    """
    result = dict(regs)
    ax = regs['ax'] & 0xffff
    if ip == 0x93 and ax == 0x3505 and regs['bx'] & 0xffff in (4, 7) and regs['cx'] == 0x260:
        # The original stores DS into BX; ring 3 uses selector 7. The legacy
        # diagnostic ring-0 mode retains selector 4.
        result['cx'] = 0x260
        result['flags'] = (regs['flags'] | 0x40) & ~1
    elif ip == 0xa0 and ax == 0x3005:
        result['ax'] = 5
        result['bx'] &= ~255
        result['cx'] &= 0xffff0000
    elif ip == 0x19a and ax == 0x3509 and allocation is not None:
        reserved = allocation.reserve_stack(regs['bx'], regs['cx'], regs['flags'])
        if reserved is None:
            raise UnsupportedGuestOperation('X-32 stack reserve unsupported or exhausted')
        result['dx'], result['flags'] = reserved
    elif ip == 0x245 and ax == 0x3508 and allocation is not None:
        grown = allocation.grow_heap(regs['cx'], regs['flags'])
        if grown is None:
            raise UnsupportedGuestOperation('X-32 heap growth unsupported or exhausted')
        result['ax'], result['cx'], result['flags'] = grown
    elif ax == 0x350b and allocation is not None:
        result['ax'] = min(allocation.heap_ceiling - allocation.heap_end, allocation.free_bytes)
        result['flags'] &= ~1
    elif ax == 0x252b and (((regs['bx'] >> 8) & 255) not in (5, 6) or
                           (regs['bx'] & 255) > 1):
        # The original extender rejects invalid residency subfunctions with
        # a sentinel EAX and carry. The native Linux bridge preserves this.
        result['ax'] = 0xa5a5a5a5
        result['flags'] |= 1
    elif (regs['ax'] >> 8) & 0xff == 0x2c:
        # DOSBox-X returns UTC wall-clock HH:MM:SS:hundredths in CX/DX.
        now = now or datetime.now(timezone.utc)
        result['ax'] = 0x2c00
        result['cx'] = (regs['cx'] & 0xffff0000) | (now.hour << 8) | now.minute
        result['dx'] = (regs['dx'] & 0xffff0000) | (now.second << 8) | (now.microsecond // 10000)
    elif (regs['ax'] >> 8) & 0xff == 0x2a:
        now = now or datetime.now(timezone.utc)
        result['ax'] = (regs['ax'] & 0xffffff00) | ((now.weekday() + 1) % 7)
        result['cx'] = (regs['cx'] & 0xffff0000) | now.year
        result['dx'] = (regs['dx'] & 0xffff0000) | (now.month << 8) | now.day
    else:
        raise UnsupportedGuestOperation(f'unimplemented INT 21h AX={ax:04x} at IP={ip:08x}')
    return result


class VectorState:
    """Small synthetic X-32 interrupt-vector model used by the startup probe."""

    def __init__(self, code_selector=0x2c):
        self.real = [0] * 256
        self.protected = [(IMAGE_SIZE + 0x100, code_selector)] * 256
        self.exceptions = [(IMAGE_SIZE + 0x100, code_selector)] * 17

    def service(self, regs):
        result = dict(regs)
        function = regs['ax'] & 0xffff
        vector = regs['cx'] & 0xff
        if function == 0x2503:
            result['bx'] = self.real[vector]
        elif function == 0x2502:
            result['bx'], result['es'] = self.protected[vector]
        elif function == 0x2504:
            self.protected[vector] = (regs['dx'], regs['ds'])
        elif function == 0x2505:
            self.real[vector] = regs['bx']
        elif function == 0x2506:
            self.protected[vector] = (regs['dx'], regs['ds'])
            self.real[vector] = 0xf0000000 | (vector << 2)
        elif function == 0x2532 and vector < 16:
            result['bx'], result['es'] = (self.protected[vector] if vector < 8
                                           else self.exceptions[vector])
        elif function == 0x2533 and vector <= 16 and regs['ds'] == self.exceptions[0][1]:
            self.exceptions[vector] = (regs['dx'], regs['ds'])
            if vector < 8:
                self.protected[vector] = (regs['dx'], regs['ds'])
        elif function == 0x250c:
            result['ax'] = (regs['ax'] & 0xffff0000) | 0x7008
            result['bx'] &= 0xffff0000
        else:
            raise UnsupportedGuestOperation(f'unimplemented X-32 vector service AX={function:04x}')
        result['flags'] &= ~1
        return result


class ConventionalMemory:
    """Observed X-32 paragraph-allocation service over the DOS RAM window."""

    def __init__(self):
        self.end = 0x10000

    def allocate(self, regs):
        result = dict(regs)
        paragraphs = regs['bx'] & 0xffff
        if not paragraphs:
            raise UnsupportedGuestOperation('unimplemented zero-paragraph DOS allocation')
        size = paragraphs << 4
        available = 0x90000 - self.end
        if size > available:
            result['ax'] = 8
            result['bx'] = available >> 4
            result['flags'] |= 1
        else:
            result['ax'] = self.end >> 4
            # The native harness returns a pointer relative to DS at
            # 0x10000000. Our ring-0 flat data selector has base zero.
            result['bx'] = 0x01000000 + self.end
            self.end += size
            result['flags'] &= ~1
        return result


def segment_descriptor(base, limit, access, flags=0xc):
    """Encode a protected-mode segment descriptor for Unicorn's LDT."""
    return struct.pack('<HHBBBB', limit & 0xffff, base & 0xffff,
        (base >> 16) & 0xff, access, ((limit >> 16) & 0xf) | (flags << 4),
        (base >> 24) & 0xff)


class X32Selectors:
    """X-32 descriptor allocation with immediate updates to the guest LDT."""

    def __init__(self, cpu, ldt_base):
        self.cpu = cpu
        self.ldt_base = ldt_base
        self.entries = [None] * 16

    def service(self, regs):
        result = dict(regs)
        function = regs['ax'] & 0xffff
        if function == 0x3501:
            try:
                index = self.entries.index(None)
            except ValueError:
                result['bx'] = 0
                result['flags'] |= 1
                return result
            self.entries[index] = [0, 0xffffffff]
            result['bx'] = ((index + 6) << 3) | 7
            self._write(index)
        elif function == 0x3504 and regs['bx'] & 0xffff in (4, 7, 0x2c, 0x2f):
            result['cx'] = 0  # Synthetic flat image base, not Linux GUEST_BASE.
        elif (regs['bx'] & 7) == 7 and 6 <= (regs['bx'] >> 3) < 22:
            index = (regs['bx'] >> 3) - 6
            entry = self.entries[index]
            if entry is None:
                raise UnsupportedGuestOperation(f'X-32 selector {regs["bx"]:#x} not allocated')
            if function == 0x3502:
                self.entries[index] = None
            elif function == 0x3503:
                entry[0] = regs['cx']
            elif function == 0x3504:
                result['cx'] = entry[0]
            elif function == 0x3505:
                entry[1] = regs['cx'] | (0xfff if regs['cx'] > 0xfffff else 0)
            else:
                raise UnsupportedGuestOperation(f'unknown selector service AX={function:04x}')
            if function != 0x3504:
                self._write(index)
        else:
            raise UnsupportedGuestOperation(f'unhandled X-32 selector service AX={function:04x}')
        result['flags'] &= ~1
        return result

    def _write(self, index):
        entry = self.entries[index]
        if entry is None:
            descriptor = b'\0' * 8
        else:
            base, limit = entry
            descriptor = segment_descriptor(base, limit >> 12 if limit > 0xfffff else limit,
                                            0xf2, 0xc if limit > 0xfffff else 0x4)
        self.cpu.mem_write(self.ldt_base + (index + 6) * 8, descriptor)


def run(image, instruction_limit=DEFAULT_INSTRUCTION_LIMIT, game_dir=None,
        cd_dir=None, overlay_dir=None, ring3=True, sound='off'):
    """Run a bounded original-code probe; report the last guest interaction."""
    if instruction_limit <= 0:
        raise ValueError('instruction limit must be positive')
    if sound not in ('off', 'original'):
        raise ValueError('sound must be off or original')
    from unicorn import Uc, UcError, UC_ARCH_X86, UC_MODE_32, UC_HOOK_INTR, UC_HOOK_CODE, UC_HOOK_INSN
    from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
        UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP,
        UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EFLAGS,
        UC_X86_REG_GDTR, UC_X86_REG_LDTR, UC_X86_REG_CS,
        UC_X86_REG_DS, UC_X86_REG_ES, UC_X86_REG_SS,
        UC_X86_REG_FS, UC_X86_REG_GS, UC_X86_INS_IN, UC_X86_INS_OUT)

    registers = {'ax': UC_X86_REG_EAX, 'bx': UC_X86_REG_EBX,
                 'cx': UC_X86_REG_ECX, 'dx': UC_X86_REG_EDX,
                 'si': UC_X86_REG_ESI, 'di': UC_X86_REG_EDI,
                 'bp': UC_X86_REG_EBP, 'sp': UC_X86_REG_ESP,
                 'ip': UC_X86_REG_EIP, 'flags': UC_X86_REG_EFLAGS,
                 'ds': UC_X86_REG_DS, 'es': UC_X86_REG_ES,
                 'ss': UC_X86_REG_SS}
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(0, IMAGE_SIZE)
    # Guest selectors reference a synthetic LDT. Establish the descriptor
    # tables explicitly: unlike the Linux backend there is no modify_ldt().
    table_base = IMAGE_SIZE
    cpu.mem_map(table_base, 0x1000)
    gdt = bytearray(0x30)
    gdt[8:16] = segment_descriptor(table_base + 0x100, 0xff, 0x82, 0)
    if ring3:
        gdt[16:24] = segment_descriptor(0, 0xffff, 0x9a)
        gdt[24:32] = segment_descriptor(0, 0xffff, 0x92)
    cpu.mem_write(table_base, bytes(gdt))
    ldt = bytearray(0x100)
    selector_bases = (0, 0x02000000, 0x02001000, 0x01000000,
                      0x02002000, 0)
    for index, base in enumerate(selector_bases):
        # DOS-facing selectors keep the original RPL=3 value; the primary
        # protected data/stack selectors remain RPL=0 for this probe.
        access = ((0xfa if ring3 else 0x9a) if index == 5 else
                  (0xf2 if ring3 or index in (1, 2, 3, 4) else 0x92))
        if index == 0 and ring3:
            ldt[:8] = segment_descriptor(0, 0x7f, 0xf6, 0x4)
        else:
            ldt[index * 8:index * 8 + 8] = segment_descriptor(base, 0xffff, access)
    cpu.mem_write(table_base + 0x100, bytes(ldt))
    cpu.reg_write(UC_X86_REG_GDTR, (0, table_base, len(gdt) - 1, 0))
    cpu.reg_write(UC_X86_REG_LDTR, (8, table_base + 0x100, len(ldt) - 1, 0))
    initial_segments = (((UC_X86_REG_DS, 0x18), (UC_X86_REG_ES, 0x18),
                         (UC_X86_REG_SS, 0x18), (UC_X86_REG_FS, 0x18),
                         (UC_X86_REG_GS, 0x18), (UC_X86_REG_CS, 0x10))
                        if ring3 else
                        ((UC_X86_REG_DS, 4), (UC_X86_REG_ES, 4),
                         (UC_X86_REG_SS, 4), (UC_X86_REG_FS, 0xc),
                         (UC_X86_REG_GS, 0x14), (UC_X86_REG_CS, 0x2c)))
    for reg, selector in initial_segments:
        try:
            cpu.reg_write(reg, selector)
        except UcError as exc:
            raise RuntimeError(f'cannot initialize selector {selector:#x}: {exc}; '
                f'LDTR={cpu.reg_read(UC_X86_REG_LDTR)}') from exc
    cpu.mem_write(0, image)
    for address, payload in startup_memory(image):
        cpu.mem_write(address, payload)
    if ring3:
        # IRETD performs the privilege transition that direct uc_reg_write(CS)
        # cannot. The user-mode trampoline loads DS/ES/FS/GS from the LDT.
        bootstrap = table_base + 0x200
        ring3_entry = table_base + 0x300
        code = bytearray.fromhex('66 b8 07 00 8e d8 8e c0 66 b8 0f 00 8e e0 '
                                  '66 b8 17 00 8e e8 e9 00 00 00 00')
        struct.pack_into('<i', code, len(code) - 4,
                         ENTRY - (ring3_entry + len(code)))
        cpu.mem_write(bootstrap, b'\xcf')
        cpu.mem_write(ring3_entry, bytes(code))
        cpu.mem_write(INITIAL_SP - 20,
                      struct.pack('<5I', ring3_entry, 0x2f, 0x3202, INITIAL_SP, 7))
        cpu.reg_write(UC_X86_REG_ESP, INITIAL_SP - 20)
        cpu.reg_write(UC_X86_REG_EIP, bootstrap)
    else:
        cpu.reg_write(UC_X86_REG_ESP, INITIAL_SP)
        cpu.reg_write(UC_X86_REG_EIP, ENTRY)
    cpu.reg_write(UC_X86_REG_EFLAGS, 0x202)
    events = []
    allocation = Allocation()
    vectors = VectorState(0x2f if ring3 else 0x2c)
    conventional = ConventionalMemory()
    files = None
    if game_dir is not None:
        files = DosFiles(game_dir, cd_dir, overlay_dir)
        if sound == 'off':
            try:
                original_config = DosFiles._walk(files.root, ['EF2000.CFG'], must_exist=True)
                lines = original_config.read_bytes().splitlines()
            except (DosFileError, OSError):
                # Keep an explicit DOS missing-file result for absent configs.
                pass
            else:
                changes = {b'MUSIC': b'0,127', b'SOUNDFX': b'0,127',
                           b'SPEECH': b'0', b'VRHELMET': b'0'}
                files.config_data = b'\r\n'.join(
                    line.split(b'=', 1)[0] + b'=' + changes[line.split(b'=', 1)[0]]
                    if line.split(b'=', 1)[0] in changes else line for line in lines) + b'\r\n'
    selectors = X32Selectors(cpu, table_base + 0x100)
    keyboard = KeyboardController()
    result = {'status': 'instruction_limit', 'events': events, 'console': [],
              'cpu_mode': 'x86-ring3-iopl3' if ring3 else 'x86-ring0-flat',
              'privileged_ops': [],
              'instruction_limit': instruction_limit}

    def interrupt(uc, vector, _user):
        # Software INT advances EIP; hardware #GP reports the faulting EIP.
        raw_ip = uc.reg_read(UC_X86_REG_EIP)
        ip = raw_ip if vector == 13 else raw_ip - 2
        regs = {name: uc.reg_read(reg) for name, reg in registers.items()}
        if vector == 13 and ring3 and ENTRY <= ip < len(image):
            opcode = uc.mem_read(ip, 1)[0]
            if opcode in (0xfa, 0xfb):
                result['privileged_ops'].append({'ip': ip, 'operation': 'cli' if opcode == 0xfa else 'sti'})
                uc.reg_write(UC_X86_REG_EIP, ip + 1)
                return
        event = {'ip': ip, 'vector': vector, 'ax': regs['ax'],
                 'bx': regs['bx'], 'cx': regs['cx'], 'dx': regs['dx'],
                 'ds': regs['ds'], 'es': regs['es'], 'sp': regs['sp']}
        if vector == 0x21 and (regs['ax'] >> 8) & 0xff in (0x3c, 0x3d):
            # A diagnostic-only guest filename, not an instruction to open a
            # host path. The main game directory remains completely untouched.
            address = regs['dx']
            if 0x260 < address < IMAGE_SIZE - 260:
                raw = bytes(uc.mem_read(address, 260)).split(b'\0', 1)[0]
                if raw and all(32 <= byte < 127 for byte in raw):
                    event['filename'] = raw.decode('ascii')
        events.append(event)
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
            if (regs['ax'] >> 8) & 0xff == 0x4c:
                result['status'] = 'guest_exit'
                result['exit_code'] = regs['ax'] & 0xff
                uc.emu_stop()
                return
            if (regs['ax'] & 0xffff) in (0x2502, 0x2503, 0x2504, 0x2505,
                                         0x2506, 0x250c, 0x2532, 0x2533):
                next_regs = vectors.service(regs)
            elif (regs['ax'] >> 8) & 0xff == 0x48:
                next_regs = conventional.allocate(regs)
            elif regs['ax'] & 0xffff == 0x252b and regs['bx'] & 0xffff in (0x501, 0x601):
                # All guest memory is already allocated in Unicorn. The X-32
                # lock service needs no corresponding host mlock on Windows.
                if regs['cx'] >= IMAGE_SIZE or regs['dx'] > IMAGE_SIZE - regs['cx']:
                    raise UnsupportedGuestOperation('invalid X-32 lock/unlock range')
                next_regs = dict(regs)
                next_regs['ax'] = 0x252b
                next_regs['flags'] &= ~1
            elif ip != 0x93 and regs['ax'] & 0xffff in (0x3501, 0x3502, 0x3503, 0x3504, 0x3505):
                next_regs = selectors.service(regs)
            elif (regs['ax'] >> 8) & 0xff == 0x40:
                handle, count, address = regs['bx'] & 0xffff, regs['cx'], regs['dx']
                if address <= 0x260 or address >= IMAGE_SIZE or count > IMAGE_SIZE - address or count > 65536:
                    raise UnsupportedGuestOperation('invalid console output buffer')
                payload = bytes(uc.mem_read(address, count))
                if handle in (1, 2):
                    result['console'].append(payload.decode('latin-1'))
                    next_regs = dict(regs, ax=count, flags=regs['flags'] & ~1)
                else:
                    if files is None:
                        raise UnsupportedGuestOperation('DOS file access needs --game-dir')
                    next_regs = dict(regs)
                    try:
                        next_regs['ax'] = files.write(handle, payload)
                        next_regs['flags'] &= ~1
                    except DosFileError as exc:
                        next_regs['ax'] = exc.code
                        next_regs['flags'] |= 1
            elif (regs['ax'] >> 8) & 0xff in (0x3c, 0x3d, 0x3e, 0x3f, 0x42, 0x68) or (
                    regs['ax'] & 0xffff == 0x4400):
                if files is None:
                    raise UnsupportedGuestOperation('DOS file access needs --game-dir')
                next_regs = dict(regs)
                ah = (regs['ax'] >> 8) & 0xff
                try:
                    if ah in (0x3c, 0x3d):
                        if 'filename' not in event:
                            raise DosFileError(3)
                        value = (files.create(event['filename']) if ah == 0x3c
                                 else files.open(event['filename'], regs['ax'] & 3))
                        next_regs['dx'] = 0
                    elif ah == 0x3e:
                        files.close(regs['bx'] & 0xffff)
                        value = 0x3e01
                    elif ah == 0x3f:
                        address, count = regs['dx'], regs['cx']
                        if address <= 0x260 or address >= IMAGE_SIZE or count > IMAGE_SIZE - address:
                            raise DosFileError(5)
                        chunk = files.read(regs['bx'] & 0xffff, count)
                        uc.mem_write(address, chunk)
                        next_regs['cx'] = next_regs['dx'] = 0
                        value = len(chunk)
                    elif ah == 0x42:
                        offset = ((regs['cx'] & 0xffff) << 16) | (regs['dx'] & 0xffff)
                        if offset & 0x80000000:
                            offset -= 0x100000000
                        position = files.seek(regs['bx'] & 0xffff, offset, regs['ax'] & 255)
                        next_regs['dx'] = (regs['dx'] & 0xffff0000) | ((position >> 16) & 0xffff)
                        value = position & 0xffff
                    elif ah == 0x68:
                        handle = regs['bx'] & 0xffff
                        if handle > 2:
                            files.flush(handle)
                        value = regs['ax']
                    else:
                        handle = regs['bx'] & 0xffff
                        if handle > 2 and handle not in files.handles:
                            raise DosFileError(6)
                        value = 0x80d3 if handle <= 2 else 2
                        next_regs['dx'] = (regs['dx'] & 0xffff0000) | value
                    next_regs['ax'] = value
                    next_regs['flags'] &= ~1
                except DosFileError as exc:
                    next_regs['ax'] = exc.code
                    next_regs['flags'] |= 1
            else:
                next_regs = serve_startup_int21(ip, regs, allocation)
                if ip == 0x93 and ring3:
                    # The first X-32 3505 call grows the expand-down data
                    # segment's forbidden low area from 0x7f to 0x260.
                    uc.mem_write(table_base + 0x100,
                                 segment_descriptor(0, 0x260, 0xf6, 0x4))
        except UnsupportedGuestOperation as exc:
            result['status'] = 'unsupported_interface'
            result['detail'] = str(exc)
            uc.emu_stop()
            return
        for name, reg in registers.items():
            if regs[name] != next_regs[name]:
                uc.reg_write(reg, next_regs[name])

    def instruction(uc, address, size, _user):
        if not (ENTRY <= address < IMAGE_SIZE or
                ring3 and table_base + 0x200 <= address < table_base + 0x400):
            result['status'] = 'unexpected_execution_address'
            result['detail'] = f'IP={address:08x}'
            uc.emu_stop()

    def port_in(uc, port, size, _user):
        if port == 0x201:  # No physical joystick: all four buttons/axes high.
            return (1 << (size * 8)) - 1
        if port in (0x60, 0x64):
            return keyboard.read(port)
        if port in (0x61, 0x3da):
            return 0
        result['status'] = 'unsupported_interface'
        result['detail'] = f'unsupported IN port={port:04x}'
        uc.emu_stop()
        return 0

    def port_out(uc, port, size, value, _user):
        if port == 0x60:
            keyboard.write(port, value)
            return
        if port in (0x20, 0x43, 0x40, 0x61, 0x3c8, 0x3c9, 0x201):
            return
        result['status'] = 'unsupported_interface'
        result['detail'] = f'unsupported OUT port={port:04x} value={value:08x}'
        uc.emu_stop()

    cpu.hook_add(UC_HOOK_INTR, interrupt)
    cpu.hook_add(UC_HOOK_CODE, instruction)
    if ring3:
        cpu.hook_add(UC_HOOK_INSN, port_in, None, 1, 0, UC_X86_INS_IN)
        cpu.hook_add(UC_HOOK_INSN, port_out, None, 1, 0, UC_X86_INS_OUT)
    try:
        cpu.emu_start(table_base + 0x200 if ring3 else ENTRY, 0xffffffff,
                      count=instruction_limit)
    except UcError as exc:
        result['status'] = 'cpu_fault'
        result['detail'] = str(exc)
    if files is not None:
        for handle in tuple(files.handles):
            try:
                files.flush(handle)
            except DosFileError as exc:
                result['status'] = 'host_io_error'
                result['detail'] = f'failed to flush DOS handle {handle}: {exc}'
                break
    result['ip'] = cpu.reg_read(UC_X86_REG_EIP)
    result['ax'] = cpu.reg_read(UC_X86_REG_EAX)
    result['allocation'] = vars(allocation).copy()
    result['conventional_end'] = conventional.end
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path, help='original _EF2000.EXE')
    parser.add_argument('--instruction-limit', type=int, default=DEFAULT_INSTRUCTION_LIMIT)
    parser.add_argument('--game-dir', type=Path, help='read-only asset directory (default: EXE directory)')
    parser.add_argument('--cd-dir', type=Path, help='read-only root for absolute DOS CD paths')
    parser.add_argument('--overlay-dir', type=Path,
                        help='separate writable DOS drive overlay (original game data remains read-only)')
    parser.add_argument('--sound', choices=['off', 'original'], default='off',
                        help='disable original sound drivers in a virtual config copy (default: off)')
    parser.add_argument('--flat-ring0', action='store_true', help='diagnose legacy flat ring-0 CPU model')
    parser.add_argument('--report', type=Path, help='optional JSON diagnostic output')
    args = parser.parse_args()
    try:
        from .extract_image import extract
    except ImportError:
        from extract_image import extract
    _manifest, images = extract(args.executable.read_bytes())
    report = run(images['payload.bin'], args.instruction_limit,
                 game_dir=args.game_dir or args.executable.parent,
                 cd_dir=args.cd_dir, overlay_dir=args.overlay_dir,
                 ring3=not args.flat_ring0, sound=args.sound)
    print(json.dumps(report, indent=2))
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + '\n')
    # An unsupported guest request is the expected diagnostic boundary, not
    # successful full game execution.
    return {'unsupported_interface': 20, 'guest_exit': 26}.get(report['status'], 21)


if __name__ == '__main__':
    raise SystemExit(main())
