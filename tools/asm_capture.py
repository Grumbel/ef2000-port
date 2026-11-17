# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Restore EFCAP002 inputs for the explicit-state ASM differential oracle.

Descriptors reconstruct segment caches from captured tables. EFCAP002 does not
contain hidden CPU caches; this is deliberately not a claim to restore caches
whose descriptors have changed since loading. Unsupported profiles fail closed.
"""

import struct

try:
    from . import asm_c_differential as differential
except ImportError:
    import asm_c_differential as differential


def live_table_windows(capture):
    """Return only complete descriptor tables backed by identical guest RAM.

    Restoration may map snapshot tables outside captured RAM. Such mappings
    cannot be exposed to the bounded C memory model as live descriptor tables.
    This does not enable segment reloads or reconstruct hidden caches.
    """
    windows = []
    for name in ('gdtr', 'ldtr'):
        table = capture.table_context[name]
        base, limit = table['base'], table['limit']
        raw = table['bytes']
        end = base + limit + 1
        if (0 <= base <= 0xffffffff and 0 <= limit <= 0xffffffff and
                end <= 0x100000000 and end <= len(capture.guest_ram) and
                len(raw) == limit + 1 and capture.guest_ram[base:end] == raw):
            windows.append((base, limit))
        else:
            windows.append(None)
    return tuple(windows)


def data_reload_target(code):
    """Recognize data-selector loads in an oracle-decoded instruction."""
    while code and code[0] in (0x26, 0x2e, 0x36, 0x3e, 0x64, 0x65, 0x66, 0x67):
        code = code[1:]
    if not code:
        return None
    if code[0] == 0x8e and len(code) >= 2:
        target = (code[1] >> 3) & 7
        return target if target in (0, 3, 4, 5) else None
    if code[0] in (0x07, 0x1f, 0xc4, 0xc5):
        return {0x07: 0, 0x1f: 3, 0xc4: 0, 0xc5: 3}[code[0]]
    if len(code) >= 2 and code[0] == 0x0f:
        return {0xa1: 4, 0xa9: 5, 0xb4: 4, 0xb5: 5}.get(code[1])
    return None


class DataReloadObserver:
    """Observe successful original reloads, including unchanged selectors.

    Save descriptor bytes before the instruction and commit the expectation
    only at the following instruction boundary. Later guest table writes
    must not retroactively change a hidden-cache expectation.
    """

    def __init__(self, cpu, capture):
        from unicorn import UC_HOOK_CODE, x86_const
        try:
            from . import asm_segments
        except ImportError:
            import asm_segments
        self.cpu, self.capture = cpu, capture
        self.decode = asm_segments.descriptor
        self.registers = [getattr(x86_const, 'UC_X86_REG_' + name.upper())
                          for name in ('es', 'cs', 'ss', 'ds', 'fs', 'gs')]
        self.caches = [self.decode(capture.registers[name],
                                  capture.table_context['descriptors'][name] or 0)
                       for name in ('es', 'cs', 'ss', 'ds', 'fs', 'gs')]
        self.pending = None
        self.hook = cpu.hook_add(UC_HOOK_CODE, self.before)

    def complete(self):
        if self.pending is None:
            return
        target, tables = self.pending
        selector = self.cpu.reg_read(self.registers[target])
        if selector & 0xfffc == 0:
            self.caches[target] = (selector, 0, 0, 0, 0)
        else:
            offset = selector & 0xfff8
            raw = tables[(selector >> 2) & 1][offset:offset + 8]
            if len(raw) != 8:
                raise ValueError('successful original reload outside captured table window')
            self.caches[target] = self.decode(selector, int.from_bytes(raw, 'little') | (1 << 40))
        self.pending = None

    def before(self, cpu, address, size, unused):
        self.complete()
        target = data_reload_target(bytes(cpu.mem_read(address, size)))
        if target is not None:
            tables = self.capture.table_context
            self.pending = (target, tuple(bytes(cpu.mem_read(tables[name]['base'],
                                                           tables[name]['limit'] + 1))
                                          for name in ('gdtr', 'ldtr')))

    def finish(self):
        self.complete()
        self.cpu.hook_del(self.hook)


def restore(capture):
    """Return a fresh Unicorn CPU with the captured zero-base ring-3 context.

    An IRET establishes CPL as well as CS/SS. Direct selector writes alone do
    not establish CPL in Unicorn and reject the game's ring-3 SS. Temporary
    transition code, stack and GDT live outside guest RAM and are unmapped before
    returning. No guest instruction executes during restoration.
    """
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, x86_const

    if capture.format_version != 2 or capture.x87 is None or capture.table_context is None:
        raise ValueError('ASM context restoration requires EFCAP002')
    tables = capture.table_context
    regs = capture.registers
    if regs['eflags'] & ((1 << 17) | (1 << 14)):
        raise ValueError('VM86 and nested-task captures are unsupported')
    for name in ('gdtr', 'ldtr'):
        table = tables[name]
        if table['bytes'] is None or len(table['bytes']) != table['limit'] + 1:
            raise ValueError('complete descriptor-table windows are required')
        if table['base'] + len(table['bytes']) > 0x10000000:
            raise ValueError('descriptor table overlaps transition address space')
    for name in ('es', 'cs', 'ss', 'ds', 'fs', 'gs'):
        selector, descriptor = regs[name], tables['descriptors'][name]
        if selector & 0xfffc == 0 and name in ('es', 'ds', 'fs', 'gs'):
            continue
        if descriptor is None:
            raise ValueError('unavailable descriptor for ' + name)
        base = ((descriptor >> 16) & 0xffff) | ((descriptor >> 32) & 255) << 16 | ((descriptor >> 56) & 255) << 24
        access = (descriptor >> 40) & 255
        if base or selector & 3 != 3 or access & 0xf0 != 0xf0:
            raise ValueError('requires present zero-base ring-3 descriptors: ' + name)
        if name == 'cs' and (access & 0xc != 8 or not descriptor & (1 << 54)):
            raise ValueError('requires nonconforming 32-bit code segment')
        if name == 'ss' and (access & 0xa != 2 or not descriptor & (1 << 54)):
            raise ValueError('requires writable 32-bit stack segment')

    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    ram_size = len(capture.guest_ram)
    cpu.mem_map(0, ram_size)
    cpu.mem_write(0, capture.guest_ram)
    mapped = set(range(0, ram_size, 4096))
    for name in ('gdtr', 'ldtr'):
        table = tables[name]
        first = table['base'] & ~4095
        end = (table['base'] + len(table['bytes']) + 4095) & ~4095
        for page in range(first, end, 4096):
            if page not in mapped:
                cpu.mem_map(page, 4096)
                mapped.add(page)
        cpu.mem_write(table['base'], table['bytes'])
        cpu.reg_write(getattr(x86_const, 'UC_X86_REG_' + name.upper()),
                      tuple(table[k] for k in ('selector', 'base', 'limit', 'flags')))

    scratch = 0x10000000
    cpu.mem_map(scratch, 4096)
    cpu.mem_write(scratch + 256, struct.pack('<QQQ', 0, 0x00cf9a000000ffff, 0x00cf92000000ffff))
    cpu.reg_write(x86_const.UC_X86_REG_GDTR, (0, scratch + 256, 23, 0))
    cpu.reg_write(x86_const.UC_X86_REG_CS, 8)
    cpu.reg_write(x86_const.UC_X86_REG_SS, 16)
    table = tables['gdtr']
    cpu.reg_write(x86_const.UC_X86_REG_GDTR,
                  tuple(table[k] for k in ('selector', 'base', 'limit', 'flags')))
    cpu.mem_write(scratch, b'\xcf')  # IRET, not original game code.
    cpu.mem_write(scratch + 512, struct.pack('<5I', regs['eip'], regs['cs'],
                                           regs['eflags'], regs['esp'], regs['ss']))
    cpu.reg_write(x86_const.UC_X86_REG_ESP, scratch + 512)
    cpu.emu_start(scratch, regs['eip'], count=1)
    for name in ('ds', 'es', 'fs', 'gs'):
        cpu.reg_write(getattr(x86_const, 'UC_X86_REG_' + name.upper()), regs[name])
    # Descriptor loads may set accessed bits. Restore the captured table memory
    # after populating the caches, so initial RAM/table bytes are not normalized.
    for name in ('gdtr', 'ldtr'):
        cpu.mem_write(tables[name]['base'], tables[name]['bytes'])
    for name in ('eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp'):
        cpu.reg_write(getattr(x86_const, 'UC_X86_REG_' + name.upper()), regs[name])
    differential.bind_x87(cpu, capture.x87)
    cpu.mem_unmap(scratch, 4096)
    for name, expected in regs.items():
        actual = cpu.reg_read(getattr(x86_const, 'UC_X86_REG_' + name.upper()))
        if actual != expected:
            raise ValueError('captured register did not round-trip: ' + name)
    if differential.capture_x87(cpu) != capture.x87:
        raise ValueError('captured raw x87 context did not round-trip')
    return cpu
