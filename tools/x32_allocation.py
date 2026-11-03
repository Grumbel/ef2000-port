# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Portable Python form of the verified X-32 allocation model in runtime/x32.c.

Keep the arithmetic and x86 condition flags aligned with the C implementation;
this module is usable on hosts that cannot execute the Linux harness.
"""

from dataclasses import dataclass

MASK = 0xffffffff


def _parity_even(value):
    return (value & 0xff).bit_count() % 2 == 0


def _dec_flags(before, result, flags):
    flags &= ~0x8d5
    if result & 0x80000000:
        flags |= 0x80
    if result == 0:
        flags |= 0x40
    if (before ^ result ^ 1) & 0x10:
        flags |= 0x10
    if before == 0x80000000:
        flags |= 0x800
    if _parity_even(result):
        flags |= 4
    return flags


def _sub_flags(left, right, difference, flags):
    flags &= ~0x8d5
    if difference & 0x80000000:
        flags |= 0x80
    if difference == 0:
        flags |= 0x40
    if (left ^ right ^ difference) & 0x10:
        flags |= 0x10
    if (left ^ right) & (left ^ difference) & 0x80000000:
        flags |= 0x800
    if _parity_even(difference):
        flags |= 4
    return flags


@dataclass
class Allocation:
    heap_end: int = 0x4df000
    stack_end: int = (0x3fff000 - 0x126a) & 0xfffff000
    minimum_stack: int = 0x4e1000
    heap_ceiling: int = 0x4000000
    free_bytes: int = -1

    def __post_init__(self):
        if self.free_bytes < 0:
            self.free_bytes = self.stack_end - self.minimum_stack

    def reserve_stack(self, sp, size, flags):
        if sp < size:
            return None
        boundary = (sp - size) & 0xfffff000
        if boundary < self.minimum_stack or boundary > self.stack_end:
            return None
        consumed = self.stack_end - boundary
        if consumed > self.free_bytes:
            return None
        self.stack_end = boundary
        self.free_bytes -= consumed
        before = (boundary - self.free_bytes) & MASK
        low_limit = (before - 1) & MASK
        return low_limit, _dec_flags(before, low_limit, flags)

    def grow_heap(self, requested, flags):
        rounded = (requested + 0xfff) & 0xfffff000
        if rounded >= self.heap_ceiling:
            return None
        if rounded > self.heap_end:
            growth = rounded - self.heap_end
            if growth > self.free_bytes:
                return None
            self.free_bytes -= growth
            self.heap_end = rounded
        floor = (self.stack_end - self.free_bytes) & MASK
        difference = (floor - self.minimum_stack) & MASK
        stack_low = max(floor, self.minimum_stack)
        return stack_low, self.heap_end, _sub_flags(floor, self.minimum_stack, difference, flags)
