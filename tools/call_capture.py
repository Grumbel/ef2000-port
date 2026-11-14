# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Read original function-entry EFCAP001/002 snapshots from C++ Unicorn.

Snapshots include proprietary initialized code/data; they remain local and
must never be added to git or sent to a binary substitute service.
"""

from dataclasses import dataclass
import hashlib
from pathlib import Path
import struct


MAGIC = b'EFCAP001'
REGISTERS = ('eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp', 'esp',
             'eip', 'eflags', 'ds', 'es', 'ss')
HEADER = struct.Struct('<8s32sIIII13I')
MAGIC2 = b'EFCAP002'
REGISTERS2 = REGISTERS + ('fs', 'gs', 'cs')
HEADER2 = struct.Struct('<8s32sIIII16I')
CONTEXT2 = struct.Struct('<7I32I10I12I')
X87_FIELDS = ('x87_control', 'x87_status', 'x87_tag', 'x87_ip',
              'x87_cs', 'x87_dp', 'x87_ds', 'x87_opcode')
SEGMENTS = ('es', 'cs', 'ss', 'ds', 'fs', 'gs')
MAX_RAM = 0x4000000


@dataclass(frozen=True)
class CapturedCall:
    source_sha256: bytes
    address: int
    sequence: int
    registers: dict
    guest_ram: bytes
    format_version: int = 1
    x87: dict | None = None
    table_context: dict | None = None
    producer_version: int | None = None


def decode(blob, original_payload, expected_address=None):
    if len(blob) < HEADER.size:
        raise ValueError('truncated EFCAP001 header')
    magic = blob[:8]
    if magic not in (MAGIC, MAGIC2):
        raise ValueError('invalid EFCAP001 magic')
    version = 2 if magic == MAGIC2 else 1
    header = HEADER2 if version == 2 else HEADER
    size = header.size + (CONTEXT2.size if version == 2 else 0)
    if len(blob) < size:
        raise ValueError('truncated EFCAP002 context')
    _, source_hash, address, index, memory_size, count, *registers = header.unpack_from(blob)
    if source_hash != hashlib.sha256(original_payload).digest():
        raise ValueError('EFCAP001 belongs to a different original initialized payload')
    if expected_address is not None and expected_address != address:
        raise ValueError('EFCAP001 targets a different function')
    names = REGISTERS2 if version == 2 else REGISTERS
    context = CONTEXT2.unpack_from(blob, header.size) if version == 2 else None
    gdt_size, ldt_size = context[5:7] if context else (0, 0)
    if gdt_size > 65536 or ldt_size > 65536:
        raise ValueError('invalid EFCAP002 table window size')
    ram_offset = size + gdt_size + ldt_size
    if count != len(names) or not memory_size or memory_size > MAX_RAM or \
            memory_size % 0x1000 or len(blob) != ram_offset + memory_size:
        raise ValueError('invalid EFCAP001 RAM size, register count or trailing bytes')
    regs = dict(zip(names, registers))
    if address >= len(original_payload) or regs['eip'] != address or \
            regs['esp'] > memory_size - 4 or regs['esp'] < 0x100:
        raise ValueError('invalid EFCAP001 entry address or call stack')
    x87, tables, producer = None, None, None
    if version == 2:
        values = context
        producer, fp_count, table_count, descriptor_count, valid = values[:5]
        if (fp_count, table_count, descriptor_count) != (32, 10, 6) or valid > 63:
            raise ValueError('invalid EFCAP002 context counts or descriptor mask')
        fp, table_words, descriptors = values[7:39], values[39:49], values[49:]
        if table_words[0] > 0xffff or table_words[5] > 0xffff or any(
                not valid & (1 << i) and any(descriptors[2*i:2*i+2]) for i in range(6)):
            raise ValueError('invalid EFCAP002 table selector or unavailable descriptor')
        if (any(regs[name] > 0xffff for name in SEGMENTS) or
                any(fp[i] > 0xffff for i in (0, 1, 2, 4, 6)) or fp[7] > 0x7ff or
                any(fp[8+3*i+2] > 0xffff for i in range(8))):
            raise ValueError('invalid EFCAP002 selector or raw x87 field width')
        x87 = dict(zip(X87_FIELDS, fp[:8]))
        x87['x87'] = [list(fp[8+3*i:11+3*i]) for i in range(8)]
        tables = {'descriptors': {name: (descriptors[2*i] | descriptors[2*i+1] << 32)
                                  if valid & (1 << i) else None for i, name in enumerate(SEGMENTS)}}
        for i, name in enumerate(('gdtr', 'ldtr')):
            selector, low, high, limit, flags = table_words[5*i:5*i+5]
            tables[name] = dict(selector=selector, base=low | high << 32, limit=limit, flags=flags)
        offset = size
        for name, length in (('gdtr', gdt_size), ('ldtr', ldt_size)):
            if length and length != min(tables[name]['limit']+1, 65536):
                raise ValueError('invalid EFCAP002 table window size')
            tables[name]['bytes'] = blob[offset:offset+length] if length else None
            offset += length
        for i, name in enumerate(SEGMENTS):
            table = tables['ldtr' if regs[name] & 4 else 'gdtr']
            raw, offset = table['bytes'], regs[name] & 0xfff8
            if valid & (1 << i) and offset+7 > table['limit']:
                raise ValueError('invalid EFCAP002 descriptor bounds')
            if raw is not None:
                available = offset+8 <= len(raw)
                descriptor = int.from_bytes(raw[offset:offset+8], 'little') if available else None
                if available != bool(valid & (1 << i)) or descriptor != tables['descriptors'][name]:
                    raise ValueError('inconsistent EFCAP002 table descriptor')
    return CapturedCall(source_hash, address, index, regs, blob[ram_offset:], version, x87, tables, producer)


def load(path, original_payload, expected_address=None):
    path = Path(path)
    if path.stat().st_size > HEADER2.size + CONTEXT2.size + 2*65536 + MAX_RAM:
        raise ValueError('EFCAP001 exceeds 64 MiB guest RAM limit')
    return decode(path.read_bytes(), original_payload, expected_address)
