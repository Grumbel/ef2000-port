# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Read original function-entry EFCAP001 snapshots from C++ Unicorn.

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
MAX_RAM = 0x4000000


@dataclass(frozen=True)
class CapturedCall:
    source_sha256: bytes
    address: int
    sequence: int
    registers: dict
    guest_ram: bytes


def decode(blob, original_payload, expected_address=None):
    if len(blob) < HEADER.size:
        raise ValueError('truncated EFCAP001 header')
    magic, source_hash, address, index, memory_size, count, *registers = HEADER.unpack_from(blob)
    if magic != MAGIC:
        raise ValueError('invalid EFCAP001 magic')
    if source_hash != hashlib.sha256(original_payload).digest():
        raise ValueError('EFCAP001 belongs to a different original initialized payload')
    if expected_address is not None and expected_address != address:
        raise ValueError('EFCAP001 targets a different function')
    if count != len(REGISTERS) or not memory_size or memory_size > MAX_RAM or \
            memory_size % 0x1000 or len(blob) != HEADER.size + memory_size:
        raise ValueError('invalid EFCAP001 RAM size, register count or trailing bytes')
    regs = dict(zip(REGISTERS, registers))
    if address >= len(original_payload) or regs['eip'] != address or \
            regs['esp'] > memory_size - 4 or regs['esp'] < 0x100:
        raise ValueError('invalid EFCAP001 entry address or call stack')
    return CapturedCall(source_hash, address, index, regs, blob[HEADER.size:])


def load(path, original_payload, expected_address=None):
    path = Path(path)
    if path.stat().st_size > HEADER.size + MAX_RAM:
        raise ValueError('EFCAP001 exceeds 64 MiB guest RAM limit')
    return decode(path.read_bytes(), original_payload, expected_address)
