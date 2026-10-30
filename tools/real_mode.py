#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Bounded 16-bit execution for X-32 real-mode requests from the native harness."""

import os
import json
import select
import signal
import struct
import subprocess
import time

PACKET = struct.Struct('<8I4H2I')
MAGIC = 0x314d5258
MEMORY_START = 0
MEMORY_SIZE = 0x100000

try:
    from .input_devices import HostInput
except ImportError:
    from input_devices import HostInput


class RealMode(HostInput):
    def __init__(self):
        super().__init__()
        from unicorn import Uc, UC_ARCH_X86, UC_MODE_16, UC_HOOK_INSN, UC_HOOK_INTR
        from unicorn.x86_const import UC_X86_INS_IN, UC_X86_INS_OUT
        self.cpu = Uc(UC_ARCH_X86, UC_MODE_16)
        self.cpu.mem_map(0, 0x100000)
        self.cpu.hook_add(UC_HOOK_INSN, self.port_in, None, 1, 0, UC_X86_INS_IN)
        self.cpu.hook_add(UC_HOOK_INSN, self.port_out, None, 1, 0, UC_X86_INS_OUT)
        self.cpu.hook_add(UC_HOOK_INTR, self.interrupt)
        self.io = {}
        self.error = None
        self.mouse_x, self.mouse_y = 320, 100
        self.mouse_motion_x, self.mouse_motion_y = 0, 0
        self.mouse_limits = [0, 639, 0, 199]
        self.mouse_visible = False
        self.mouse_buttons = 0
        self.keys = []
        self.video_mode = 3
        self.video_modes = {0x100: (640, 400), 0x101: (640, 480),
                            0x103: (800, 600), 0x105: (1024, 768)}
        self.video_bank = 0
        self.video_pitch = 640
        self.display_start = (0, 0)
        self.vram = bytearray(0x100000)

    def frame(self, memory):
        if self.video_mode == 0x13:
            return 320, 200, memory[0xa0000:0xafa00]
        if self.video_mode not in self.video_modes:
            return None
        self.capture(memory)
        width, height = self.video_modes[self.video_mode]
        x, y = self.display_start
        start = y * self.video_pitch + x
        indices = b''.join(self.vram[start + row * self.video_pitch:start + row * self.video_pitch + width]
                           for row in range(height))
        return width, height, indices

    def capture(self, memory):
        if self.video_mode in self.video_modes:
            start = self.video_bank * 65536
            self.vram[start:start + 65536] = memory[0xa0000:0xb0000]

    def save_video(self, output):
        memory_file = output / 'dos-memory.bin'
        if not memory_file.exists():
            return
        memory = memory_file.read_bytes()
        self.capture(memory)
        try:
            from .vga import save_indexed
        except ImportError:
            from vga import save_indexed
        frame = self.frame(memory)
        if frame is not None:
            width, height, indices = frame
            (output / 'vram.bin').write_bytes(self.vram)
            save_indexed(indices, memory[0xf1000:0xf1300],
                         width, height, output / 'framebuffer.png')
        (output / 'video.json').write_text(json.dumps({'mode': self.video_mode,
            'dimensions': self.video_modes.get(self.video_mode, (320, 200)),
            'bank': self.video_bank}, indent=2) + '\n')

    def video(self, values, memory):
        function = (values[2] >> 8) & 255
        if function == 0x4f:
            command = values[2] & 255
            memory = bytearray(memory)
            address = (values[9] << 4) + (values[7] & 0xffff)  # ES:DI
            values[2] = (values[2] & 0xffff0000) | 0x004f
            if command == 0:
                if address + 512 > len(memory):
                    raise ValueError('VBE controller-info buffer outside DOS memory')
                info = bytearray(512)
                info[:4] = b'VESA'
                struct.pack_into('<HI', info, 4, 0x0102, 0xf0000200)
                struct.pack_into('<IH', info, 14, 0xf0000300, 16)
                memory[address:address + 512] = info
                oem = b'EF2000 virtual VBE BIOS\0'
                memory[0xf0200:0xf0200 + len(oem)] = oem
                modes = struct.pack('<5H', *self.video_modes, 0xffff)
                memory[0xf0300:0xf0300 + len(modes)] = modes
            elif command == 1:
                mode = values[4] & 0x3fff
                if mode not in self.video_modes:
                    values[2] = (values[2] & 0xffff0000) | 0x014f
                else:
                    if address + 256 > len(memory):
                        raise ValueError('VBE mode-info buffer outside DOS memory')
                    width, height = self.video_modes[mode]
                    info = bytearray(256)
                    struct.pack_into('<HBB4H', info, 0, 0x1b, 7, 0, 64, 64, 0xa000, 0)
                    struct.pack_into('<3H', info, 16, width, width, height)
                    info[22:30] = bytes((8, 16, 1, 8, 1, 4, 64, 0x100000 // (width * height) - 1))
                    memory[address:address + 256] = info
            elif command == 2:
                mode = values[3] & 0x3fff
                if mode not in self.video_modes or values[3] & 0x4000:
                    values[2] = (values[2] & 0xffff0000) | 0x014f
                else:
                    self.video_mode, self.video_bank = mode, 0
                    self.video_pitch = self.video_modes[mode][0]
                    self.display_start = (0, 0)
                    if not values[3] & 0x8000:
                        self.vram[:] = bytes(len(self.vram))
                    memory[0xa0000:0xb0000] = self.vram[:65536]
            elif command == 3:
                values[3] = (values[3] & 0xffff0000) | self.video_mode
            elif command == 5 and values[3] & 255 == 0:
                operation = (values[3] >> 8) & 255
                if operation == 1:
                    values[5] = (values[5] & 0xffff0000) | self.video_bank
                elif operation == 0 and values[5] & 0xffff < 16:
                    self.video_bank = values[5] & 0xffff
                    start = self.video_bank * 65536
                    memory[0xa0000:0xb0000] = self.vram[start:start + 65536]
                else:
                    values[2] = (values[2] & 0xffff0000) | 0x014f
            elif command == 6 and self.video_mode in self.video_modes and (values[3] & 255) in (0, 1):
                width, height = self.video_modes[self.video_mode]
                pitch = ((values[4] & 0xffff) + 7) & ~7 if values[3] & 255 == 0 else self.video_pitch
                if pitch < width or pitch * height > len(self.vram):
                    values[2] = (values[2] & 0xffff0000) | 0x014f
                else:
                    self.video_pitch = pitch
                    values[3] = (values[3] & 0xffff0000) | pitch
                    values[4] = (values[4] & 0xffff0000) | pitch
                    values[5] = (values[5] & 0xffff0000) | (len(self.vram) // pitch)
            elif command == 7 and self.video_mode in self.video_modes:
                operation = values[3] & 0xffff
                width, height = self.video_modes[self.video_mode]
                x, y = values[4] & 0xffff, values[5] & 0xffff
                if operation == 1:
                    values[4] = (values[4] & 0xffff0000) | self.display_start[0]
                    values[5] = (values[5] & 0xffff0000) | self.display_start[1]
                elif operation in (0, 0x80) and x + width <= self.video_pitch and (y + height) * self.video_pitch <= len(self.vram):
                    self.display_start = (x, y)
                else:
                    values[2] = (values[2] & 0xffff0000) | 0x014f
            else:
                values[2] = (values[2] & 0xffff0000) | 0x014f
            memory = bytes(memory)
        elif function == 0xf:
            values[2] = (values[2] & 0xffff0000) | ((40 if self.video_mode == 0x13 else 80) << 8) | self.video_mode
            values[3] &= ~0xff00
        elif function == 0 and values[2] & 0x7f in (3, 0x13):
            self.video_mode = values[2] & 0x7f
            memory = bytearray(memory)
            memory[0x449] = self.video_mode
            struct.pack_into('<H', memory, 0x44a, 40 if self.video_mode == 0x13 else 80)
            if not values[2] & 0x80:
                memory[0xa0000:0xb0000] = bytes(65536)
            memory = bytes(memory)
        else:
            raise ValueError(f'unsupported video BIOS AX={values[2] & 0xffff:#x}')
        return PACKET.pack(*values), memory

    def move_mouse(self, dx, dy):
        # Mickey counters keep moving even when the absolute position is clipped.
        self.mouse_motion_x += dx
        self.mouse_motion_y += dy
        left, right, top, bottom = self.mouse_limits
        self.mouse_x = max(left, min(right, self.mouse_x + dx))
        self.mouse_y = max(top, min(bottom, self.mouse_y + dy))

    def mouse(self, values, memory):
        function = values[2] & 0xffff
        if function == 0:
            values[2] = (values[2] & 0xffff0000) | 0xffff
            values[3] = (values[3] & 0xffff0000) | 2
            self.mouse_visible = False
            width, height = self.video_modes.get(self.video_mode, (640, 200))
            self.mouse_limits = [0, width - 1, 0, height - 1]
            self.mouse_x, self.mouse_y = width // 2, height // 2
            self.mouse_motion_x, self.mouse_motion_y = 0, 0
        elif function in (1, 2):
            self.mouse_visible = function == 1
        elif function == 3:
            values[3] = (values[3] & 0xffff0000) | self.mouse_buttons
            values[4] = (values[4] & 0xffff0000) | self.mouse_x
            values[5] = (values[5] & 0xffff0000) | self.mouse_y
        elif function == 4:
            self.mouse_x, self.mouse_y = values[4] & 0xffff, values[5] & 0xffff
        elif function in (7, 8):
            index = 0 if function == 7 else 2
            self.mouse_limits[index:index + 2] = [values[4] & 0xffff, values[5] & 0xffff]
        elif function >= 0x100:
            # A regular DOSBox-X mouse driver leaves registers unchanged for
            # unrecognized vendor extensions (including mission-start probes).
            pass
        elif function == 0xb:
            values[4] = (values[4] & 0xffff0000) | ((self.mouse_motion_x) & 0xffff)
            values[5] = (values[5] & 0xffff0000) | ((self.mouse_motion_y) & 0xffff)
            self.mouse_motion_x, self.mouse_motion_y = 0, 0
        else:
            raise ValueError(f'unsupported mouse function {function:#x}')
        return PACKET.pack(*values), memory

    def keyboard(self, values, memory):
        function = (values[2] >> 8) & 255
        if function in (1, 0x11):
            if self.keys:
                values[2] = (values[2] & 0xffff0000) | self.keys[0]
                values[-2] &= ~0x40
            else:
                values[-2] |= 0x40
        elif function == 2:
            values[2] = (values[2] & 0xffffff00) | self.shift_flags
        elif function in (0, 0x10) and self.keys:
            values[2] = (values[2] & 0xffff0000) | self.keys.pop(0)
        else:
            raise ValueError(f'unsupported/blocking keyboard function {function:#x}')
        return PACKET.pack(*values), memory

    def port_in(self, cpu, port, size, _):
        # A declared absent Sound Blaster. The original driver must perform its
        # own detection/failure path; other device requests stop explicitly.
        if 0x220 <= port <= 0x22f:
            value = (1 << (size * 8)) - 1
            self.record_io('in', port, size, value)
            return value
        self.error = f'unsupported real-mode IN port {port:#x}, width {size}'
        cpu.emu_stop()
        return 0

    def port_out(self, cpu, port, size, value, _):
        if 0x220 <= port <= 0x22f:
            self.record_io('out', port, size, value)
            return
        self.error = f'unsupported real-mode OUT port {port:#x}, width {size}'
        cpu.emu_stop()

    def record_io(self, operation, port, size, value):
        key = (operation, port, size, value)
        if key not in self.io:
            self.io[key] = {'operation': operation, 'port': port, 'size': size,
                            'value': value, 'count': 0}
        self.io[key]['count'] += 1

    def interrupt(self, cpu, vector, _):
        self.error = f'unsupported nested real-mode INT {vector:#x}'
        cpu.emu_stop()

    def execute(self, packet, vectors, memory):
        from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
            UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_DS, UC_X86_REG_ES,
            UC_X86_REG_FS, UC_X86_REG_GS, UC_X86_REG_EFLAGS, UC_X86_REG_CS, UC_X86_REG_IP,
            UC_X86_REG_SS, UC_X86_REG_SP)
        values = list(PACKET.unpack(packet))
        self.capture(memory)
        self.error = None
        self.io = {}
        if values[0] == MAGIC and values[-1] == 0x100:
            return packet, memory
        if values[0] == MAGIC and values[-1] == 0x33:
            return self.mouse(values, memory)
        if values[0] == MAGIC and values[-1] == 0x10:
            return self.video(values, memory)
        if values[0] == MAGIC and values[-1] == 0x16:
            return self.keyboard(values, memory)
        if values[0] != MAGIC or not values[1]:
            raise ValueError(f'invalid or uninstalled real-mode INT {values[-1]:#x}, AX={values[2]:#x}')
        segment, offset = values[1] >> 16, values[1] & 0xffff
        address = (segment << 4) + offset
        if not 0x10000 <= address < 0x90000:
            raise ValueError(f'real-mode entry outside loaded conventional memory: {segment:04x}:{offset:04x}')
        registers = (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
                     UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_DS, UC_X86_REG_ES,
                     UC_X86_REG_FS, UC_X86_REG_GS, UC_X86_REG_EFLAGS)
        self.cpu.mem_write(MEMORY_START, memory)
        self.cpu.mem_write(0, vectors)
        # Native code can replace/relocate a driver between calls. External
        # mem_write does not invalidate Unicorn's previously translated blocks.
        self.cpu.ctl_remove_cache(0, MEMORY_SIZE)
        self.cpu.mem_write(0xf0000, b'\xf4')
        for register, value in zip(registers, values[2:-1]):
            self.cpu.reg_write(register, value)
        self.cpu.reg_write(UC_X86_REG_SS, 0x9000)
        self.cpu.reg_write(UC_X86_REG_SP, 0xfefa)
        self.cpu.mem_write(0x9fefa, struct.pack('<HHH', 0, 0xf000, values[-2] & 0xffff))
        self.cpu.reg_write(UC_X86_REG_CS, segment)
        self.cpu.reg_write(UC_X86_REG_IP, offset)
        self.error = None
        self.io = {}
        self.cpu.emu_start(address, 0xf0000, timeout=500000, count=1000000)
        if self.error:
            raise ValueError(self.error)
        if (self.cpu.reg_read(UC_X86_REG_CS), self.cpu.reg_read(UC_X86_REG_IP)) != (0xf000, 0):
            raise ValueError('real-mode instruction/time limit reached before interrupt return')
        values[2:-1] = [self.cpu.reg_read(register) for register in registers]
        return PACKET.pack(*values), bytes(self.cpu.mem_read(MEMORY_START, MEMORY_SIZE))


class WindowClosed(Exception):
    pass


def read_exact(fd, size, deadline, poll=None):
    result = bytearray()
    while len(result) < size:
        remaining = deadline - time.monotonic()
        if poll is not None:
            poll()
        if remaining <= 0:
            raise TimeoutError('startup runtime timed out')
        if not select.select([fd], [], [], min(0.05, remaining))[0]:
            continue
        chunk = os.read(fd, size - len(result))
        if not chunk:
            if result:
                raise ValueError('truncated real-mode bridge request')
            return None
        result.extend(chunk)
    return bytes(result)


def write_all(fd, data):
    cursor = memoryview(data)
    while cursor:
        cursor = cursor[os.write(fd, cursor):]


def run(command, output, log, timeout=15, headless=True, controls='original',
        joystick='auto', joystick_axes=None):
    engine = RealMode()
    display = None
    if not headless:
        try:
            from .display import Display
        except ImportError:
            from display import Display
        display = Display(joystick=joystick, joystick_axes=joystick_axes)
        display.poll(engine)
    def poll():
        if display is not None:
            display.poll(engine)
            if display.closed:
                raise WindowClosed()
    if controls == 'auto':
        config = output / 'drive/EF2000/EF2000.CFG'
        if config.exists():
            lines = config.read_bytes().splitlines()
            value = b'1' if engine.joystick_axes else b'0'
            lines = [b'CONTROL=' + value if line.startswith(b'CONTROL=') else line for line in lines]
            config.write_bytes(b'\r\n'.join(lines) + b'\r\n')
    request_read, request_write = os.pipe()
    response_read, response_write = os.pipe()
    requests = []
    process = None
    try:
        process = subprocess.Popen(command + [str(request_write), str(response_read)], cwd=output,
            stdout=log, stderr=subprocess.STDOUT, pass_fds=(request_write, response_read))
        os.close(request_write)
        request_write = -1
        os.close(response_read)
        response_read = -1
        deadline = time.monotonic() + timeout if timeout else float('inf')
        while True:
            packet = read_exact(request_read, PACKET.size, deadline, poll)
            if packet is None:
                break
            vectors = read_exact(request_read, 1024, deadline, poll)
            memory = read_exact(request_read, MEMORY_SIZE, deadline, poll)
            if vectors is None or memory is None:
                raise ValueError('missing real-mode memory snapshot')
            record = {'input': list(PACKET.unpack(packet))}
            requests.append(record)
            try:
                poll()
                response, memory = engine.execute(packet, vectors, memory)
                if len(response) != PACKET.size or len(memory) != MEMORY_SIZE:
                    raise ValueError('real-mode response changed the bridge memory/packet size')
                record['output'] = list(PACKET.unpack(response))
                record['io'] = list(engine.io.values())
                if display is not None:
                    display.update(engine, memory)
                memory = engine.sync_input(memory)
                write_all(response_write, response + memory)
            except WindowClosed:
                raise
            except Exception as error:
                record['error'] = str(error)
                record['io'] = list(engine.io.values())
                (output / 'conventional.bin').write_bytes(memory)
                write_all(response_write, bytes(PACKET.size))
        process.wait(timeout=max(0.01, deadline - time.monotonic()) if timeout else None)
        engine.save_video(output)
        return subprocess.CompletedProcess(command, process.returncode), requests
    except TimeoutError:
        if process is not None:
            process.send_signal(signal.SIGUSR1)
            process.wait(timeout=2)
            engine.save_video(output)
            return subprocess.CompletedProcess(command, process.returncode), requests
        raise
    except WindowClosed:
        process.send_signal(signal.SIGUSR2)
        process.wait(timeout=2)
        engine.save_video(output)
        return subprocess.CompletedProcess(command, process.returncode), requests
    finally:
        if process is not None and process.poll() is None:
            process.kill()
            process.wait()
        for fd in (request_read, request_write, response_read, response_write):
            if fd >= 0:
                os.close(fd)
        if display is not None:
            display.close()
