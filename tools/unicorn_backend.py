#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Experimental portable execution of the original X-32 protected-mode image.

The probe can reach the original menu and flight, but still lacks playable
performance and full device/flight-control validation. The X-32 requests are
modeled here to compare the original instruction stream against the native
harness while porting its services.
The CPU now enters ring 3 with original X-32 selectors through an IRETD
trampoline and a synthetic LDT. The image base remains zero rather than the
Linux native backend's 0x10000000. Fault recovery, IRQ delivery, and complete
port/device emulation are not implemented by this probe.
"""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import sys
import time

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
# The protected image, including its heap and stack, occupies [0, IMAGE_SIZE).
# Keep DOS RAM and the extender's synthetic selectors outside that range.
DOS_BASE = 0x05000000
PSP_BASE = 0x05200000
ENV_BASE = PSP_BASE + 0x1000
EXTENDER_BASE = PSP_BASE + 0x2000


def fletcher16(data):
    """The original 0xFA2E4 checksum: two rolling 8-bit byte sums."""
    low = high = 0
    for byte in data:
        low = (low + byte) & 0xff
        high = (high + low) & 0xff
    return low | (high << 8)


class UnsupportedGuestOperation(RuntimeError):
    """Guest requested an interface not implemented by this probe."""


class KeyboardController:
    """Keyboard ACK handshake and host SDL scan-code queue from DOS RAM."""

    def __init__(self):
        self.ack_pending = False

    def pending(self, cpu):
        if cpu is None:
            return False
        magic, head, tail = struct.unpack('<IHH', bytes(cpu.mem_read(DOS_BASE + 0xf1400, 8)))
        return magic == 0x31504e49 and head < 256 and tail < 256 and head != tail

    def read(self, port, cpu=None):
        if port == 0x64:
            return int(self.ack_pending or self.pending(cpu))
        if port == 0x60:
            if self.ack_pending:
                self.ack_pending = False
                return 0xfa
            if self.pending(cpu):
                head, tail = struct.unpack('<HH', bytes(cpu.mem_read(DOS_BASE + 0xf1404, 4)))
                value = cpu.mem_read(DOS_BASE + 0xf1408 + (head & 255), 1)[0]
                cpu.mem_write(DOS_BASE + 0xf1404, struct.pack('<H', (head + 1) & 255))
                return value
            return 0
        raise UnsupportedGuestOperation(f'unsupported keyboard port {port:04x}')

    def write(self, port, value):
        if port != 0x60:
            raise UnsupportedGuestOperation(f'unsupported keyboard controller command {value:02x}')
        self.ack_pending = True


class ProtectedTimer:
    """Instruction-paced PIT ticks and protected IRQ0 with a real IRET frame.

    Ticks are deterministic for a fixed instruction stream, not a substitute
    for the native backend's wall-clock pacing. Only a registered original
    IRQ0 handler can receive a frame; all other states are left untouched.
    """

    def __init__(self, interval=250000):
        if interval <= 0:
            raise ValueError('timer interval must be positive')
        self.interval = interval
        self.instructions = 0
        self.ticks = 0
        self.interrupts = 0
        self.keyboard_interrupts = 0
        self.divisor = 65536
        self.low = 0
        self.high_next = False
        self.bios_accumulator = 0
        self.in_service = False
        self.eoi_seen = False
        self.return_sp = 0
        self.return_ip = 0

    def port_out(self, port, value):
        value &= 255
        if port == 0x43:
            if value not in (0x36, 0x34):
                raise UnsupportedGuestOperation(f'unsupported PIT command {value:02x}')
            self.high_next = False
        elif port == 0x40:
            if not self.high_next:
                self.low = value
            else:
                self.divisor = self.low | (value << 8) or 65536
            self.high_next = not self.high_next
        elif port == 0x20 and value == 0x20:
            self.eoi_seen = True

    def instruction(self, cpu, address, vectors, regs, keyboard_pending=False):
        if self.in_service and self.eoi_seen and regs['sp'] == self.return_sp:
            self.in_service = self.eoi_seen = False
        if self.instructions % self.interval:
            return False
        self.ticks += 1
        self.bios_accumulator += self.divisor
        if self.bios_accumulator >= 65536:
            address_of_ticks = DOS_BASE + 0x46c
            old = struct.unpack('<I', bytes(cpu.mem_read(address_of_ticks, 4)))[0]
            advances, self.bios_accumulator = divmod(self.bios_accumulator, 65536)
            cpu.mem_write(address_of_ticks, struct.pack('<I', (old + advances) & 0xffffffff))
        irq_vector = 9 if keyboard_pending and vectors.protected[9][0] != IMAGE_SIZE + 0x100 else 8
        handler, selector = vectors.protected[irq_vector]
        if (self.in_service or handler >= IMAGE_SIZE or selector not in (0x2c, 0x2f) or
            regs['cs'] not in (0x2c, 0x2f) or regs['ss'] != 7 or
            not (regs['flags'] & 0x200) or regs['sp'] < 0x1000):
            return False
        cpu.mem_write(regs['sp'] - 12,
                      struct.pack('<3I', address, regs['cs'], regs['flags']))
        cpu.reg_write(regs['sp_register'], regs['sp'] - 12)
        cpu.reg_write(regs['ip_register'], handler)
        cpu.reg_write(regs['flags_register'], regs['flags'] & ~0x300)
        self.in_service = True
        self.eoi_seen = False
        self.return_sp = regs['sp']
        self.return_ip = address
        self.interrupts += 1
        if irq_vector == 9:
            self.keyboard_interrupts += 1
        return True


class VgaDac:
    """Protected-mode VGA DAC writes shared with the DOS framebuffer view."""

    def __init__(self):
        self.cursor = 0

    def write(self, cpu, port, value):
        if port == 0x3c8:
            self.cursor = (value & 255) * 3
        elif port == 0x3c9:
            cpu.mem_write(DOS_BASE + 0xf1000 + self.cursor, bytes((value & 63,)))
            self.cursor = (self.cursor + 1) % 768
        else:
            raise UnsupportedGuestOperation(f'unsupported VGA DAC port {port:04x}')


class VgaStatus:
    """VGA input-status-1 vertical retrace, matching the native harness."""

    def __init__(self, clock=time.monotonic_ns):
        self.clock = clock

    def read(self):
        # The native harness uses a 60 Hz scan period with a 1.67 ms retrace.
        # A constant zero leaves original code's 03DAh polling loop stalled.
        return 8 if self.clock() % 16666667 >= 15000000 else 0


class GamePort:
    """Four discharge counters and active-low buttons from the SDL mailbox."""

    def __init__(self, tick_provider=None):
        self.remaining = [0] * 4
        self.reads = 0
        self.writes = 0
        self.states = {}
        self.tick_provider = tick_provider or (lambda: 0)

    @staticmethod
    def state(cpu):
        raw = bytes(cpu.mem_read(DOS_BASE + 0xf1400, 4))
        if struct.unpack('<I', raw)[0] != 0x31504e49:
            return 0, 0, (0, 0, 0, 0)
        mask, buttons, *axes = struct.unpack('<BB4H', bytes(cpu.mem_read(DOS_BASE + 0xf1600, 10)))
        return mask & 15, buttons & 15, axes

    def write(self, cpu):
        mask, buttons, axes = self.state(cpu)
        self.remaining[:] = axes
        self.writes += 1
        key = (mask, buttons, *axes)
        tick = self.tick_provider()
        count, first, _last = self.states.get(key, (0, tick, tick))
        self.states[key] = (count + 1, first, tick)

    def read(self, cpu):
        self.reads += 1
        mask, buttons, _ = self.state(cpu)
        value = ((~buttons & 15) << 4) | (~mask & 15)
        for axis in range(4):
            if mask & (1 << axis) and self.remaining[axis]:
                value |= 1 << axis
                self.remaining[axis] -= 1
        return value

    def report(self):
        return {'reads': self.reads, 'writes': self.writes,
                'sampled_states': [{'mask': state[0], 'buttons': state[1],
                                    'discharge_counts': list(state[2:]), 'count': record[0],
                                    'first_tick': record[1], 'last_tick': record[2]}
                                   for state, record in sorted(self.states.items())]}


class MouseScript:
    """Reproducible host mouse state transitions for headless driver tests."""

    def __init__(self, entries=()):
        self.events = []
        self.applied = []
        for entry in entries:
            try:
                values = tuple(map(int, entry.split(':'))) if isinstance(entry, str) else tuple(entry)
                if len(values) != 4 or any(not isinstance(value, int) for value in values):
                    raise ValueError()
                tick, x, y, buttons = values
                if tick < 0 or not 0 <= x < 640 or not 0 <= y < 480 or not 0 <= buttons < 8:
                    raise ValueError()
            except (TypeError, ValueError):
                raise ValueError(f'invalid mouse event {entry!r}: expected TICK:X:Y:BUTTONS') from None
            self.events.append((tick, x, y, buttons))
        self.events.sort(key=lambda event: event[0])

    def apply(self, ticks, engine):
        while self.events and self.events[0][0] <= ticks:
            tick, x, y, buttons = self.events.pop(0)
            # The original menu reads relative mickeys (INT 33h AX=000Bh)
            # in addition to absolute position (AX=0003h). Share the SDL
            # movement path so scripts update both representations.
            engine.move_mouse(x - engine.mouse_x, y - engine.mouse_y)
            engine.mouse_buttons = buttons
            self.applied.append({'tick': tick, 'x': x, 'y': y, 'buttons': buttons})


class GuestMouseTrace:
    """Group original EF2000 mouse-call sites by caller and button state."""

    # Protected-image wrappers around INT 33h AX=3 and AX=0Bh.
    ENTRIES = {0xcdfe8: 'position', 0xce02c: 'motion', 0xce064: 'buttons'}

    def __init__(self):
        self.sites = {}
        self.gui_frames = {}
        self.gui_hits = {}
        self.gui_events = {}
        self.gui_dispatches = {}
        self.gui_callbacks = {}
        self.gui_subcontrols = {}
        self.gui_activations = {}
        self.gui_window_commands = {}
        self.gui_keys = {}

    def record(self, entry, caller, buttons, x, y, tick):
        key = (entry, caller, buttons, x, y)
        if key not in self.sites:
            self.sites[key] = {'entry': entry, 'kind': self.ENTRIES[entry],
                               'caller': caller, 'buttons': buttons,
                               'x': x, 'y': y, 'first_tick': tick,
                               'last_tick': tick, 'count': 0}
        site = self.sites[key]
        site['count'] += 1
        site['last_tick'] = tick

    def report(self):
        return sorted(self.sites.values(), key=lambda site: (
            site['entry'], site['caller'], site['buttons'], site['first_tick']))

    @staticmethod
    def _record_group(groups, identity, tick, **fields):
        if identity not in groups:
            groups[identity] = dict(fields, first_tick=tick, last_tick=tick, count=0)
        groups[identity]['last_tick'] = tick
        groups[identity]['count'] += 1

    def record_gui_frame(self, x, y, left_down, tick):
        self._record_group(self.gui_frames, (x, y, left_down), tick,
                           x=x, y=y, left_down=left_down)

    def record_gui_hit(self, control, left_down, tick):
        self._record_group(self.gui_hits, (control, left_down), tick,
                           control=control, left_down=left_down)

    def record_gui_event(self, control_id, control, left_down, tick):
        self._record_group(self.gui_events, (control_id, control, left_down), tick,
                           control_id=control_id, control=control, left_down=left_down)

    def record_gui_dispatch(self, control_id, control, action, tick):
        self._record_group(self.gui_dispatches, (control_id, control, action), tick,
                           control_id=control_id, control=control, action=action)

    def record_gui_callback(self, control, mask, triggered, callback, called, tick):
        self._record_group(self.gui_callbacks,
                           (control, mask, triggered, callback, called), tick,
                           control=control, mask=mask, triggered=triggered,
                           callback=callback, called=called)

    def record_gui_subcontrol(self, stage, control, mask, flags, transitions, callback,
                              open_trigger, open_windows, close_windows, child, behavior, kind,
                              tick):
        self._record_group(self.gui_subcontrols,
                           (stage, control, mask, flags, transitions, callback, open_trigger,
                            open_windows, close_windows, child, behavior, kind), tick,
                           stage=stage, control=control, mask=mask, flags=flags,
                           transitions=transitions, callback=callback,
                           open_trigger=open_trigger, open_windows=list(open_windows),
                           close_windows=list(close_windows), child=child,
                           behavior=behavior, kind=kind)

    def record_gui_activation(self, stage, control, status, flags, disabled, callback,
                              stack_slots, tick):
        self._record_group(self.gui_activations,
                           (stage, control, status, flags, disabled, callback, stack_slots), tick,
                           stage=stage, control=control, status=status,
                           flags=flags, disabled=disabled, callback=callback,
                           stack_slots=list(stack_slots))

    def record_gui_window_command(self, stage, index, control, status, flags, tick):
        self._record_group(self.gui_window_commands,
                           (stage, index, control, status, flags), tick,
                           stage=stage, index=index, control=control,
                           status=status, flags=flags)

    def record_gui_key(self, key, tick):
        self._record_group(self.gui_keys, (key,), tick, key=key)

    def gui_report(self):
        return {'frames': sorted(self.gui_frames.values(), key=lambda row: row['first_tick']),
                'hits': sorted(self.gui_hits.values(), key=lambda row: row['first_tick']),
                'events': sorted(self.gui_events.values(), key=lambda row: row['first_tick']),
                'dispatches': sorted(self.gui_dispatches.values(), key=lambda row: row['first_tick']),
                'callbacks': sorted(self.gui_callbacks.values(), key=lambda row: row['first_tick']),
                'subcontrols': sorted(self.gui_subcontrols.values(), key=lambda row: row['first_tick']),
                'activations': sorted(self.gui_activations.values(), key=lambda row: row['first_tick']),
                'window_commands': sorted(self.gui_window_commands.values(), key=lambda row: row['first_tick']),
                'keys': sorted(self.gui_keys.values(), key=lambda row: row['first_tick'])}


class KeyScript:
    """Reproducible SDL scancodes for BIOS and protected-mode keyboard probes."""

    def __init__(self, entries=()):
        self.events = []
        self.applied = []
        for entry in entries:
            try:
                values = tuple(map(int, entry.split(':'))) if isinstance(entry, str) else tuple(entry)
                if len(values) != 4 or any(not isinstance(value, int) for value in values):
                    raise ValueError()
                tick, scan, key, pressed = values
                if tick < 0 or not 0 <= scan <= 255 or not 0 <= key <= 255 or pressed not in (0, 1):
                    raise ValueError()
            except (TypeError, ValueError):
                raise ValueError(f'invalid key event {entry!r}: expected TICK:SDL_SCAN:ASCII:DOWN') from None
            self.events.append((tick, scan, key, pressed))
        self.events.sort(key=lambda event: event[0])

    def apply(self, ticks, engine):
        while self.events and self.events[0][0] <= ticks:
            tick, scan, key, pressed = self.events.pop(0)
            engine.key_event(scan, key, bool(pressed))
            self.applied.append({'tick': tick, 'scan': scan, 'ascii': key, 'down': bool(pressed)})


class JoystickScript:
    """Repeatable two- or four-axis signed SDL joystick states at PIT ticks."""

    def __init__(self, entries=()):
        self.events = []
        self.applied = []
        self.axis_count = 0
        for entry in entries:
            try:
                values = tuple(map(int, entry.split(':'))) if isinstance(entry, str) else tuple(entry)
                if len(values) not in (4, 6) or any(not isinstance(value, int) for value in values):
                    raise ValueError()
                tick, *state = values
                axes, buttons = state[:-1], state[-1]
                if (tick < 0 or not 0 <= buttons <= 15 or
                        any(not -32768 <= axis <= 32767 for axis in axes) or
                        (self.axis_count and len(axes) != self.axis_count)):
                    raise ValueError()
            except (TypeError, ValueError):
                raise ValueError(f'invalid joystick event {entry!r}: expected '
                                 'TICK:X:Y:BUTTONS or TICK:X:Y:Z:W:BUTTONS') from None
            self.axis_count = len(axes)
            self.events.append((tick, tuple(axes), buttons))
        self.events.sort(key=lambda event: event[0])

    def apply(self, ticks, engine):
        changed = False
        while self.events and self.events[0][0] <= ticks:
            tick, axes, buttons = self.events.pop(0)
            engine.joystick_axes = list(axes)
            engine.joystick_buttons = buttons
            self.applied.append({'tick': tick, 'axes': list(axes), 'buttons': buttons})
            changed = True
        return changed


class RealModeBridge:
    """Run X-32 AX=2511 through the shared bounded 16-bit Unicorn engine."""

    def __init__(self):
        try:
            from .real_mode import RealMode, PACKET, MAGIC
        except ImportError:
            from real_mode import RealMode, PACKET, MAGIC
        self.engine = RealMode()
        self.packet = PACKET
        self.magic = MAGIC
        self.requests = []

    def execute(self, cpu, regs, real_vectors):
        address = regs['dx']
        if regs['ds'] != 7 or address <= 0x260 or address > IMAGE_SIZE - 18:
            raise UnsupportedGuestOperation('invalid AX=2511 real-mode call frame')
        frame = bytearray(cpu.mem_read(address, 18))
        vector, ds, es, fs, gs, ax, dx = struct.unpack('<5H2I', frame)
        if vector >= 256:
            raise UnsupportedGuestOperation(f'invalid real-mode vector {vector}')
        entry = real_vectors[vector]
        values = self._exchange(cpu, real_vectors, vector, entry, ax, regs['bx'],
                                regs['cx'], dx, regs['si'], regs['di'],
                                ds, es, fs, gs, regs['flags'])
        struct.pack_into('<4H', frame, 2, *values[8:12])
        struct.pack_into('<I', frame, 14, values[5])
        cpu.mem_write(address, bytes(frame))
        return dict(regs, ax=values[2], bx=values[3], cx=values[4], dx=values[5],
                    si=values[6], di=values[7],
                    flags=(regs['flags'] & ~0x8d5) | (values[12] & 0x8d5))

    def bios(self, cpu, regs, real_vectors, vector):
        values = self._exchange(cpu, real_vectors, vector, real_vectors[vector],
                                regs['ax'], regs['bx'], regs['cx'], regs['dx'],
                                regs['si'], regs['di'], regs['ds'], regs['es'],
                                0, 0, regs['flags'])
        return dict(regs, ax=values[2], bx=values[3], cx=values[4], dx=values[5],
                    si=values[6], di=values[7],
                    flags=(regs['flags'] & ~0x8d5) | (values[12] & 0x8d5))

    def _exchange(self, cpu, real_vectors, vector, entry, ax, bx, cx, dx,
                  si, di, ds, es, fs, gs, flags):
        request = self.packet.pack(self.magic, entry, ax, bx, cx, dx, si, di,
                                   ds, es, fs, gs, flags, vector)
        conventional = bytes(cpu.mem_read(DOS_BASE, 0x100000))
        vectors = struct.pack('<256I', *real_vectors)
        record = {'vector': vector, 'entry': entry, 'ax': ax, 'dx': dx}
        self.requests.append(record)
        try:
            response, updated = self.engine.execute(request, vectors, conventional)
        except ValueError as exc:
            record['error'] = str(exc)
            raise UnsupportedGuestOperation(f'real-mode INT {vector:02x}h: {exc}') from exc
        values = self.packet.unpack(response)
        if len(updated) != 0x100000:
            raise UnsupportedGuestOperation('real-mode bridge returned invalid DOS memory size')
        cpu.mem_write(DOS_BASE, updated)
        record.update(result_ax=values[2], result_bx=values[3], result_dx=values[5])
        return values


def startup_memory(image):
    """Return the memory writes performed by the Linux harness before entry."""
    if len(image) > IMAGE_SIZE or len(image) < ENTRY + 2:
        raise ValueError('invalid protected-mode image length')
    return (
        # The X-32 loader presents PSP, command tail and environment via
        # LDT selectors. Without these the game follows a different boot path
        # than the working Linux native harness (notably DID archive lookup).
        (PSP_BASE + 0x80, b'\x04 gui\r'),
        (ENV_BASE, b'DID=.\0quick_flag=1\0\0\x01\0C:\\EF2000\\_EF2000.EXE\0'),
        (0x120cc4, struct.pack('<H', 0x1f)),
        # Both probe modes map DS to base zero, so DOS RAM offsets are positive.
        (0x120cc6, struct.pack('<I', DOS_BASE)),
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
        elif function == 0x2507:
            self.protected[vector] = (regs['dx'], regs['ds'])
            self.real[vector] = regs['bx']
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
            result['bx'] = DOS_BASE + self.end
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


def run(image, instruction_limit=None, game_dir=None,
        cd_dir=None, overlay_dir=None, ring3=True, sound='off',
        timer_instructions=250000, mouse_events=(), key_events=(), joystick_events=(), windowed=False,
        fullscreen=False, geometry=(1280, 960), aspect_ratio='4:3',
        smooth_scaling=False, display_instructions=50000,
        display_factory=None, framebuffer=None, framebuffer_samples=(), sample_instructions=0,
        vga_clock=None, trace_mouse=False, trace_decompress=False,
        progress_instructions=0, fast_execution=False, fast_quantum=1000):
    """Run original-code x86; windowed sessions continue until closed by default."""
    if instruction_limit is None:
        instruction_limit = 0 if windowed else DEFAULT_INSTRUCTION_LIMIT
    if instruction_limit < 0:
        raise ValueError('instruction limit must not be negative (0 means unlimited)')
    if sound not in ('off', 'original'):
        raise ValueError('sound must be off or original')
    if timer_instructions <= 0:
        raise ValueError('timer_instructions must be positive')
    if display_instructions <= 0:
        raise ValueError('display_instructions must be positive')
    if sample_instructions < 0:
        raise ValueError('sample_instructions must not be negative')
    if progress_instructions < 0:
        raise ValueError('progress_instructions must not be negative')
    if fast_quantum <= 0:
        raise ValueError('fast_quantum must be positive')
    if fast_execution and (trace_mouse or trace_decompress):
        raise ValueError('fast execution cannot preserve instruction-level mouse/decompression traces')
    if any(value <= 0 for value in framebuffer_samples):
        raise ValueError('framebuffer sample instructions must be positive')
    if framebuffer_samples and framebuffer is None:
        raise ValueError('framebuffer samples require a framebuffer output path')
    framebuffer = Path(framebuffer) if framebuffer is not None else None
    pending_frames = sorted(set(framebuffer_samples))
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
    cpu.mem_map(DOS_BASE, 0x100000)
    cpu.mem_map(PSP_BASE, 0x3000)
    # Guest selectors reference a synthetic LDT. Establish the descriptor
    # tables explicitly: unlike the Linux backend there is no modify_ldt().
    # Reserve the extender's documented default IRQ trampoline at +0x100.
    # Keep Unicorn's private descriptor tables a page away from guest code.
    table_base = IMAGE_SIZE + 0x1000
    cpu.mem_map(IMAGE_SIZE, 0x2000)
    cpu.mem_write(IMAGE_SIZE + 0x100,
                  bytes.fromhex('50 b0 20 e6 20 fb 58 cf'))  # EOI, STI, IRET
    gdt = bytearray(0x30)
    gdt[8:16] = segment_descriptor(table_base + 0x100, 0xff, 0x82, 0)
    if ring3:
        gdt[16:24] = segment_descriptor(0, 0xffff, 0x9a)
        gdt[24:32] = segment_descriptor(0, 0xffff, 0x92)
    cpu.mem_write(table_base, bytes(gdt))
    ldt = bytearray(0x100)
    selector_bases = (0, PSP_BASE, ENV_BASE, DOS_BASE, EXTENDER_BASE, 0)
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
    joystick_script = JoystickScript(joystick_events)
    files = None
    if game_dir is not None:
        files = DosFiles(game_dir, cd_dir, overlay_dir)
        if sound == 'off' or joystick_script.axis_count:
            try:
                original_config = DosFiles._walk(files.root, ['EF2000.CFG'], must_exist=True)
                lines = original_config.read_bytes().splitlines()
            except (DosFileError, OSError):
                # Keep an explicit DOS missing-file result for absent configs.
                pass
            else:
                if sound == 'off':
                    changes = {b'MUSIC': b'0,127', b'SOUNDFX': b'0,127',
                               b'SPEECH': b'0', b'VRHELMET': b'0'}
                    lines = [line.split(b'=', 1)[0] + b'=' + changes[line.split(b'=', 1)[0]]
                             if line.split(b'=', 1)[0] in changes else line for line in lines]
                if joystick_script.axis_count:
                    try:
                        from .input_devices import flight_control_settings
                    except ImportError:
                        from input_devices import flight_control_settings
                    lines = flight_control_settings(lines, True)
                files.config_data = b'\r\n'.join(lines) + b'\r\n'
    selectors = X32Selectors(cpu, table_base + 0x100)
    keyboard = KeyboardController()
    dac = VgaDac()
    vga_status = VgaStatus(clock=vga_clock or time.monotonic_ns)
    timer = ProtectedTimer(timer_instructions)
    gameport = GamePort(tick_provider=lambda: timer.ticks)
    mouse_script = MouseScript(mouse_events)
    key_script = KeyScript(key_events)
    real_mode = RealModeBridge() if windowed or joystick_script.axis_count else None
    if joystick_script.axis_count:
        real_mode.engine.joystick_axes = [0] * joystick_script.axis_count
        initial = real_mode.engine.sync_input(bytes(cpu.mem_read(DOS_BASE, 0x100000)))
        cpu.mem_write(DOS_BASE + 0xf1400, bytes(initial[0xf1400:0xf1508]))
        cpu.mem_write(DOS_BASE + 0xf1600, bytes(initial[0xf1600:0xf160a]))
    display = None
    if windowed:
        if display_factory is None:
            try:
                from .display import Display
            except ImportError:
                from display import Display
            display_factory = Display
        display = display_factory(fullscreen=fullscreen, geometry=geometry,
                                  aspect_ratio=aspect_ratio, smooth_scaling=smooth_scaling)
        display.poll(real_mode.engine)
    result = {'status': 'instruction_limit', 'events': events, 'console': [],
              'cpu_mode': 'x86-ring3-iopl3' if ring3 else 'x86-ring0-flat',
              'privileged_ops': [], 'video_modes': [], 'ip_samples': [],
              'instruction_limit': instruction_limit}
    result['execution'] = {'mode': 'batched' if fast_execution else 'instruction-hook',
                           'quantum': fast_quantum if fast_execution else 1,
                           'timing_approximate': bool(fast_execution),
                           'irq_hook_installs': 0, 'irq_completions': 0}
    if pending_frames:
        result['framebuffer_samples'] = []
    mouse_trace = GuestMouseTrace() if trace_mouse else None
    decompress_trace = {'checks': 0, 'last_success': None, 'failures': []} if trace_decompress else None

    def interrupt(uc, vector, _user):
        nonlocal real_mode
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
        if (vector == 3 and ip == 0xfa166 and decompress_trace is not None and
                decompress_trace['failures']):
            # The original LZ decoder deliberately executes INT 3 when its
            # decompressed-output checksum disagrees with the block header.
            failed = decompress_trace['failures'][-1]
            result['status'] = 'guest_integrity_failure'
            result['detail'] = (f'guest LZ checksum mismatch: '
                                f'expected={failed["expected"]:04x} '
                                f'actual={failed["actual"]:04x}')
            uc.emu_stop()
            return
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
                if vector not in (0x10, 0x33, 0x16, 0x1a):
                    raise UnsupportedGuestOperation(f'unimplemented INT {vector:02x}h at IP={ip:08x}')
                if real_mode is None:
                    real_mode = RealModeBridge()
                    mouse_script.apply(timer.ticks, real_mode.engine)
                next_regs = real_mode.bios(uc, regs, vectors.real, vector)
            elif (regs['ax'] >> 8) & 0xff == 0x4c:
                result['status'] = 'guest_exit'
                result['exit_code'] = regs['ax'] & 0xff
                uc.emu_stop()
                return
            elif (regs['ax'] & 0xffff) in (0x2502, 0x2503, 0x2504, 0x2505,
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
            elif regs['ax'] & 0xffff == 0x2511:
                if real_mode is None:
                    real_mode = RealModeBridge()
                    mouse_script.apply(timer.ticks, real_mode.engine)
                next_regs = real_mode.execute(uc, regs, vectors.real)
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
        if vector == 0x21 and ((regs['ax'] >> 8) & 0xff) in (0x3c, 0x3d, 0x3e, 0x3f, 0x42):
            event['result_ax'] = next_regs['ax']
            event['carry'] = bool(next_regs['flags'] & 1)
        if real_mode is not None:
            mode = real_mode.engine.video_mode
            if not result['video_modes'] or result['video_modes'][-1]['mode'] != mode:
                result['video_modes'].append({'instruction': timer.instructions, 'mode': mode})

    def save_frame(path):
        if real_mode is None:
            return False
        memory = bytes(cpu.mem_read(DOS_BASE, 0x100000))
        frame = real_mode.engine.frame(memory)
        if frame is None:
            return False
        try:
            from .vga import save_indexed
        except ImportError:
            from vga import save_indexed
        width, height, indices = frame
        save_indexed(indices, memory[0xf1000:0xf1300], width, height, path)
        return True

    def instruction(uc, address, size, _user):
        if not (ENTRY <= address < IMAGE_SIZE or
                IMAGE_SIZE + 0x100 <= address < IMAGE_SIZE + 0x108 or
                ring3 and table_base + 0x200 <= address < table_base + 0x400):
            result['status'] = 'unexpected_execution_address'
            result['detail'] = f'IP={address:08x}'
            uc.emu_stop()
            return
        timer.instructions += 1
        if progress_instructions and timer.instructions % progress_instructions == 0:
            mode = (f'{real_mode.engine.video_mode:#x}' if real_mode is not None
                    else 'none')
            checks = decompress_trace['checks'] if decompress_trace is not None else '-'
            print(f'unicorn: instructions={timer.instructions} ticks={timer.ticks} '
                  f'ip={address:08x} video={mode} checks={checks}',
                  file=sys.stderr, flush=True)
        if pending_frames and timer.instructions == pending_frames[0]:
            point = pending_frames.pop(0)
            path = framebuffer.with_name(f'{framebuffer.stem}-{point:09d}{framebuffer.suffix or ".png"}')
            if save_frame(path):
                result['framebuffer_samples'].append({'instruction': point,
                    'video_mode': real_mode.engine.video_mode, 'path': str(path)})
        if decompress_trace is not None and address == 0xfa153:
            # Protected image 0xFA2E4 returns a two-byte Fletcher checksum in
            # AX. 0xFA153 compares it to the original compressed block header,
            # then the guest itself trips INT 3 at 0xFA167 on a mismatch.
            expected = struct.unpack('<H', uc.mem_read(0x16279e, 2))[0]
            actual = uc.reg_read(UC_X86_REG_EAX) & 0xffff
            length = struct.unpack('<I', uc.mem_read(0x162790, 4))[0]
            output = struct.unpack('<I', uc.mem_read(0x162794, 4))[0]
            frame = uc.reg_read(UC_X86_REG_EBP)
            return_ip, source, destination = struct.unpack('<3I', uc.mem_read(frame + 4, 12))
            row = {'tick': timer.ticks, 'instruction': timer.instructions,
                   'expected': expected, 'actual': actual, 'length': length,
                   'output': output, 'return_ip': return_ip,
                   'source_argument': source, 'destination_argument': destination}
            decompress_trace['checks'] += 1
            if expected == actual:
                decompress_trace['last_success'] = row
            else:
                if 0 <= output < IMAGE_SIZE and 0 < length <= IMAGE_SIZE - output:
                    contents = bytes(uc.mem_read(output, length))
                    row.update(checksum=fletcher16(contents),
                               sha256=hashlib.sha256(contents).hexdigest(),
                               head=contents[:32].hex(), tail=contents[-32:].hex())
                row['recent_filenames'] = [e['filename'] for e in events
                                           if 'filename' in e][-8:]
                decompress_trace['failures'].append(row)
        if mouse_trace is not None and real_mode is not None:
            if address in GuestMouseTrace.ENTRIES:
                caller = struct.unpack('<I', uc.mem_read(uc.reg_read(UC_X86_REG_ESP), 4))[0]
                engine = real_mode.engine
                mouse_trace.record(address, caller, engine.mouse_buttons,
                                   engine.mouse_x, engine.mouse_y, timer.ticks)
            elif address == 0xacc75:
                # Main menu's input event has just copied the software cursor
                # and mouse button into its own event structure.
                x, y = struct.unpack('<2H', uc.mem_read(0x13725e, 4))
                event = uc.reg_read(UC_X86_REG_EBX)
                left_down = bool(uc.mem_read(event + 0x1e, 1)[0] & 1)
                mouse_trace.record_gui_frame(x, y, left_down, timer.ticks)
            elif address == 0xacd80:
                # Main menu's control hit-test accepted a cursor rectangle.
                mouse_trace.record_gui_hit(uc.reg_read(UC_X86_REG_EDX),
                                           bool(real_mode.engine.mouse_buttons & 1), timer.ticks)
            elif address == 0xacfa0:
                # The GUI's completed event carries a control id and pointer.
                event = uc.reg_read(UC_X86_REG_EBX)
                data = bytes(uc.mem_read(event, 0x1f))
                mouse_trace.record_gui_event(struct.unpack_from('<H', data)[0],
                                             struct.unpack_from('<I', data, 4)[0],
                                             bool(data[0x1e] & 1), timer.ticks)
            elif address == 0xacffc:
                # The event dispatcher has selected an action code for this
                # control (0x19 is a left-button-down event).
                event = uc.reg_read(UC_X86_REG_EDX)
                data = bytes(uc.mem_read(event, 8))
                mouse_trace.record_gui_dispatch(struct.unpack_from('<H', data)[0],
                                                struct.unpack_from('<I', data, 4)[0],
                                                uc.reg_read(UC_X86_REG_EAX), timer.ticks)
            elif address == 0xac6b6:
                mouse_trace.record_gui_key(uc.mem_read(0x41f2e6, 1)[0], timer.ticks)
            elif address in (0xa4b80, 0xa4cb4):
                # Scripted GUI window-open/window-close commands. Distinguish
                # them from the chrome callback's focus/drag activation gate.
                index = uc.reg_read(UC_X86_REG_EAX) & 0xffff
                maximum = struct.unpack('<H', uc.mem_read(0x41eed8, 2))[0]
                if 0 < index <= maximum:
                    control = struct.unpack('<I', uc.mem_read(0x41edfc + index * 4, 4))[0]
                    if control:
                        status = uc.mem_read(control + 0xc4, 1)[0]
                        flags = uc.mem_read(control + 0x3b, 1)[0]
                        mouse_trace.record_gui_window_command(address, index, control,
                                                              status, flags, timer.ticks)
            elif address in (0xf1d2f, 0xf1d44):
                # Event masks and dispatch callbacks are evaluated by the
                # original SuperGUI widget code, not by the DOS mouse driver.
                control = (uc.reg_read(UC_X86_REG_EAX) if address == 0xf1d2f
                           else uc.reg_read(UC_X86_REG_EDX))
                mask, triggered, callback = struct.unpack('<HHI', uc.mem_read(control + 0xa9, 8))
                mouse_trace.record_gui_callback(control, mask, triggered, callback,
                                                address == 0xf1d44, timer.ticks)
            elif address in (0xf18a4, 0xf18e2, 0xf18fa, 0xf193f, 0xf1a3f):
                # The main menu's nested widget has separate event-mask and
                # callback gates from the top-level control at 0xF1C58.
                if address == 0xf18a4:
                    event = uc.reg_read(UC_X86_REG_EAX)
                    control = struct.unpack('<I', uc.mem_read(event + 0x10, 4))[0]
                else:
                    control = uc.reg_read(UC_X86_REG_ESI if address in (0xf193f, 0xf1a3f)
                                          else UC_X86_REG_EAX)
                if control:
                    mask, flags = struct.unpack('<HH', uc.mem_read(control + 0x0a, 4))
                    transitions, open_trigger = struct.unpack('<HH', uc.mem_read(control + 0x14, 4))
                    close_windows = struct.unpack('<5H', uc.mem_read(control + 0x59, 10))
                    open_windows = struct.unpack('<5H', uc.mem_read(control + 0x63, 10))
                    child = struct.unpack('<I', uc.mem_read(control + 0x6d, 4))[0]
                    callback = struct.unpack('<I', uc.mem_read(control + 0x71, 4))[0]
                    behavior = uc.mem_read(control + 0x1d, 1)[0]
                    kind = uc.mem_read(control + 0x8b, 1)[0]
                    mouse_trace.record_gui_subcontrol(address, control, mask, flags,
                                                      transitions, callback, open_trigger,
                                                      open_windows, close_windows, child,
                                                      behavior, kind, timer.ticks)
            elif address in (0xe1d14, 0xe1d53, 0xe1d95, 0xa4b08,
                             0xa4b39, 0xa4b5c, 0xa4b79):
                # Title-bar widget 0xE1D14 gates window focus/drag operations
                # through its parent status; it is not the menu-icon action.
                if address == 0xe1d14:
                    subcontrol = uc.reg_read(UC_X86_REG_EAX)
                    index = uc.mem_read(subcontrol + 0x50, 1)[0]
                    control = struct.unpack('<I', uc.mem_read(0x41edfc + index * 4, 4))[0]
                else:
                    control = uc.reg_read(UC_X86_REG_EDX if address in (0xa4b39, 0xa4b79)
                                          else UC_X86_REG_EAX)
                if control:
                    status = uc.mem_read(control + 0xc4, 1)[0]
                    flags = uc.mem_read(control + 0x3b, 1)[0]
                    disabled = uc.mem_read(control + 0x11, 1)[0]
                    callback = struct.unpack('<I', uc.mem_read(control + 0xbd, 4))[0]
                    stack_slots = struct.unpack('<HH', uc.mem_read(0x41eda8, 4))
                    mouse_trace.record_gui_activation(address, control, status, flags,
                                                      disabled, callback, stack_slots,
                                                      timer.ticks)
        if sample_instructions and timer.instructions % sample_instructions == sample_instructions // 2:
            result['ip_samples'].append({'instruction': timer.instructions, 'ip': address,
                                         'ax': uc.reg_read(UC_X86_REG_EAX),
                                         'sp': uc.reg_read(UC_X86_REG_ESP),
                                         'tick': timer.ticks})
        if display is not None and timer.instructions % display_instructions == 0:
            snapshot = bytes(uc.mem_read(DOS_BASE, 0x100000))
            try:
                display.update(real_mode.engine, snapshot)
            except (ValueError, OSError) as exc:
                result['status'] = 'host_display_error'
                result['detail'] = str(exc)
                uc.emu_stop()
                return
            if display.closed:
                result['status'] = 'window_closed'
                uc.emu_stop()
                return
            while display.menu.open and not display.closed:
                try:
                    display.update(real_mode.engine, snapshot)
                except (ValueError, OSError) as exc:
                    result['status'] = 'host_display_error'
                    result['detail'] = str(exc)
                    uc.emu_stop()
                    return
                time.sleep(0.01)
            if display.closed:
                result['status'] = 'window_closed'
                uc.emu_stop()
                return
            snapshot = real_mode.engine.sync_input(snapshot)
            uc.mem_write(DOS_BASE + 0xf1400, bytes(snapshot[0xf1400:0xf1508]))
            uc.mem_write(DOS_BASE + 0xf1600, bytes(snapshot[0xf1600:0xf160a]))
        if (timer.instructions % timer.interval == 0 or
            (timer.in_service and timer.eoi_seen)):
            previous_ticks = timer.ticks
            timer.instruction(uc, address, vectors, {
                'sp': uc.reg_read(UC_X86_REG_ESP), 'sp_register': UC_X86_REG_ESP,
                'ip_register': UC_X86_REG_EIP, 'flags_register': UC_X86_REG_EFLAGS,
                'cs': uc.reg_read(UC_X86_REG_CS), 'ss': uc.reg_read(UC_X86_REG_SS),
                'flags': uc.reg_read(UC_X86_REG_EFLAGS),
            }, keyboard_pending=keyboard.pending(uc))
            if real_mode is not None and timer.ticks != previous_ticks:
                mouse_script.apply(timer.ticks, real_mode.engine)
                key_script.apply(timer.ticks, real_mode.engine)
                joystick_changed = joystick_script.apply(timer.ticks, real_mode.engine)
                if real_mode.engine.scan_queue or joystick_changed:
                    snapshot = real_mode.engine.sync_input(bytes(uc.mem_read(DOS_BASE, 0x100000)))
                    uc.mem_write(DOS_BASE + 0xf1400, bytes(snapshot[0xf1400:0xf1508]))
                    if joystick_changed:
                        uc.mem_write(DOS_BASE + 0xf1600, bytes(snapshot[0xf1600:0xf160a]))

    eoi_hook = None

    def post_eoi_instruction(uc, _address, _size, _user):
        # The IRQ's IRETD returns to its saved original instruction address.
        # A hook limited to that exact address observes the restored SP without
        # running a Python callback for every instruction in the IRQ handler.
        if (timer.in_service and timer.eoi_seen and
                uc.reg_read(UC_X86_REG_ESP) == timer.return_sp):
            timer.in_service = timer.eoi_seen = False
            result['execution']['irq_completions'] += 1

    def port_in(uc, port, size, _user):
        if port == 0x201:
            return gameport.read(uc)
        if port in (0x60, 0x64):
            return keyboard.read(port, uc)
        if port == 0x3da:
            return vga_status.read()
        if port == 0x61:
            return 0
        result['status'] = 'unsupported_interface'
        result['detail'] = f'unsupported IN port={port:04x}'
        uc.emu_stop()
        return 0

    def port_out(uc, port, size, value, _user):
        if port == 0x60:
            keyboard.write(port, value)
            return
        if port in (0x20, 0x43, 0x40):
            try:
                timer.port_out(port, value)
            except UnsupportedGuestOperation as exc:
                result['status'] = 'unsupported_interface'
                result['detail'] = str(exc)
                uc.emu_stop()
            return
        if port in (0x3c8, 0x3c9):
            dac.write(uc, port, value)
            return
        if port == 0x201:
            gameport.write(uc)
            return
        if port == 0x61:
            return
        result['status'] = 'unsupported_interface'
        result['detail'] = f'unsupported OUT port={port:04x} value={value:08x}'
        uc.emu_stop()

    cpu.hook_add(UC_HOOK_INTR, interrupt)
    if not fast_execution:
        cpu.hook_add(UC_HOOK_CODE, instruction)
    if ring3:
        cpu.hook_add(UC_HOOK_INSN, port_in, None, 1, 0, UC_X86_INS_IN)
        cpu.hook_add(UC_HOOK_INSN, port_out, None, 1, 0, UC_X86_INS_OUT)
    try:
        try:
            if not fast_execution:
                cpu.emu_start(table_base + 0x200 if ring3 else ENTRY, 0xffffffff,
                              count=instruction_limit)
            else:
                # Run bounded native Unicorn instruction batches without the
                # expensive per-instruction Python callback. PIT and display
                # boundaries are exact; an EOI mid-batch is serviced at the
                # next batch boundary (at most fast_quantum instructions).
                ip = table_base + 0x200 if ring3 else ENTRY
                while result['status'] == 'instruction_limit':
                    count = fast_quantum
                    count = min(count, timer.interval - timer.instructions % timer.interval)
                    if instruction_limit:
                        remaining = instruction_limit - timer.instructions
                        if remaining <= 0:
                            break
                        count = min(count, remaining)
                    if display is not None:
                        count = min(count, display_instructions - timer.instructions % display_instructions)
                    if progress_instructions:
                        count = min(count, progress_instructions - timer.instructions % progress_instructions)
                    if pending_frames:
                        count = min(count, max(1, pending_frames[0] - timer.instructions))
                    if sample_instructions:
                        sample_next = (sample_instructions // 2 - timer.instructions - 1) % sample_instructions + 1
                        count = min(count, sample_next)
                    cpu.emu_start(ip, 0xffffffff, count=count)
                    if result['status'] != 'instruction_limit':
                        break
                    if eoi_hook is not None and not timer.in_service:
                        cpu.hook_del(eoi_hook)
                        eoi_hook = None
                    timer.instructions += count - 1
                    ip = cpu.reg_read(UC_X86_REG_EIP)
                    instruction(cpu, ip, 0, None)
                    if timer.in_service and eoi_hook is None:
                        # Arm at the resume address *before* executing the IRQ
                        # handler, and invalidate the previously translated
                        # instruction so Unicorn cannot skip this new hook.
                        eoi_hook = cpu.hook_add(UC_HOOK_CODE, post_eoi_instruction,
                                                None, timer.return_ip, timer.return_ip)
                        cpu.ctl_remove_cache(timer.return_ip, timer.return_ip + 1)
                        result['execution']['irq_hook_installs'] += 1
                    ip = cpu.reg_read(UC_X86_REG_EIP)
        except UcError as exc:
            result['status'] = 'cpu_fault'
            result['detail'] = str(exc)
    finally:
        if display is not None:
            display.close()
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
    result['mouse_events'] = mouse_script.applied
    if mouse_trace is not None:
        result['mouse_call_sites'] = mouse_trace.report()
        result['mouse_gui'] = mouse_trace.gui_report()
    if decompress_trace is not None:
        result['decompression'] = decompress_trace
    result['key_events'] = key_script.applied
    result['joystick_events'] = joystick_script.applied
    result['gameport'] = gameport.report()
    result['timer'] = {'instructions': timer.instructions, 'ticks': timer.ticks,
                       'irq0': timer.interrupts - timer.keyboard_interrupts,
                       'irq1': timer.keyboard_interrupts, 'divisor': timer.divisor,
                       'in_service': timer.in_service}
    result['real_mode_requests'] = real_mode.requests if real_mode is not None else []
    if real_mode is not None:
        result['video_mode'] = real_mode.engine.video_mode
        if framebuffer is not None and save_frame(framebuffer):
            result['framebuffer'] = str(framebuffer)
    return result


def main(argv=None, *, interactive=False):
    parser = argparse.ArgumentParser(prog='ef2000-unicorn' if interactive else None,
                                     description=__doc__)
    version = os.environ.get('EF2000_VERSION') or (Path(__file__).resolve().parent.parent / 'VERSION').read_text().strip()
    parser.add_argument('-v', '--version', action='version', version=f'ef2000-unicorn {version}')
    parser.add_argument('executable', type=Path, nargs='?',
                        help='EF2000 installation directory or original _EF2000.EXE')
    parser.add_argument('--data-dir', type=Path, metavar='DIR',
                        help='original EF2000 game data containing _EF2000.EXE')
    parser.add_argument('--user-data-dir', type=Path, metavar='DIR',
                        help='installed user game data (default: XDG_DATA_HOME/ef2000)')
    parser.add_argument('--state-dir', type=Path, metavar='DIR',
                        help='state root for interactive writable game overlay')
    parser.add_argument('--portable', action='store_true',
                        help='place user files next to the game executable')
    parser.add_argument('--show-paths', action='store_true',
                        help='print resolved game, state, and user-data paths and exit')
    parser.add_argument('--instruction-limit', type=int,
                        default=0 if interactive else None,
                        help='guest instruction budget (headless default 30000000; '
                             'windowed default unlimited; 0 = unlimited)')
    parser.add_argument('--timer-instructions', type=int, default=100000 if interactive else 250000,
                        help='deterministic guest instruction count per PIT tick (default: %(default)s)')
    parser.add_argument('--mouse-event', action='append', default=[], metavar='TICK:X:Y:BUTTONS',
                        help='script mouse coordinates and button mask at a PIT tick; repeat to press/release')
    parser.add_argument('--key-event', action='append', default=[], metavar='TICK:SCAN:ASCII:DOWN',
                        help='script SDL keyboard make/break at a PIT tick (e.g. 600:41:27:1 for Escape)')
    parser.add_argument('--joystick-event', action='append', default=[],
                        metavar='TICK:X:Y[:Z:W]:BUTTONS',
                        help='script signed SDL joystick axes (-32768..32767) and button mask (0..15)')
    parser.add_argument('--game-dir', type=Path, help='read-only asset directory (default: EXE directory)')
    parser.add_argument('--cd-dir', type=Path, help='read-only root for absolute DOS CD paths')
    parser.add_argument('--overlay-dir', type=Path,
                        help='separate writable DOS drive overlay (original game data remains read-only)')
    parser.add_argument('--sound', choices=['off', 'original'], default='off',
                        help='disable original sound drivers in a virtual config copy (default: off)')
    parser.add_argument('--flat-ring0', action='store_true', help='diagnose legacy flat ring-0 CPU model')
    window_mode = parser.add_mutually_exclusive_group()
    window_mode.add_argument('-w', '--windowed', action='store_true',
                             help='show experimental SDL3 video and forward physical input')
    window_mode.add_argument('--headless', action='store_false', dest='windowed',
                             help='disable SDL window and use bounded diagnostic execution')
    parser.set_defaults(windowed=interactive)
    parser.add_argument('-f', '--fullscreen', action='store_true', help='start SDL3 window fullscreen')
    parser.add_argument('-g', '--geometry', default='1280x960', metavar='WIDTHxHEIGHT',
                        help='initial SDL3 window size (default: %(default)s)')
    parser.add_argument('--aspect-ratio', choices=('4:3', 'native', 'stretch'), default='4:3',
                        help='SDL3 output aspect ratio (default: %(default)s)')
    parser.add_argument('--smooth-scaling', action='store_true',
                        help='use filtered rather than nearest-neighbor scaling')
    parser.add_argument('--display-instructions', type=int, default=50000, metavar='N',
                        help='guest instructions between SDL3 event polls (default: %(default)s)')
    parser.add_argument('--sample-instructions', type=int, default=0, metavar='N',
                        help='sample guest EIP and registers every N instructions (0 disables sampling)')
    parser.add_argument('--progress-instructions', type=int,
                        default=100000000 if interactive else 0, metavar='N',
                        help='print running instruction/tick/video/checksum progress to stderr every N instructions '
                             '(0 disables progress; default: %(default)s)')
    execution = parser.add_mutually_exclusive_group()
    execution.add_argument('--fast-execution', action='store_true',
                           help='batch guest execution for speed; EOI and trace timing is approximate')
    execution.add_argument('--precise-execution', action='store_false', dest='fast_execution',
                           help='use the original per-instruction hook for exact diagnostics')
    parser.set_defaults(fast_execution=interactive)
    parser.add_argument('--fast-quantum', type=int, default=1000, metavar='N',
                        help='maximum guest instructions per fast batch (default: %(default)s)')
    parser.add_argument('--trace-mouse', action='store_true',
                        help='summarize original guest mouse wrappers, callers, and button state')
    parser.add_argument('--trace-decompress', action='store_true',
                        help='record original LZ output checksums, buffer bounds, and integrity failures')
    parser.add_argument('--framebuffer', type=Path,
                        help='save the last available VGA frame to a PNG (including headless)')
    parser.add_argument('--framebuffer-sample', type=int, action='append', default=[], metavar='N',
                        help='also save a PNG at guest instruction N (repeatable; requires --framebuffer)')
    parser.add_argument('--report', type=Path, help='optional JSON diagnostic output')
    args = parser.parse_args(argv)
    if args.executable and args.executable.is_dir():
        args.executable = args.executable / '_EF2000.EXE'
    if args.instruction_limit is not None and args.instruction_limit < 0:
        parser.error('--instruction-limit must be nonnegative (0 = unlimited)')
    if args.timer_instructions <= 0:
        parser.error('--timer-instructions must be positive')
    if args.display_instructions <= 0:
        parser.error('--display-instructions must be positive')
    if args.sample_instructions < 0:
        parser.error('--sample-instructions must be nonnegative')
    if args.progress_instructions < 0:
        parser.error('--progress-instructions must be nonnegative')
    if args.fast_quantum <= 0:
        parser.error('--fast-quantum must be positive')
    if args.fast_execution and (args.trace_mouse or args.trace_decompress):
        parser.error('--fast-execution cannot combine with instruction-level tracing')
    if args.fullscreen and not args.windowed:
        parser.error('--fullscreen requires a window (omit --headless)')
    if any(value <= 0 for value in args.framebuffer_sample) or (args.framebuffer_sample and not args.framebuffer):
        parser.error('--framebuffer-sample needs a positive N and --framebuffer')
    match = re.fullmatch(r'([1-9][0-9]*)x([1-9][0-9]*)', args.geometry)
    if not match or any(int(value) > 16384 for value in match.groups()):
        parser.error('--geometry must be WIDTHxHEIGHT with dimensions in 1..16384')
    try:
        MouseScript(args.mouse_event)
        KeyScript(args.key_event)
        JoystickScript(args.joystick_event)
    except ValueError as exc:
        parser.error(str(exc))
    if args.data_dir and args.executable and args.executable.resolve().parent != args.data_dir.resolve():
        parser.error('--data-dir must match the directory containing the supplied executable')
    if not args.executable and args.data_dir:
        args.executable = args.data_dir / '_EF2000.EXE'
    if args.portable and args.executable is None:
        parser.error('--portable requires an executable or --data-dir')
    paths = None
    if (interactive or args.show_paths or args.portable or args.state_dir or
            args.user_data_dir or args.executable is None):
        try:
            try:
                from .paths import resolve_paths, format_paths
            except ImportError:
                from paths import resolve_paths, format_paths
            paths = resolve_paths({'state': args.state_dir, 'user_data': args.user_data_dir},
                                  portable=args.portable, executable=args.executable)
        except ValueError as exc:
            parser.error(str(exc))
    if args.show_paths:
        print(format_paths(paths, args.data_dir))
        return 0
    if args.executable is None:
        args.executable = paths['user_data'][0] / '_EF2000.EXE'
        if (args.executable.parent / 'portable.txt').is_file():
            paths = resolve_paths({'state': args.state_dir, 'user_data': args.user_data_dir},
                                  portable=args.portable, executable=args.executable)
    if not args.executable.is_file():
        parser.error('original _EF2000.EXE missing; supply its path, --data-dir, '
                     'or install it under the user-data directory shown by --show-paths')
    if args.overlay_dir is None and interactive:
        args.overlay_dir = paths['state'][0] / 'unicorn' / 'drive'
        try:
            args.overlay_dir.mkdir(parents=True, exist_ok=True)
        except OSError as exc:
            parser.error(f'cannot create Unicorn writable overlay: {exc}')
    try:
        from .extract_image import extract
    except ImportError:
        from extract_image import extract
    _manifest, images = extract(args.executable.read_bytes())
    report = run(images['payload.bin'], args.instruction_limit,
                 game_dir=args.game_dir or args.executable.parent,
                 cd_dir=args.cd_dir, overlay_dir=args.overlay_dir,
                 ring3=not args.flat_ring0, sound=args.sound,
                 timer_instructions=args.timer_instructions, mouse_events=args.mouse_event,
                 key_events=args.key_event, joystick_events=args.joystick_event,
                 windowed=args.windowed, fullscreen=args.fullscreen,
                 geometry=tuple(map(int, match.groups())), aspect_ratio=args.aspect_ratio,
                 smooth_scaling=args.smooth_scaling,
                 display_instructions=args.display_instructions, framebuffer=args.framebuffer,
                 framebuffer_samples=args.framebuffer_sample,
                 sample_instructions=args.sample_instructions, trace_mouse=args.trace_mouse,
                 trace_decompress=args.trace_decompress,
                 progress_instructions=args.progress_instructions,
                 fast_execution=args.fast_execution, fast_quantum=args.fast_quantum)
    if interactive:
        print(f'ef2000-unicorn: {report["status"]}; '
              f'instructions={report["timer"]["instructions"]}', flush=True)
        if 'detail' in report:
            print(f'ef2000-unicorn: {report["detail"]}', file=sys.stderr)
    else:
        print(json.dumps(report, indent=2))
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + '\n')
    # An unsupported guest request is the expected diagnostic boundary, not
    # successful full game execution.
    if interactive and report['status'] == 'window_closed':
        return 0
    if interactive and report['status'] == 'guest_exit':
        return report.get('exit_code', 0)
    return {'unsupported_interface': 20, 'guest_integrity_failure': 20,
            'guest_exit': 26}.get(report['status'], 21)


if __name__ == '__main__':
    raise SystemExit(main())
