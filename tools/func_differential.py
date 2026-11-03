#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Isolated original-vs-in-place-patched EF2000 function differential runner.

This is an ABI/RAM-state oracle, not proof of full-game equivalence. It runs
one bounded call in each fresh flat Unicorn guest. Functions depending on
DOS services, custom segment descriptors, interrupt delivery or x87 ST0
need their own platform/stub support before use.
"""

import argparse
import csv
from dataclasses import dataclass
from pathlib import Path
import struct

try:
    from .efpatch import apply_patches, decode as decode_patch
    from .highscores_differential import load_image
    from .call_capture import load as load_capture
except ImportError:
    from efpatch import apply_patches, decode as decode_patch
    from highscores_differential import load_image
    from call_capture import load as load_capture


ROOT = Path(__file__).resolve().parent.parent
GENERAL_REGS = ('eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp', 'esp')
INITIAL_REGS = {
    'eax': 0x10203040, 'ebx': 0x21324354, 'ecx': 0x32435465,
    'edx': 0x43546576, 'esi': 0x54657687, 'edi': 0x65768798,
    'ebp': 0x768798a9,
}


def number(value, label):
    if type(value) is int:
        result = value
    elif isinstance(value, str):
        result = int(value, 0)
    else:
        raise ValueError(f'{label} must be an integer or 0x-prefixed string')
    if not 0 <= result <= 0xffffffff:
        raise ValueError(f'{label} is outside 32-bit range')
    return result


def register_part(part):
    """Return the parent GPR, shift and width of a reviewed ABI operand."""
    raw = part.lower()
    if raw in GENERAL_REGS:
        return raw, 0, 32
    aliases = {'al': ('eax', 0, 8), 'ah': ('eax', 8, 8),
               'bl': ('ebx', 0, 8), 'bh': ('ebx', 8, 8),
               'cl': ('ecx', 0, 8), 'ch': ('ecx', 8, 8),
               'dl': ('edx', 0, 8), 'dh': ('edx', 8, 8),
               'ax': ('eax', 0, 16), 'bx': ('ebx', 0, 16),
               'cx': ('ecx', 0, 16), 'dx': ('edx', 0, 16),
               'si': ('esi', 0, 16), 'di': ('edi', 0, 16),
               'bp': ('ebp', 0, 16)}
    if raw not in aliases:
        raise ValueError(f'unsupported ABI register {part!r} (x87 not yet supported)')
    return aliases[raw]


def get_part(regs, part):
    parent, shift, bits = register_part(part)
    return (regs[parent] >> shift) & ((1 << bits) - 1)


@dataclass(frozen=True)
class Signature:
    address: int
    name: str
    outputs: tuple
    preserved: tuple
    stack_bytes: int


def read_signature(path, address):
    with Path(path).open(newline='') as stream:
        rows = [row for row in csv.DictReader(stream, delimiter='\t')
                if number(row['addr'], 'signature address') == address]
    if len(rows) != 1:
        raise ValueError(f'expected one confirmed signature at 0x{address:x}; got {len(rows)}')
    row = rows[0]
    outputs = tuple(x.split(':', 1)[0] for x in row['outputs'].split(',') if x)
    preserved = tuple(x for x in row['preserved'].split(',') if x)
    for register in outputs + preserved:
        register_part(register)
    stack_bytes = number(row['stack_bytes'], 'stack cleanup')
    if stack_bytes % 4 or stack_bytes > 256:
        raise ValueError('unsupported stack cleanup (expected at most 64 dwords)')
    return Signature(address, row['name'], outputs, preserved, stack_bytes)


def read_function_extent(path, address):
    with Path(path).open(newline='') as stream:
        rows = [row for row in csv.DictReader(stream, delimiter='\t')
                if number(row['addr'], 'function address') == address]
    if len(rows) != 1:
        raise ValueError(f'expected one tracked function at 0x{address:x}')
    row = rows[0]
    if row['sig_status'] != 'confirmed' or row['state'] not in ('compiles', 'spliced', 'verified'):
        raise ValueError('differential execution requires confirmed ABI and compiled function state')
    return number(row['size'], 'function size')


def validate_spec(spec, signature, image_size, default_guest_size=0x4000000):
    if number(spec['address'], 'spec address') != signature.address:
        raise ValueError('test spec and confirmed signature addresses differ')
    guest_size = number(spec.get('guest_size', default_guest_size), 'guest size')
    stack = number(spec.get('stack_top', guest_size - 0x1000), 'stack top')
    sentinel = number(spec.get('return_sentinel', guest_size - 0x2000), 'sentinel')
    scratch = number(spec.get('stack_scratch_bytes', 0x10000), 'scratch size')
    budget = number(spec.get('max_instructions', 20000), 'instruction limit')
    function_size = number(spec['function_size'], 'function size')
    if (guest_size % 0x1000 or guest_size < image_size + 0x3000 or
            stack < scratch + 0x100 or stack + 4 + signature.stack_bytes > guest_size or
            sentinel >= guest_size or sentinel == signature.address or budget == 0 or
            not 0 < function_size <= 0x10000 or
            signature.address + function_size > image_size):
        raise ValueError('invalid guest memory, stack, sentinel or instruction budget')
    cases = spec.get('cases')
    if not isinstance(cases, list) or not cases:
        raise ValueError('spec must contain at least one case')
    validated = []
    for case in cases:
        name = case.get('name', '')
        if not isinstance(name, str) or not name:
            raise ValueError('case needs a name')
        registers = {}
        for key, value in case.get('registers', {}).items():
            if key not in INITIAL_REGS:
                raise ValueError(f'case {name}: only full input GPRs are supported: {key}')
            registers[key] = number(value, key)
        args = [number(item, 'stack argument') for item in case.get('stack_args', [])]
        if len(args) != signature.stack_bytes // 4:
            raise ValueError(f'case {name}: expected {signature.stack_bytes // 4} stack arguments')
        memory = []
        for item in case.get('memory', []):
            address = number(item['address'], 'memory address')
            data = bytes.fromhex(item['hex'])
            if not data or address >= guest_size or len(data) > guest_size - address:
                raise ValueError(f'case {name}: invalid memory initialization range')
            if address < stack + 4 + 4 * len(args) and address + len(data) > stack - scratch:
                raise ValueError(f'case {name}: memory initialization overlaps scratch/call frame')
            memory.append((address, data))
        ranges = sorted((address, address + len(data)) for address, data in memory)
        if any(a[1] > b[0] for a, b in zip(ranges, ranges[1:])):
            raise ValueError(f'case {name}: overlapping memory initialization')
        validated.append((name, registers, args, memory))
    return guest_size, stack, sentinel, scratch, budget, function_size, validated


def merged_ranges(ranges):
    """Combine all runtime-written byte intervals without reading whole RAM."""
    result = []
    for begin, end in sorted(ranges):
        if result and begin <= result[-1][1]:
            result[-1] = (result[-1][0], max(end, result[-1][1]))
        else:
            result.append((begin, end))
    return result


def visible_ranges(writes, stack, scratch):
    """Exclude only dead callee scratch below the caller's original ESP.

    The return address and stack arguments, at/above original ESP, remain
    checked. This exclusion follows §2.2 and does not hide guest globals.
    """
    floor = stack - scratch
    visible = []
    for begin, end in writes:
        if begin < floor:
            visible.append((begin, min(end, floor)))
        if end > stack:
            visible.append((max(begin, stack), end))
    return merged_ranges((a, b) for a, b in visible if a < b)


def execute(image, signature, scenario, memory_size, stack, sentinel, budget,
            function_size):
    # Deferred so pure unit tests do not require Python Unicorn installed.
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_MEM_WRITE, UC_HOOK_BLOCK
    from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_EFLAGS
    from unicorn import x86_const

    name, inputs, args, memory = scenario
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(0, memory_size)
    cpu.mem_write(0, image)
    for address, data in memory:
        cpu.mem_write(address, data)
    cpu.mem_write(stack, struct.pack('<' + 'I' * (len(args) + 1), sentinel, *args))
    registers = dict(INITIAL_REGS, **{key: value for key, value in inputs.items()
                                      if key in INITIAL_REGS})
    registers['esp'] = stack
    for reg, value in registers.items():
        cpu.reg_write(getattr(x86_const, 'UC_X86_REG_' + reg.upper()), value)
    cpu.reg_write(UC_X86_REG_EFLAGS, inputs.get('eflags', 0x202))

    writes = []
    blocks = set()

    def write_hook(uc, access, address, size, value, unused):
        if size <= 0 or address + size > memory_size:
            raise RuntimeError(f'{name}: out-of-model guest write 0x{address:x}+{size}')
        writes.append((address, address + size))
        if len(writes) > 100000:
            raise RuntimeError(f'{name}: exceeded 100,000 observed memory writes')

    def block_hook(uc, address, size, unused):
        if signature.address <= address < signature.address + function_size:
            blocks.add(address)

    cpu.hook_add(UC_HOOK_MEM_WRITE, write_hook)
    cpu.hook_add(UC_HOOK_BLOCK, block_hook)
    try:
        cpu.emu_start(signature.address, sentinel, count=budget)
    except Exception as error:
        ip = cpu.reg_read(UC_X86_REG_EIP)
        raise RuntimeError(f'{name}: guest fault at 0x{ip:x}: {error}') from error
    ip = cpu.reg_read(UC_X86_REG_EIP)
    if ip != sentinel:
        raise RuntimeError(f'{name}: instruction budget {budget} exceeded (ip=0x{ip:x})')
    regs = {reg: cpu.reg_read(getattr(x86_const, 'UC_X86_REG_' + reg.upper()))
            for reg in GENERAL_REGS}
    regs['eflags'] = cpu.reg_read(UC_X86_REG_EFLAGS)
    return cpu, regs, writes, blocks


def compare_case(original, patched, signature, scenario, guest_size, stack,
                 sentinel, scratch, budget, function_size, flags_mask=0,
                 patched_scenario=None):
    name = scenario[0]
    left, left_regs, left_writes, blocks = execute(
        original, signature, scenario, guest_size, stack, sentinel, budget, function_size)
    right, right_regs, right_writes, _ = execute(
        patched, signature, patched_scenario or scenario, guest_size, stack,
        sentinel, budget, function_size)
    diffs = []
    expected_sp = stack + 4 + signature.stack_bytes
    if left_regs['esp'] != expected_sp or right_regs['esp'] != expected_sp:
        diffs.append(f'ESP cleanup expected 0x{expected_sp:x}, got '
                     f'0x{left_regs["esp"]:x}/0x{right_regs["esp"]:x}')
    for part in dict.fromkeys(signature.outputs + signature.preserved):
        a, b = get_part(left_regs, part), get_part(right_regs, part)
        if a != b:
            diffs.append(f'{part}: original 0x{a:x}, patched 0x{b:x}')
    if flags_mask and ((left_regs['eflags'] ^ right_regs['eflags']) & flags_mask):
        diffs.append(f'EFLAGS differ under mask 0x{flags_mask:x}')
    outside_stack = visible_ranges(merged_ranges(left_writes + right_writes), stack, scratch)
    for begin, end in outside_stack:
        a = bytes(left.mem_read(begin, end - begin))
        b = bytes(right.mem_read(begin, end - begin))
        if a != b:
            difference = next(i for i, pair in enumerate(zip(a, b)) if pair[0] != pair[1])
            diffs.append(f'guest RAM 0x{begin + difference:x}: '
                         f'original 0x{a[difference]:02x}, patched 0x{b[difference]:02x}')
            break
    return diffs, blocks, sum(b - a for a, b in outside_stack)


def scenarios_from_capture(captured, patch_writes):
    """Construct equal live states, changing only intended patch bytes.

    Never copy the replacement's whole initialized image over live guest
    RAM: that would wipe original game data and BSS modifications.
    """
    regs = captured.registers
    if any(regs[name] != 7 for name in ('ds', 'es', 'ss')):
        raise ValueError('captured function requires non-flat DS/ES/SS selector handling')
    label = f'captured_{captured.sequence}'
    inputs = {key: regs[key] for key in INITIAL_REGS}
    inputs['eflags'] = regs['eflags']
    left = (label, inputs, [], [(0, captured.guest_ram)])
    right = (label, inputs, [], [(0, captured.guest_ram), *patch_writes])
    return left, right


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('address', help='entry address, e.g. 0x432c')
    payload = parser.add_mutually_exclusive_group(required=True)
    payload.add_argument('--executable', type=Path, help='SHA-pinned _EF2000.EXE')
    payload.add_argument('--payload', type=Path, help='SHA-pinned extracted payload.bin')
    parser.add_argument('--patch', type=Path, action='append', required=True,
                        help='in-place EFPATCH1 replacement (repeatable)')
    parser.add_argument('--spec', type=Path, help='defaults to decomp/tests/0xADDRESS.json')
    parser.add_argument('--captured', type=Path, action='append', default=[],
                        help='EFCAP001 original call snapshot; repeatable, replaces spec cases')
    parser.add_argument('--max-instructions', type=int, default=20000,
                        help='per captured function call (default: 20000)')
    parser.add_argument('--signatures', type=Path, default=ROOT / 'decomp/symbols/signatures.tsv')
    parser.add_argument('--functions', type=Path, default=ROOT / 'decomp/symbols/functions.tsv')
    args = parser.parse_args()
    try:
        address = number(args.address, 'address')
        signature = read_signature(args.signatures, address)
        tracked_size = read_function_extent(args.functions, address)
        if args.captured and args.spec:
            raise ValueError('use either --captured inputs or an explicit --spec')
        original = load_image(executable=args.executable, payload_path=args.payload)
        patched, hashes = apply_patches(original, args.patch)
        if args.captured:
            if args.max_instructions <= 0:
                raise ValueError('captured-call instruction budget must be positive')
            patch_writes = []
            for path in args.patch:
                patch_writes.extend(decode_patch(path.read_bytes(), original))
            cases = []
            for path in args.captured:
                captured = load_capture(path, original, address)
                if len(captured.guest_ram) != 0x4000000:
                    raise ValueError('captured game calls require the complete 64 MiB guest RAM')
                stack = captured.registers['esp']
                if stack < 0x10100 or stack + 4 + signature.stack_bytes > 0x4000000:
                    raise ValueError('captured stack outside replayable guest range')
                left, right = scenarios_from_capture(captured, patch_writes)
                cases.append((left, right, stack))
            flags_mask, function_size = 0, tracked_size
        else:
            spec_path = args.spec or ROOT / 'decomp/tests' / f'0x{address:07x}.json'
            import json
            spec = json.loads(spec_path.read_text())
            guest_size, stack, sentinel, scratch, budget, function_size, generated = validate_spec(
                spec, signature, len(original))
            if function_size != tracked_size:
                raise ValueError('spec function extent differs from reviewed functions.tsv boundary')
            flags_mask = number(spec.get('eflags_mask', 0), 'eflags mask')
            cases = [(case, None, stack) for case in generated]
        failures = 0
        observed = set()
        for scenario, replacement, case_stack in cases:
            if args.captured:
                guest_size, sentinel, scratch, budget = (0x4000000, 0x3ffe000,
                                                        0x10000, args.max_instructions)
            diffs, blocks, written = compare_case(original, patched, signature, scenario,
                                                 guest_size, case_stack, sentinel, scratch,
                                                 budget, function_size, flags_mask,
                                                 patched_scenario=replacement)
            print(f'{scenario[0]}: {"FAIL" if diffs else "PASS"} '
                  f'original_blocks={len(blocks)} compared_ram_bytes={written}')
            for diff in diffs:
                print(f'  {diff}')
            failures += bool(diffs)
            observed.update(blocks)
        print(f'function 0x{address:x} ({signature.name}): '
              f'{len(cases) - failures}/{len(cases)} cases, {len(hashes)} patch(es); '
              f'{len(observed)} unique observed original blocks across cases '
              '(NOT a static coverage percentage)')
        return 1 if failures else 0
    except (OSError, ValueError, RuntimeError, ImportError) as error:
        parser.error(str(error))


if __name__ == '__main__':
    raise SystemExit(main())
