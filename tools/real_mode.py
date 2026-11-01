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
import sys
import threading
import time
import zipfile

PACKET = struct.Struct('<8I4H2I')
MAGIC = 0x314d5258
MEMORY_START = 0
MEMORY_SIZE = 0x100000

try:
    from .input_devices import HostInput, flight_control_settings
except ImportError:
    from input_devices import HostInput, flight_control_settings


class RealMode(HostInput):
    def __init__(self, sound_hardware='absent', vr='off', cd_cue=None):
        super().__init__()
        if vr not in ('off', 'vfx1'):
            raise ValueError(f'unsupported VR interface {vr!r}')
        self.vr = vr
        try:
            from .cd_audio import CDAudio
        except ImportError:
            from cd_audio import CDAudio
        self.cd_audio = CDAudio(cd_cue) if cd_cue else None
        self.vfx_orientation = (0, 0, 0)
        self.sound_device = None
        if sound_hardware == 'sb16':
            try:
                from .sound_blaster import SoundBlaster16
            except ImportError:
                from sound_blaster import SoundBlaster16
            self.sound_device = SoundBlaster16()
        elif sound_hardware == 'sb16-probe':
            try:
                from .sound_blaster import SB16Probe
            except ImportError:
                from sound_blaster import SB16Probe
            self.sound_device = SB16Probe()
        elif sound_hardware != 'absent':
            raise ValueError(f'unsupported sound hardware {sound_hardware!r}')
        from unicorn import Uc, UC_ARCH_X86, UC_MODE_16, UC_HOOK_INSN, UC_HOOK_INTR
        from unicorn.x86_const import UC_X86_INS_IN, UC_X86_INS_OUT
        self.cpu = Uc(UC_ARCH_X86, UC_MODE_16)
        self.cpu.mem_map(0, 0x100000)
        self.cpu.hook_add(UC_HOOK_INSN, self.port_in, None, 1, 0, UC_X86_INS_IN)
        self.cpu.hook_add(UC_HOOK_INSN, self.port_out, None, 1, 0, UC_X86_INS_OUT)
        self.cpu.hook_add(UC_HOOK_INTR, self.interrupt)
        self.pending_real_irq = None
        self.audio_interrupts = 0
        if sound_hardware == 'sb16':
            from unicorn import UC_HOOK_BLOCK
            self.cpu.hook_add(UC_HOOK_BLOCK, self.audio_block)
        self.io = {}
        self.error = None
        self.failed_memory = None
        self.failure_state = None
        self.mouse_x, self.mouse_y = 320, 100
        self.mouse_motion_x, self.mouse_motion_y = 0, 0
        self.mouse_limits = [0, 639, 0, 199]
        self.mouse_visible = False
        self.mouse_buttons = 0
        self.keys = []
        self.video_mode = 3
        self.game_phase = 'startup'
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
        if self.vr == 'vfx1' and function == 0x607f:
            # Original D9B30h probes AX=607Fh and checks for AX=7F60h.
            values[2] = (values[2] & 0xffff0000) | 0x7f60
        elif self.vr == 'vfx1' and function == 0x6070 and values[3] & 0xffff == 0x0301:
            # D9BA0h requests BX=0301h; orientation returns in BX/CX/DX.
            yaw, pitch, roll = self.vfx_orientation
            for index, angle in ((3, yaw), (4, pitch), (5, roll)):
                values[index] = (values[index] & 0xffff0000) | (angle & 0xffff)
            values[2] = (values[2] & 0xffff0000) | 0x6000
        elif function == 0:
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
        # Default: absent Sound Blaster. The opt-in probe supplies measured
        # discovery registers only; unknown device requests stop explicitly.
        if 0x220 <= port <= 0x22f or (hasattr(self.sound_device, 'handles') and self.sound_device.handles(port)):
            try:
                value = (self.sound_device.read(port, size) if self.sound_device
                         else (1 << (size * 8)) - 1)
            except ValueError as error:
                self.error = str(error)
                cpu.emu_stop()
                return 0
            self.record_io('in', port, size, value)
            return value
        self.error = f'unsupported real-mode IN port {port:#x}, width {size}'
        cpu.emu_stop()
        return 0

    def port_out(self, cpu, port, size, value, _):
        if 0x220 <= port <= 0x22f or (hasattr(self.sound_device, 'handles') and self.sound_device.handles(port)):
            self.record_io('out', port, size, value)
            if self.sound_device:
                try:
                    self.sound_device.write(port, value, size)
                except ValueError as error:
                    self.error = str(error)
                    cpu.emu_stop()
            return
        self.error = f'unsupported real-mode OUT port {port:#x}, width {size}'
        cpu.emu_stop()

    def audio_block(self, cpu, address, size, _):
        from unicorn.x86_const import UC_X86_REG_EFLAGS
        try:
            self.sound_device.advance(time.monotonic(), lambda start, length: bytes(cpu.mem_read(start, length)))
            irq = self.sound_device.pic.next_irq()
            if irq is not None and cpu.reg_read(UC_X86_REG_EFLAGS) & 0x200:
                self.pending_real_irq = irq
                cpu.emu_stop()
        except Exception as error:
            self.error = f'audio hardware: {error}'
            cpu.emu_stop()

    def inject_audio_irq(self):
        from unicorn.x86_const import (UC_X86_REG_CS, UC_X86_REG_IP,
            UC_X86_REG_SS, UC_X86_REG_SP, UC_X86_REG_EFLAGS)
        irq = self.pending_real_irq
        self.pending_real_irq = None
        vector = irq + 8
        offset, segment = struct.unpack('<HH', bytes(self.cpu.mem_read(vector * 4, 4)))
        if not segment:
            raise ValueError(f'uninstalled audio IRQ{irq} vector')
        flags = self.cpu.reg_read(UC_X86_REG_EFLAGS)
        sp = (self.cpu.reg_read(UC_X86_REG_SP) - 6) & 0xffff
        frame = struct.pack('<HHH', self.cpu.reg_read(UC_X86_REG_IP),
                            self.cpu.reg_read(UC_X86_REG_CS), flags & 0xffff)
        self.cpu.mem_write((self.cpu.reg_read(UC_X86_REG_SS) << 4) + sp, frame)
        self.cpu.reg_write(UC_X86_REG_SP, sp)
        self.cpu.reg_write(UC_X86_REG_EFLAGS, flags & ~0x300)
        self.cpu.reg_write(UC_X86_REG_CS, segment)
        self.cpu.reg_write(UC_X86_REG_IP, offset)
        self.sound_device.pic.begin(irq)
        self.audio_interrupts += 1
        self.record_io('irq', vector, 0, (segment << 16) | offset)

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
        self.failed_memory = None
        self.failure_state = None
        self.capture(memory)
        self.error = None
        self.io = {}
        snapshot = values[0] == MAGIC and values[-1] == 0x100
        if snapshot and not hasattr(self.sound_device, 'advance'):
            return packet, memory
        if values[0] == MAGIC and values[-1] == 0x2f and values[2] & 0xff00 == 0x1500:
            if self.cd_audio:
                values, memory = self.cd_audio.interrupt(values, memory)
            elif values[2] & 0xffff == 0x1500:
                values[3] = 0
            return PACKET.pack(*values), memory
        if values[0] == MAGIC and values[-1] == 0x33:
            return self.mouse(values, memory)
        if values[0] == MAGIC and values[-1] == 0x10:
            return self.video(values, memory)
        if values[0] == MAGIC and values[-1] == 0x16:
            return self.keyboard(values, memory)
        if values[0] != MAGIC or (not values[1] and not snapshot):
            raise ValueError(f'invalid or uninstalled real-mode INT {values[-1]:#x}, AX={values[2]:#x}')
        segment, offset = (0xf000, 0) if snapshot else (values[1] >> 16, values[1] & 0xffff)
        address = (segment << 4) + offset
        if not snapshot and not 0x10000 <= address < 0x90000:
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
        deadline = time.monotonic() + 0.5
        if snapshot:
            self.cpu.reg_write(UC_X86_REG_EFLAGS, 0x202)
            self.audio_block(self.cpu, address, 0, None)
        for _ in range(64):
            if self.pending_real_irq is not None:
                self.inject_audio_irq()
            address = (self.cpu.reg_read(UC_X86_REG_CS) << 4) + self.cpu.reg_read(UC_X86_REG_IP)
            if address == 0xf0000 or self.error:
                break
            remaining = int((deadline - time.monotonic()) * 1000000)
            if remaining <= 0:
                break
            self.cpu.emu_start(address, 0xf0000, timeout=remaining, count=1000000)
            if self.pending_real_irq is None:
                break
        if not self.error and (self.cpu.reg_read(UC_X86_REG_CS), self.cpu.reg_read(UC_X86_REG_IP)) != (0xf000, 0):
            self.error = 'real-mode instruction/time limit reached before interrupt return'
        if self.error:
            self.failed_memory = bytes(self.cpu.mem_read(MEMORY_START, MEMORY_SIZE))
            self.failure_state = {
                'cs': self.cpu.reg_read(UC_X86_REG_CS), 'ip': self.cpu.reg_read(UC_X86_REG_IP),
                'ss': self.cpu.reg_read(UC_X86_REG_SS), 'sp': self.cpu.reg_read(UC_X86_REG_SP),
                **{name: self.cpu.reg_read(register) for name, register in
                   zip(('eax', 'ebx', 'ecx', 'edx', 'esi', 'edi'), registers[:6])},
            }
            raise ValueError(self.error)
        if not snapshot:
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
        joystick='auto', joystick_axes=None, sound_hardware='absent', vr='off',
        states=None, load_state=None, fullscreen=False, geometry=(1280, 960),
        aspect_ratio='4:3', smooth_scaling=False, verbose=False, cd_cue=None,
        debugger=False):
    try:
        from .runtime_log import RuntimeLog
    except ImportError:
        from runtime_log import RuntimeLog
    runtime_log = RuntimeLog(output / 'calls.log', verbose)
    engine = RealMode(sound_hardware=sound_hardware, vr=vr, cd_cue=cd_cue)
    display = None
    if not headless:
        try:
            from .display import Display
        except ImportError:
            from display import Display
        display = Display(joystick=joystick, joystick_axes=joystick_axes, states=states,
                          fullscreen=fullscreen, geometry=geometry, aspect_ratio=aspect_ratio,
                          smooth_scaling=smooth_scaling)
        display.verbose = verbose
        display.poll(engine)
    def poll():
        runtime_log.poll()
        if states:
            states.context = dict(runtime_log.context)
        if display is not None:
            display.poll(engine)
            if display.closed:
                raise WindowClosed()
    if controls == 'auto':
        config = output / 'drive/EF2000/EF2000.CFG'
        if config.exists():
            lines = config.read_bytes().splitlines()
            value = b'1' if engine.joystick_axes else b'0'
            lines = flight_control_settings(lines, value == b'1')
            config.write_bytes(b'\r\n'.join(lines) + b'\r\n')
            print(f'input: automatic flight controls={"joystick" if engine.joystick_axes else "keyboard"}', flush=True)
    audio_capture = None
    cd_capture = None
    mixer = None
    cd_events = 0
    request_read = request_write = response_read = response_write = -1
    requests = []
    process = None
    output_thread = None
    try:
        if cd_cue and not headless:
            try:
                from .audio_output import AudioMixer
            except ImportError:
                from audio_output import AudioMixer
            mixer = AudioMixer()
        if sound_hardware == 'sb16':
            try:
                from .audio_output import AudioCapture
            except ImportError:
                from audio_output import AudioCapture
            audio_capture = AudioCapture(output, playback=mixer.source() if mixer else not headless)
        if engine.cd_audio:
            try:
                from .audio_output import AudioCapture
            except ImportError:
                from audio_output import AudioCapture
            cd_output = output / 'cd-audio'
            cd_output.mkdir(exist_ok=True)
            cd_capture = AudioCapture(cd_output, playback=mixer.source() if mixer else not headless)
        request_read, request_write = os.pipe()
        response_read, response_write = os.pipe()
        process = subprocess.Popen(command + [str(request_write), str(response_read)], cwd=output,
            stdout=subprocess.PIPE if debugger else log,
            stderr=subprocess.STDOUT, pass_fds=(request_write, response_read))
        if debugger:
            # Keep the interactive GDB prompt visible while preserving the
            # inferior transcript consumed by report.json and runtime polling.
            def relay_output():
                while True:
                    chunk = os.read(process.stdout.fileno(), 4096)
                    if not chunk:
                        break
                    log.write(chunk.decode('utf-8', errors='replace'))
                    log.flush()
                    sys.stdout.buffer.write(chunk)
                    sys.stdout.buffer.flush()

            output_thread = threading.Thread(target=relay_output, daemon=True)
            output_thread.start()
        os.close(request_write)
        request_write = -1
        os.close(response_read)
        response_read = -1
        deadline = time.monotonic() + timeout if timeout else float('inf')
        loading = None
        saving = None
        last_mode = None
        if load_state:
            states.pending = ('load', load_state)
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
                bridge_kind = PACKET.unpack(packet)[-1]
                if bridge_kind == 0x103:
                    if loading is not None:
                        states.restore_engine(engine, loading)
                        runtime_log.context = dict(states.context)
                        loading = None
                        last_mode = engine.video_mode
                        print(f'game: restored {engine.game_phase} (video mode 0x{engine.video_mode:x})', flush=True)
                    elif saving is not None:
                        try:
                            states.save(engine, saving[1])
                        except (OSError, ValueError, TypeError) as error:
                            print(f'state: save failed: {error}', flush=True)
                            if display:
                                display.menu.status = f'Save failed: {error}'
                                display.menu.open = True
                        saving = None
                    response = packet
                else:
                    response, memory = engine.execute(packet, vectors, memory)
                if engine.video_mode != last_mode:
                    engine.game_phase = ('menu' if engine.video_mode == 0x100 else
                                         'flight' if engine.video_mode == 0x101 or
                                         (engine.video_mode == 0x13 and last_mode == 0x100) else
                                         'loading' if engine.video_mode == 0x13 else 'startup')
                    print(f'game: {engine.game_phase} (video mode 0x{engine.video_mode:x})', flush=True)
                    last_mode = engine.video_mode
                if cd_capture:
                    if len(engine.cd_audio.events) != cd_events:
                        cd_events = len(engine.cd_audio.events)
                        (output / 'cd-audio' / 'requests.json').write_text(json.dumps(engine.cd_audio.events, indent=2) + '\n')
                    block = engine.cd_audio.pcm()
                    if block:
                        cd_capture.push(block)
                if audio_capture:
                    for block in engine.sound_device.completed:
                        audio_capture.push(block)
                    engine.sound_device.completed.clear()
                if mixer:
                    mixer.flush()
                if len(response) != PACKET.size or len(memory) != MEMORY_SIZE:
                    raise ValueError('real-mode response changed the bridge memory/packet size')
                record['output'] = list(PACKET.unpack(response))
                record['io'] = list(engine.io.values())
                if display is not None:
                    display.update(engine, memory)
                # Withhold the bridge response while the overlay is open: the
                # native process is blocked, so guest time and input cannot advance.
                paused_at = time.monotonic()
                while display is not None and display.menu.open:
                    poll()
                    display.update(engine, memory)
                    time.sleep(0.01)
                pause_duration = time.monotonic() - paused_at
                if engine.cd_audio and engine.cd_audio.started is not None:
                    engine.cd_audio.started += pause_duration
                deadline += pause_duration
                if bridge_kind == 0x100 and states and states.pending:
                    action, path = states.pending
                    states.pending = None
                    try:
                        if action == 'load':
                            loading = states.prepare_load(path)
                        else:
                            saving = (action, path)
                        values = list(PACKET.unpack(response))
                        values[-1] = 0x102 if action == 'load' else 0x101
                        response = PACKET.pack(*values)
                    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
                        print(f'state: failed: {error}', flush=True)
                        if display:
                            display.menu.status = str(error)
                            display.menu.open = True
                        elif load_state:
                            raise
                memory = engine.sync_input(memory)
                write_all(response_write, response + memory)
            except WindowClosed:
                raise
            except Exception as error:
                record['error'] = str(error)
                record['io'] = list(engine.io.values())
                if engine.failure_state is not None:
                    record['failure_state'] = engine.failure_state
                (output / 'conventional.bin').write_bytes(
                    engine.failed_memory if engine.failed_memory is not None else memory)
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
        runtime_log.poll(force=True)
        if process is not None and process.poll() is None:
            process.kill()
            process.wait()
        if output_thread is not None:
            output_thread.join(timeout=2)
        for fd in (request_read, request_write, response_read, response_write):
            if fd >= 0:
                os.close(fd)
        if cd_capture is not None:
            cd_capture.close()
        if mixer:
            mixer.close()
        if engine.cd_audio:
            (output / 'cd-audio' / 'requests.json').write_text(json.dumps(engine.cd_audio.events, indent=2) + '\n')
            engine.cd_audio.close()
        if audio_capture is not None:
            audio_capture.close(engine.audio_interrupts)
        if display is not None:
            display.close()
