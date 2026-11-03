#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Compare pinned original EF2000 high-score routines with native C++ bytes.

No original data is committed. Pass the SHA-pinned _EF2000.EXE or a matching
locally extracted payload. The original executes only within a bounded,
isolated 32-bit Unicorn machine; no DOS/UI device or file access is provided.
"""

import argparse
import ctypes
import hashlib
from pathlib import Path
import random
import shutil
import struct
import subprocess
import tempfile

try:
    from .extract_image import extract
except ImportError:
    from extract_image import extract


ROOT = Path(__file__).resolve().parent.parent
IMAGE_SHA256 = '3fd26b189efe289feb9e6ac701d592a5d48d7c1e152527aa00c01611e87e0fdb'
IMAGE_SIZE = 0x195fb0
CURRENT_SCORE = 0x1970a0
DIFFICULTY = 0x121e44
STATE_BYTES = 254
NAME_BYTES = 20
NAME_ADDRESS = 0x510000
STACK_TOP = 0x5ff000
STOP_IP = 0x5fe000
MAX_INSTRUCTIONS = 20000


def load_image(executable=None, payload_path=None):
    if (executable is None) == (payload_path is None):
        raise ValueError('provide exactly one executable or --payload')
    if executable is not None:
        _, images = extract(executable.read_bytes())
        payload = images['payload.bin']
    else:
        payload = payload_path.read_bytes()
    if len(payload) != IMAGE_SIZE or hashlib.sha256(payload).hexdigest() != IMAGE_SHA256:
        raise ValueError('not the supported, SHA-pinned EF2000 payload')
    return payload


def player_name(text, padding=0x77):
    raw = text.encode('ascii')
    if len(raw) >= NAME_BYTES or b'\0' in raw:
        raise ValueError('expected a NUL-terminated ASCII name within 20 bytes')
    return raw + b'\0' + bytes([padding]) * (NAME_BYTES - len(raw) - 1)


def make_state(current, scores, padding=0x77):
    if len(scores) != 10:
        raise ValueError('expected ten guest records')
    records = [struct.pack('<20sBi', player_name(f'Pilot{index}', padding),
                           index % 3 + 1, value)
               for index, value in enumerate(scores)]
    state = struct.pack('<i', current) + b''.join(records)
    assert len(state) == STATE_BYTES
    return state


def scenarios(random_count=40, seed=0xef2000):
    standard = [1000 - index * 100 for index in range(10)]
    cases = [
        ('top', make_state(1200, standard), player_name('Ace'), 1),
        ('middle', make_state(850, standard), player_name('Jane', 0xa7), 2),
        ('equal_middle', make_state(800, standard), player_name('Tie', 0xf0), 3),
        ('equal_last', make_state(100, standard), player_name('Last'), 1),
        ('rejected', make_state(99, standard), player_name('Reject'), 2),
        ('min_wrap', make_state(0, [-2**31] * 10), player_name('Wrap'), 2),
        ('signed', make_state(-150, [-100, -120, -200, -230, -260,
                                    -290, -320, -350, -380, -410]),
         player_name('Negative'), 3),
        ('long_name', make_state(250, standard), player_name('N' * 19), 1),
    ]
    rng = random.Random(seed)
    for index in range(random_count):
        scores = sorted((rng.randint(-10000, 10000) for _ in range(10)), reverse=True)
        current = rng.randint(-12000, 12000)
        padding = rng.randrange(1, 256)
        cases.append((f'random_{index}', make_state(current, scores, padding),
                      player_name(f'Random{index}', rng.randrange(1, 256)),
                      rng.randint(1, 3)))
    return cases


def guest_call(payload, entry, initial, name=None, difficulty=1):
    # Imported lazily: asset-free Python tests can inspect fixtures even if
    # Unicorn is not installed in the host environment.
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
    from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EIP,
                                  UC_X86_REG_ESP, UC_X86_REG_EFLAGS)

    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(0, 0x600000)
    cpu.mem_write(0, payload)
    cpu.mem_write(CURRENT_SCORE, initial)
    cpu.mem_write(STACK_TOP, struct.pack('<I', STOP_IP))
    cpu.reg_write(UC_X86_REG_ESP, STACK_TOP)
    cpu.reg_write(UC_X86_REG_EFLAGS, 0x202)
    if name is not None:
        cpu.mem_write(NAME_ADDRESS, name)
        cpu.mem_write(DIFFICULTY, bytes([difficulty]))
        cpu.reg_write(UC_X86_REG_EAX, NAME_ADDRESS)
    cpu.reg_write(UC_X86_REG_EIP, entry)
    cpu.emu_start(entry, STOP_IP, count=MAX_INSTRUCTIONS)
    ip = cpu.reg_read(UC_X86_REG_EIP)
    if ip != STOP_IP:
        raise RuntimeError(f'0x{entry:x} guest stopped at 0x{ip:x}, not at return sentinel')
    return bytes(cpu.mem_read(CURRENT_SCORE, STATE_BYTES)), cpu.reg_read(UC_X86_REG_EAX)


def guest_defaults(payload, initial, random_values):
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
    from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EIP,
                                  UC_X86_REG_ESP, UC_X86_REG_EFLAGS)

    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(0, 0x600000)
    cpu.mem_write(0, payload)
    cpu.mem_write(CURRENT_SCORE, initial)
    cpu.mem_write(STACK_TOP, struct.pack('<I', STOP_IP))
    cpu.reg_write(UC_X86_REG_ESP, STACK_TOP)
    cpu.reg_write(UC_X86_REG_EFLAGS, 0x202)
    cpu.reg_write(UC_X86_REG_EIP, 0x4050)
    used = 0

    def intercept(uc, address, size, unused):
        nonlocal used
        if address not in (0xa4574, 0xa45e4, 0x9c960):
            return
        # These are *only* nondeterministic time/seed/random helpers. Keep
        # the rest of original 0x4050, including its strcpy calls, executing.
        if address == 0x9c960:
            if used >= len(random_values):
                raise RuntimeError('original default generator used extra RNG values')
            uc.reg_write(UC_X86_REG_EAX, random_values[used])
            used += 1
        elif address == 0xa4574:
            uc.reg_write(UC_X86_REG_EAX, 0x12345678)
        esp = uc.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack('<I', uc.mem_read(esp, 4))[0]
        uc.reg_write(UC_X86_REG_ESP, esp + (8 if address == 0xa45e4 else 4))
        uc.reg_write(UC_X86_REG_EIP, ret)

    cpu.hook_add(UC_HOOK_CODE, intercept)
    cpu.emu_start(0x4050, STOP_IP, count=MAX_INSTRUCTIONS)
    if cpu.reg_read(UC_X86_REG_EIP) != STOP_IP or used != len(random_values):
        raise RuntimeError(f'default generator did not finish: RNG calls {used}, '
                           f'EIP 0x{cpu.reg_read(UC_X86_REG_EIP):x}')
    return bytes(cpu.mem_read(CURRENT_SCORE, STATE_BYTES))


def guest_save(payload, initial, open_ok=True):
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
    from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EIP,
                                  UC_X86_REG_ESP, UC_X86_REG_EFLAGS)

    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(0, 0x600000)
    cpu.mem_write(0, payload)
    cpu.mem_write(CURRENT_SCORE, initial)
    cpu.mem_write(STACK_TOP, struct.pack('<I', STOP_IP))
    cpu.reg_write(UC_X86_REG_ESP, STACK_TOP)
    cpu.reg_write(UC_X86_REG_EFLAGS, 0x202)
    cpu.reg_write(UC_X86_REG_EIP, 0x42a4)
    writes = []
    calls = []

    def intercept(uc, address, size, unused):
        if address not in (0xa7948, 0xa7a1c, 0xa7990):
            return
        esp = uc.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack('<I', uc.mem_read(esp, 4))[0]
        if address == 0xa7948:
            pathname = struct.unpack('<I', uc.mem_read(esp + 4, 4))[0]
            if pathname != 0x17f694:
                raise RuntimeError(f'guest save used unexpected filename at 0x{pathname:x}')
            uc.reg_write(UC_X86_REG_EAX, 100 if open_ok else 0xffffffff)
            argc = 4
            calls.append('open')
        elif address == 0xa7a1c:
            handle, length, pointer = struct.unpack('<III', uc.mem_read(esp + 4, 12))
            if handle != 100 or length not in (4, 250):
                raise RuntimeError(f'guest wrote unexpected file handle/length: {handle}/{length}')
            writes.append(bytes(uc.mem_read(pointer, length)))
            uc.reg_write(UC_X86_REG_EAX, length)
            argc = 12
            calls.append(f'write{length}')
        else:
            handle = struct.unpack('<I', uc.mem_read(esp + 4, 4))[0]
            if handle != 100:
                raise RuntimeError('guest closed an unexpected high-score file handle')
            uc.reg_write(UC_X86_REG_EAX, 0)
            argc = 4
            calls.append('close')
        uc.reg_write(UC_X86_REG_ESP, esp + 4 + argc)
        uc.reg_write(UC_X86_REG_EIP, ret)

    cpu.hook_add(UC_HOOK_CODE, intercept)
    cpu.emu_start(0x42a4, STOP_IP, count=MAX_INSTRUCTIONS)
    if cpu.reg_read(UC_X86_REG_EIP) != STOP_IP:
        raise RuntimeError('original file save did not return')
    expected_calls = ['open', 'write4', 'write250', 'close'] if open_ok else ['open']
    if calls != expected_calls:
        raise RuntimeError(f'original file save call sequence differs: {calls}')
    return (bytes(cpu.mem_read(CURRENT_SCORE, STATE_BYTES)),
            cpu.reg_read(UC_X86_REG_EAX) & 0xff, b''.join(writes))


def guest_load_file(payload, initial, file_bytes, random_values=None):
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
    from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EIP,
                                  UC_X86_REG_ESP, UC_X86_REG_EFLAGS)

    if len(file_bytes) != 254:
        raise ValueError('differential loader needs a complete 254-byte file')
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(0, 0x600000)
    cpu.mem_write(0, payload)
    cpu.mem_write(CURRENT_SCORE, initial)
    cpu.mem_write(STACK_TOP, struct.pack('<I', STOP_IP))
    cpu.reg_write(UC_X86_REG_ESP, STACK_TOP)
    cpu.reg_write(UC_X86_REG_EFLAGS, 0x202)
    cpu.reg_write(UC_X86_REG_EIP, 0x41fc)
    position = 0
    used = 0
    calls = []

    def intercept(uc, address, size, unused):
        nonlocal position, used
        if address == 0x420d:
            # The indirect preflight call needs the DOS runtime, unavailable
            # in this isolated test. Force the successful-file branch only.
            uc.reg_write(UC_X86_REG_EAX, 1)
            uc.reg_write(UC_X86_REG_EIP, 0x4213)
            calls.append('preflight')
            return
        if address not in (0xa7900, 0xa79d4, 0xa7990, 0x40dc,
                           0xa4574, 0xa45e4, 0x9c960):
            return
        esp = uc.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack('<I', uc.mem_read(esp, 4))[0]
        if address in (0xa4574, 0xa45e4, 0x9c960):
            if random_values is None:
                raise RuntimeError('valid file unexpectedly entered default generation')
            if address == 0x9c960:
                if used >= len(random_values):
                    raise RuntimeError('guest fallback used extra RNG values')
                uc.reg_write(UC_X86_REG_EAX, random_values[used])
                used += 1
            elif address == 0xa4574:
                uc.reg_write(UC_X86_REG_EAX, 0x12345678)
            argc = 4 if address == 0xa45e4 else 0
        elif address == 0xa7900:
            filename = struct.unpack('<I', uc.mem_read(esp + 4, 4))[0]
            if filename != 0x17f694:
                raise RuntimeError('guest load used unexpected filename')
            uc.reg_write(UC_X86_REG_EAX, 100)
            argc = 4
            calls.append('open')
        elif address == 0xa79d4:
            handle, length, destination = struct.unpack('<III', uc.mem_read(esp + 4, 12))
            if handle != 100 or length not in (4, 250) or position + length > 254:
                raise RuntimeError('guest load requested unexpected read')
            uc.mem_write(destination, file_bytes[position:position + length])
            position += length
            uc.reg_write(UC_X86_REG_EAX, length)
            argc = 12
            calls.append(f'read{length}')
        elif address == 0xa7990:
            handle = struct.unpack('<I', uc.mem_read(esp + 4, 4))[0]
            if handle != 100:
                raise RuntimeError('guest load closed unexpected handle')
            argc = 4
            uc.reg_write(UC_X86_REG_EAX, 0)
            calls.append('close')
        else:
            argc = 0
            calls.append('refresh')
        uc.reg_write(UC_X86_REG_ESP, esp + 4 + argc)
        uc.reg_write(UC_X86_REG_EIP, ret)

    cpu.hook_add(UC_HOOK_CODE, intercept)
    cpu.emu_start(0x41fc, STOP_IP, count=MAX_INSTRUCTIONS)
    if cpu.reg_read(UC_X86_REG_EIP) != STOP_IP or position != 254:
        raise RuntimeError('guest high-score load did not finish reading the file')
    if calls != ['preflight', 'open', 'read4', 'read250', 'close', 'refresh']:
        raise RuntimeError(f'guest high-score load call sequence differs: {calls}')
    if used != (len(random_values) if random_values is not None else 0):
        raise RuntimeError(f'guest high-score fallback consumed {used} RNG values')
    return bytes(cpu.mem_read(CURRENT_SCORE, STATE_BYTES)), cpu.reg_read(UC_X86_REG_EAX) & 0xff


def build_native(compiler, output):
    subprocess.run([compiler, '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-shared', '-fPIC', str(ROOT / 'runtime/ef2000_highscores.cpp'),
                    str(ROOT / 'tests/highscores_reference_abi.cpp'), '-o', str(output)],
                   check=True, timeout=60)
    library = ctypes.CDLL(str(output))
    insert = library.ef2000_test_highscore_insert
    insert.argtypes = [ctypes.POINTER(ctypes.c_ubyte),
                       ctypes.POINTER(ctypes.c_ubyte), ctypes.c_ubyte]
    insert.restype = ctypes.c_int
    exceeds = library.ef2000_test_highscore_exceeds_last
    exceeds.argtypes = [ctypes.POINTER(ctypes.c_ubyte)]
    exceeds.restype = ctypes.c_int
    defaults = library.ef2000_test_highscore_defaults
    defaults.argtypes = [ctypes.POINTER(ctypes.c_ubyte),
                         ctypes.POINTER(ctypes.c_ubyte),
                         ctypes.POINTER(ctypes.c_int32),
                         ctypes.POINTER(ctypes.c_uint16)]
    defaults.restype = ctypes.c_int
    encode = library.ef2000_test_highscore_encode
    encode.argtypes = [ctypes.POINTER(ctypes.c_ubyte), ctypes.POINTER(ctypes.c_ubyte)]
    encode.restype = None
    decode = library.ef2000_test_highscore_decode
    decode.argtypes = [ctypes.POINTER(ctypes.c_ubyte),
                       ctypes.POINTER(ctypes.c_ubyte), ctypes.c_size_t]
    decode.restype = ctypes.c_int
    return insert, exceeds, defaults, encode, decode


def byte_array(data):
    return (ctypes.c_ubyte * len(data)).from_buffer_copy(data)


def compare_case(payload, insert_native, case):
    title, initial, name, difficulty = case
    guest_data, guest_eax = guest_call(payload, 0x433c, initial, name, difficulty)
    state = byte_array(initial)
    native_result = insert_native(state, byte_array(name), difficulty)
    native_data = bytes(state)
    if guest_data != native_data:
        different = next(i for i, (a, b) in enumerate(zip(guest_data, native_data)) if a != b)
        raise AssertionError(f'{title}: guest/native state differs at table offset '
                             f'{different}: {guest_data[different]:02x} != '
                             f'{native_data[different]:02x}')
    if (guest_eax & 0xffff) != (native_result & 0xffff):
        raise AssertionError(f'{title}: guest AX=0x{guest_eax & 0xffff:04x} '
                             f'native result={native_result}')


def compare_threshold(payload, exceeds_native, initial):
    guest_data, guest_eax = guest_call(payload, 0x432c, initial)
    native_result = exceeds_native(byte_array(initial))
    if guest_data != initial or (guest_eax & 0xff) != native_result:
        raise AssertionError('0x432c: threshold comparison or state differs')


def compare_defaults(payload, defaults_native, initial, random_values):
    guest_data = guest_defaults(payload, initial, random_values)
    name_pool = payload[0x121292:0x121292 + 48 * NAME_BYTES]
    template_scores = [struct.unpack_from('<i', payload, 0x1211ad + index * 25)[0]
                       for index in range(10)]
    state = byte_array(initial)
    sources = (ctypes.c_int32 * 10)(*template_scores)
    random_input = (ctypes.c_uint16 * 20)(*random_values)
    consumed = defaults_native(state, byte_array(name_pool), sources, random_input)
    if consumed != 20:
        raise AssertionError(f'0x4050: native default generation rejected names ({consumed})')
    native_data = bytes(state)
    if guest_data != native_data:
        different = next(i for i, (a, b) in enumerate(zip(guest_data, native_data)) if a != b)
        raise AssertionError(f'0x4050: default state differs at offset {different}: '
                             f'{guest_data[different]:02x} != {native_data[different]:02x}')


def compare_save(payload, encode_native, initial):
    saved_state, al, guest_file = guest_save(payload, initial)
    file_output = (ctypes.c_ubyte * (4 + 250))()
    encode_native(byte_array(initial), file_output)
    if saved_state != initial or al != 1 or guest_file != bytes(file_output):
        raise AssertionError('0x42a4: native file encoding differs from original writes')
    rejected_state, rejected_al, rejected_file = guest_save(payload, initial, open_ok=False)
    if rejected_state != initial or rejected_al != 0 or rejected_file:
        raise AssertionError('0x42a4: failed-open branch differs from original')
    return guest_file


def compare_load_valid(payload, decode_native, initial, file_bytes):
    guest_data, al = guest_load_file(payload, initial, file_bytes)
    native = byte_array(initial)
    status = decode_native(native, byte_array(file_bytes), len(file_bytes))
    if al != 1 or status != 1 or guest_data != bytes(native):
        raise AssertionError('0x41fc: valid-file guest load differs from native decode')


def compare_load_bad_checksum(payload, decode_native, defaults_native, initial,
                              file_bytes, random_values):
    bad_file = bytes([file_bytes[0] ^ 1]) + file_bytes[1:]
    guest_data, al = guest_load_file(payload, initial, bad_file, random_values)
    native = byte_array(initial)
    if decode_native(native, byte_array(bad_file), len(bad_file)) != 0:
        raise AssertionError('corrupted checksum unexpectedly validated natively')
    names = payload[0x121292:0x121292 + 48 * NAME_BYTES]
    source_scores = [struct.unpack_from('<i', payload, 0x1211ad + 25 * i)[0]
                     for i in range(10)]
    consumed = defaults_native(native, byte_array(names),
                               (ctypes.c_int32 * 10)(*source_scores),
                               (ctypes.c_uint16 * 20)(*random_values))
    if al != 1 or consumed != 20 or guest_data != bytes(native):
        raise AssertionError('0x41fc: bad-checksum fallback differs from native defaults')


def run(payload, compiler, random_count=40, seed=0xef2000):
    cases = scenarios(random_count, seed)
    with tempfile.TemporaryDirectory(prefix='ef2000-highscores-differential-') as directory:
        insert_native, exceeds_native, defaults_native, encode_native, decode_native = build_native(
            compiler, Path(directory) / 'libhighscore_reference.so')
        for case in cases:
            compare_case(payload, insert_native, case)
            compare_threshold(payload, exceeds_native, case[1])
        rng = random.Random(seed)
        for count in range(6):
            values = [rng.randrange(32768) for _ in range(20)]
            compare_defaults(payload, defaults_native, cases[count][1], values)
        for case in cases[:6]:
            file_bytes = compare_save(payload, encode_native, case[1])
            compare_load_valid(payload, decode_native, cases[-1][1], file_bytes)
            compare_load_bad_checksum(payload, decode_native, defaults_native,
                                      cases[-1][1], file_bytes,
                                      [rng.randrange(32768) for _ in range(20)])
    return len(cases)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path, nargs='?', help='SHA-pinned _EF2000.EXE')
    parser.add_argument('--payload', type=Path, help='already extracted, SHA-pinned payload.bin')
    parser.add_argument('--compiler', default='g++', help='native C++ compiler (default: g++)')
    parser.add_argument('--random-cases', type=int, default=40)
    parser.add_argument('--seed', type=lambda value: int(value, 0), default=0xef2000)
    args = parser.parse_args(argv)
    if args.random_cases < 0 or shutil.which(args.compiler) is None:
        parser.error('expected nonnegative --random-cases and available --compiler')
    try:
        payload = load_image(args.executable, args.payload)
        count = run(payload, args.compiler, args.random_cases, args.seed)
    except (OSError, ValueError, subprocess.CalledProcessError,
            RuntimeError, AssertionError) as error:
        parser.exit(1, f'highscores_differential: {error}\n')
    print(f'EF2000 high scores: {count} insertions, {count} threshold '
          'comparisons, 6 default-table runs and 6 save/load/fallback cycles matched original '
          'guest state, file bytes and return values')


if __name__ == '__main__':
    main()
