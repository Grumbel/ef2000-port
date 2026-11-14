# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Independent native x87 coprocessor for the bounded flat Unicorn oracle.

No translator helpers or SoftFloat. Integer execution stays in Unicorn; known
x87 opcodes run on hardware with raw FSAVE state and relocated memory operands.
Unknown forms, pending delivery and unrecognized hardware context fail closed.
"""

from collections import Counter
import ctypes
from functools import lru_cache
import hashlib
import json
from pathlib import Path
import platform
import resource
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
REGISTERS = ('EAX', 'ECX', 'EDX', 'EBX', 'ESP', 'EBP', 'ESI', 'EDI')
SEGMENTS = ('ES', 'CS', 'SS', 'DS', 'FS', 'GS')
FIELDS = dict(x87_control='FPCW', x87_status='FPSW', x87_tag='FPTAG',
              x87_ip='FIP', x87_cs='FCS', x87_dp='FDP', x87_ds='FDS', x87_opcode='FOP')
PREFIXES = {0x26: 'ES', 0x2e: 'CS', 0x36: 'SS', 0x3e: 'DS', 0x64: 'FS', 0x65: 'GS'}
# (opcode, ModRM group): (operand bytes, may write operand, nonwaiting).
MEMORY = {(0xd9, 0): (4, False, False), (0xd9, 2): (4, True, False),
          (0xd9, 3): (4, True, False), (0xd9, 5): (2, False, False),
          (0xd9, 7): (2, True, True), (0xdd, 0): (8, False, False),
          (0xdd, 2): (8, True, False), (0xdd, 3): (8, True, False),
          (0xdd, 7): (2, True, True), (0xdb, 0): (4, False, False),
          (0xdb, 5): (10, False, False), (0xdb, 7): (10, True, False),
          (0xdf, 0): (2, False, False), (0xdf, 5): (8, False, False)}
MEMORY.update({(0xdf, 2): (2, True, False), (0xdf, 3): (2, True, False),
               (0xdb, 2): (4, True, False), (0xdb, 3): (4, True, False), (0xdf, 7): (8, True, False)})
MEMORY.update({(opcode, group): (width, False, False)
               for opcode, width in ((0xd8, 4), (0xdc, 8)) for group in range(8)})


class Unavailable(ValueError):
    """Original execution lacks a validated native observation, never a pass."""


def register_opcodes():
    result = {(0xd9, value) for value in (0xe0, 0xe1, 0xe8, 0xee, 0xf6, 0xf7, 0xfc)}
    result |= {(0xdb, 0xe2), (0xdb, 0xe3), (0xdf, 0xe0), (0xd9, 0xe4), (0xde, 0xd9), (0xda, 0xe9)}
    for index in range(8):
        result |= {(0xd9, 0xc0+index), (0xd9, 0xc8+index),
                   (0xdd, 0xc0+index), (0xdd, 0xd0+index), (0xdd, 0xd8+index),
                   (0xd8, 0xd0+index), (0xd8, 0xd8+index), (0xdd, 0xe0+index), (0xdd, 0xe8+index)}
        for opcode in (0xd8, 0xdc, 0xde):
            for group in (0, 1, 4, 5, 6, 7):
                result.add((opcode, 0xc0+group*8+index))
    return result


REGISTER = register_opcodes()


def decode(encoded, registers):
    """Independent 32-bit ModRM/SIB decoder; returns native op and guest operand."""
    encoded = bytes(encoded)
    if encoded == b'\x9b':
        return dict(wait=True, nonwaiting=False)
    if not encoded:
        raise Unavailable('empty x87 instruction')
    offset, segment = 0, None
    if encoded[0] in PREFIXES:
        segment, offset = PREFIXES[encoded[0]], 1
    if len(encoded) < offset+2:
        raise Unavailable('unsupported x87 prefix/length')
    opcode, modrm = encoded[offset:offset+2]
    if opcode not in range(0xd8, 0xe0):
        raise Unavailable('unsupported x87 prefix/opcode')
    mode, group, base = modrm >> 6, (modrm >> 3) & 7, modrm & 7
    operation = (opcode << 8) | modrm
    if mode == 3:
        if offset or len(encoded) != 2 or (opcode, modrm) not in REGISTER:
            raise Unavailable('unsupported x87 register encoding')
        return dict(operation=operation, nonwaiting=operation in (0xdbe2, 0xdbe3, 0xdfe0))
    if (opcode, group) not in MEMORY:
        raise Unavailable('unsupported x87 memory encoding')
    width, writes, nonwaiting = MEMORY[opcode, group]
    cursor, index_value = offset+2, 0
    if base == 4:
        if cursor >= len(encoded):
            raise Unavailable('missing x87 SIB')
        sib = encoded[cursor]
        cursor += 1
        index, base = (sib >> 3) & 7, sib & 7
        if index != 4:
            index_value = registers[REGISTERS[index]] << (sib >> 6)
    no_base = mode == 0 and base == 5
    segment = segment or ('SS' if base in (4, 5) and not no_base else 'DS')
    base_value = 0 if no_base else registers[REGISTERS[base]]
    length = 4 if no_base or mode == 2 else 1 if mode == 1 else 0
    if cursor+length != len(encoded):
        raise Unavailable('unsupported x87 displacement/length')
    displacement = int.from_bytes(encoded[cursor:], 'little', signed=True) if length else 0
    return dict(operation=(opcode << 8) | (group << 3) | 6,
                original_fop=((opcode & 7) << 8) | modrm,
                address=(base_value+index_value+displacement) & 0xffffffff,
                segment=segment, width=width, writes=writes, nonwaiting=nonwaiting)


def source():
    """Emit only a fixed allowlist of generic ISA operations, never guest bytes."""
    operations = sorted({(a << 8) | b for a, b in REGISTER} |
                        {(a << 8) | (b << 3) | 6 for a, b in MEMORY})
    text = [r'''#include <stdint.h>
unsigned ef_x87_host_cs(void) { uint16_t value; __asm__ volatile("mov %%cs,%0" : "=r"(value)); return value; }
unsigned ef_x87_host_ds(void) { uint16_t value; __asm__ volatile("mov %%ds,%0" : "=r"(value)); return value; }
typedef uintptr_t (*operation_fn)(const void *,void *,void *,uint32_t *);
#define OP(NAME,A,B) \
static uintptr_t NAME(const void *input,void *out,void *data,uint32_t *eax) { \
 unsigned char saved[108]; uintptr_t pc; uint32_t ax=*eax; \
 __asm__ volatile("fnsave %[save]\n\tfrstor (%[input])\n\t1: .byte " A "," B "\n\t" \
 "fnsave (%[out])\n\tfrstor %[save]\n\tleaq 1b(%%rip),%[pc]" \
 : [save] "+m"(saved),[pc] "=r"(pc),"+a"(ax) \
 : [input] "r"(input),[out] "r"(out),"S"(data) : "memory"); \
 *eax=ax; return pc; }
''']
    text += ['OP(op_%04x,"0x%x","0x%x")' % (op, op >> 8, op & 255) for op in operations]
    text += ['uintptr_t ef_x87_original(unsigned op,const void *in,void *out,void *data,uint32_t *ax) {',
             'switch(op) {']
    text += ['case 0x%x: return op_%04x(in,out,data,ax);' % (op, op) for op in operations]
    text += ['default: return 0;', '}', '}']
    return '\n'.join(text)+'\n'


def build(output, *, compiler='cc'):
    if platform.machine() != 'x86_64' or platform.system() != 'Linux':
        raise ValueError('native x87 oracle requires Linux x86-64')
    output.mkdir(parents=True, exist_ok=False)
    unit, library = output/'original-x87.c', output/'original-x87.so'
    unit.write_text(source())
    command = [compiler, '-shared', '-fPIC', '-O2', '-std=c99', '-Wall', '-Wextra', '-Werror',
               str(unit), '-o', str(library)]
    result = subprocess.run(command, capture_output=True, text=True)
    (output/'compile.log').write_text(result.stdout+result.stderr)
    result.check_returncode()
    native = Native(library)
    report = dict(model='unicorn-with-native-x87-v1', source_sha256=hashlib.sha256(unit.read_bytes()).hexdigest(),
                  tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                  library_sha256=hashlib.sha256(library.read_bytes()).hexdigest(), command=command,
                  machine=platform.machine(), system=platform.system(),
                  host_selectors=dict(cs=native.host_cs, ds=native.host_ds),
                  comparison_profile=native.comparison_profile,
                  limits='Known x87 ISA only; flat zero-base operands relocated; pending #MF delivery unsupported.')
    (output/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    return native, report


def snapshot(cpu):
    from unicorn import x86_const as x
    values = {field: cpu.reg_read(getattr(x, 'UC_X86_REG_'+name)) for field, name in FIELDS.items()}
    values['x87'] = []
    for i in range(8):
        sig, exp = cpu.reg_read(x.UC_X86_REG_FP0+i)
        values['x87'].append([sig & 0xffffffff, sig >> 32, exp])
    return values


def restore(cpu, values):
    from unicorn import x86_const as x
    for i, (low, high, exponent) in enumerate(values['x87']):
        cpu.reg_write(x.UC_X86_REG_FP0+i, (low | high << 32, exponent))
    for field, name in FIELDS.items():
        cpu.reg_write(getattr(x, 'UC_X86_REG_'+name), values[field])


def pack(values):
    state = bytearray(108)
    for offset, key in ((0, 'x87_control'), (4, 'x87_status'), (8, 'x87_tag')):
        struct.pack_into('<H', state, offset, values[key])
    # Distinct relocation sentinels identify unchanged metadata, independently
    # of guest pointers/selectors and the native operation's actual address.
    struct.pack_into('<IHHIH', state, 12, 0xdecaf001, 0x1234, values['x87_opcode'], 0xdecaf002, 0x5678)
    top = (values['x87_status'] >> 11) & 7
    for logical in range(8):
        low, high, exp = values['x87'][(top+logical) & 7]
        struct.pack_into('<QH', state, 28+logical*10, low | high << 32, exp)
    return state


def detect_comparison_profile(execute):
    """Measure two CPU traits once, never fit a profile to a guest probe result."""
    profile, evidence = 0, []
    for bit, control, initial_cc, expected_cc, left in (
            (1, 0x37e, 0, 0x4500, [1, 0xc0000000, 0x7fff]),
            (2, 0x37d, 0x4000, 0x100, [1, 0, 0])):
        values = {field: 0 for field in FIELDS}
        values.update(x87_control=control, x87_status=initial_cc, x87_tag=0xfff2,
                      x87=[left, [0, 0x80000000, 0x3fff]]+[[0, 0, 0] for _ in range(6)])
        observed = execute(values)
        codes = observed['x87_status'] & 0x4500
        if codes == expected_cc:
            profile |= bit
        elif codes != initial_cc:
            raise Unavailable('unsupported unmasked comparison condition-code profile')
        evidence.append(dict(control=control, initial_cc=initial_cc, computed_cc=expected_cc,
                             observed_status=observed['x87_status']))
    return dict(flags=profile, invalid_updates_cc=bool(profile & 1),
                denormal_updates_cc=bool(profile & 2), measurements=evidence)


@lru_cache(maxsize=1)
def unicorn_comparison_profile():
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UcError
    def probe(values):
        cpu = Uc(UC_ARCH_X86, UC_MODE_32)
        cpu.mem_map(0x1000, 0x1000)
        cpu.mem_write(0x1000, b'\xd8\xd1')
        restore(cpu, values)
        try:
            cpu.emu_start(0x1000, 0x1002, count=1)
        except UcError:
            pass
        return snapshot(cpu)
    return detect_comparison_profile(probe)


class Native:
    def __init__(self, library):
        self.library = ctypes.CDLL(str(library.resolve()))
        self.run = self.library.ef_x87_original
        self.run.argtypes = [ctypes.c_uint]+[ctypes.c_void_p]*3+[ctypes.POINTER(ctypes.c_uint32)]
        self.run.restype = ctypes.c_uint64
        self.host_cs = self.library.ef_x87_host_cs()
        self.host_ds = self.library.ef_x87_host_ds()
        self.comparison_profile = detect_comparison_profile(lambda values: self.execute(0xd8d1, values, b'', 0)[0])

    def execute(self, operation, values, data, eax):
        opcode, modrm = operation >> 8, operation & 255
        memory = MEMORY.get((opcode, (modrm >> 3) & 7)) if modrm < 0xc0 else None
        if ((opcode, modrm) not in REGISTER and
                (memory is None or modrm & 7 != 6 or modrm >> 6 != 0)):
            raise Unavailable('native x87 opcode unavailable')
        if len(data) != (memory[0] if memory else 0):
            raise Unavailable('native operand span mismatch')
        nonwaiting = memory[2] if memory else operation in (0xdbe2, 0xdbe3, 0xdfe0)
        if not nonwaiting and (values['x87_status'] & 0x80 or
                               values['x87_status'] & ~values['x87_control'] & 0x3f):
            raise Unavailable('pending original x87 exception delivery')
        initial = pack(values)
        input_ = (ctypes.c_uint8*108).from_buffer(initial)
        out = (ctypes.c_uint8*108)()
        operand = (ctypes.c_uint8*len(data)).from_buffer_copy(data) if data else (ctypes.c_uint8*1)()
        original_operand = bytes(operand)
        scope = getattr(resource, 'RUSAGE_THREAD', resource.RUSAGE_SELF)
        for attempt in range(100):
            operand[:] = original_operand
            ax = ctypes.c_uint32(eax)
            before = resource.getrusage(scope)
            pc = self.run(operation, input_, out, operand, ctypes.byref(ax))
            after = resource.getrusage(scope)
            if (before.ru_nvcsw, before.ru_nivcsw) == (after.ru_nvcsw, after.ru_nivcsw):
                break
        else:
            raise Unavailable('no uninterrupted native x87 sample')
        if not pc:
            raise Unavailable('native x87 opcode unavailable')
        result = {key: struct.unpack_from('<H', bytes(out), offset)[0] for offset, key in
                  ((0, 'x87_control'), (4, 'x87_status'), (8, 'x87_tag'))}
        ip, cs, fop, dp, ds = struct.unpack_from('<IHHIH', bytes(out), 12)
        result.update(x87_ip=ip, x87_cs=cs, x87_opcode=fop & 0x7ff, x87_dp=dp, x87_ds=ds)
        result['x87'] = [[0, 0, 0] for _ in range(8)]
        top = (result['x87_status'] >> 11) & 7
        for logical in range(8):
            sig, exp = struct.unpack_from('<QH', bytes(out), 28+logical*10)
            result['x87'][(top+logical) & 7] = [sig & 0xffffffff, sig >> 32, exp]
        return result, bytes(operand[:len(data)]), ax.value, pc & 0xffffffff, ctypes.addressof(operand) & 0xffffffff, attempt

    def bind(self, cpu):
        from unicorn import UC_HOOK_CODE
        binding = Binding(self, cpu)
        cpu.hook_add(UC_HOOK_CODE, binding.instruction)
        return binding


class Binding:
    def __init__(self, native, cpu):
        self.native, self.cpu = native, cpu
        self.counts, self.interrupted = Counter(), 0

    def evidence(self):
        return dict(operations=dict(self.counts), interrupted_native_samples=self.interrupted)

    def instruction(self, cpu, address, size, _):
        from unicorn import x86_const as x
        first_byte = cpu.mem_read(address, 1)[0]
        if first_byte not in (*PREFIXES, 0x66, 0x67, 0xf0, 0xf2, 0xf3, 0x9b) and not 0xd8 <= first_byte <= 0xdf:
            return
        encoded = bytes(cpu.mem_read(address, size))
        offset = 0
        while offset < len(encoded) and encoded[offset] in (*PREFIXES, 0x66, 0x67, 0xf0, 0xf2, 0xf3):
            offset += 1
        first = encoded[offset] if offset < len(encoded) else 0
        if not (0xd8 <= first <= 0xdf or first == 0x9b):
            return
        registers = {name: cpu.reg_read(getattr(x, 'UC_X86_REG_'+name)) for name in REGISTERS}
        op = decode(encoded, registers)
        values = snapshot(cpu)
        if not op['nonwaiting'] and (values['x87_status'] & 0x80 or
                                    values['x87_status'] & ~values['x87_control'] & 0x3f):
            raise Unavailable('pending original x87 exception delivery')
        if op.get('wait'):
            self.counts['fwait'] += 1
            cpu.reg_write(x.UC_X86_REG_EIP, address+size)
            return
        data = bytes(cpu.mem_read(op['address'], op['width'])) if 'address' in op else b''
        result, written, ax, native_pc, native_dp, retries = self.native.execute(op['operation'], values, data, registers['EAX'])
        self.interrupted += retries
        if op['operation'] == 0xdbe3:
            if any(result[key] for key in ('x87_ip', 'x87_cs', 'x87_opcode', 'x87_dp', 'x87_ds')):
                raise Unavailable('unexpected native FNINIT environment')
        else:
            if result['x87_ip'] == 0xdecaf001 and result['x87_cs'] == 0x1234:
                result.update(x87_ip=values['x87_ip'], x87_cs=values['x87_cs'])
            elif result['x87_ip'] == native_pc and result['x87_cs'] == self.native.host_cs:
                result.update(x87_ip=address, x87_cs=cpu.reg_read(x.UC_X86_REG_CS))
                if 'address' in op:
                    expected = ((op['operation'] >> 8) & 7) << 8 | (op['operation'] & 255)
                    if result['x87_opcode'] != expected:
                        raise Unavailable('unexpected native memory opcode')
                    result['x87_opcode'] = op['original_fop']
            else:
                raise Unavailable('unexpected native instruction pointer')
            if result['x87_dp'] == 0xdecaf002 and result['x87_ds'] == 0x5678:
                result.update(x87_dp=values['x87_dp'], x87_ds=values['x87_ds'])
            elif 'address' in op and result['x87_dp'] == native_dp and result['x87_ds'] == self.native.host_ds:
                result.update(x87_dp=op['address'], x87_ds=cpu.reg_read(getattr(x, 'UC_X86_REG_'+op['segment'])))
            else:
                raise Unavailable('unexpected native data pointer')
        if op['operation'] == 0xdfe0:
            cpu.reg_write(x.UC_X86_REG_EAX, ax)
        elif ax != registers['EAX']:
            raise Unavailable('unexpected native integer register effect')
        restore(cpu, result)
        if 'address' in op and op['writes']:
            cpu.mem_write(op['address'], written)
        self.counts['0x%04x' % op['operation']] += 1
        cpu.reg_write(x.UC_X86_REG_EIP, address+size)
