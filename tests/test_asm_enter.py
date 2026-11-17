# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Native protected-mode ENTER ordering and explicit-state comparison."""
import ctypes
import json
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile
import unittest

from tests.test_asm_to_c import assemble
from tests.test_asm_capture import fixture
from tools import asm_to_c, asm_segments, asm_c_differential as diff


@unittest.skipUnless(platform.system() == 'Linux' and platform.machine() == 'x86_64'
                     and all(shutil.which(t) for t in ('cc', 'as', 'objcopy')),
                     'requires Linux x86-64, compiler and binutils')
class EnterTests(unittest.TestCase):
    def test_native_allocation_fault_preserves_registers_but_keeps_stack_write(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            native = root/'native'
            subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-O0',
                            str(Path(__file__).with_name('native_enter.c')), '-o', str(native)], check=True)
            (root/'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
            for allocation in (0, 1, 4, 8, 64, 2132):
                result = subprocess.run([str(native), str(allocation)], capture_output=True, text=True)
                if result.returncode == 2:
                    self.skipTest('native compatibility-mode probe unavailable: '+result.stderr)
                self.assertEqual(result.returncode, 0, result.stderr)
                observed = json.loads(result.stdout)
                self.assertEqual(observed['trap'], 12 if allocation else 3, observed)
                self.assertEqual(observed['ip_offset'], 16 if allocation else 21, observed)
                self.assertEqual(observed['error'], 0, observed)
                self.assertEqual(observed['selectors'] & 0xffff, 0x23, observed)
                self.assertEqual(observed['saved_ebp'], 77, observed)
                _, record = assemble(root, 'enter %d,0\nret' % allocation)
                (root/'lift.c').write_text(asm_to_c.translate(record))
                lib = root/('lift%d.so' % allocation)
                subprocess.run(['cc', '-shared', '-fPIC', '-O0', '-std=c99', '-Wall', '-Wextra',
                                '-Werror', '-Wno-unused-function', str(root/'lift.c'), '-o', str(lib)], check=True)
                function = ctypes.CDLL(str(lib)).ef_lift_00000100
                function.argtypes = [ctypes.POINTER(diff.State), ctypes.c_uint32]
                capture = fixture()
                memory = bytearray(capture.guest_ram)
                ram = (ctypes.c_uint8 * len(memory)).from_buffer(memory)
                regs = capture.registers
                state = diff.State((ctypes.c_uint32 * 8)(*[regs[n] for n in asm_to_c.REGS]),
                                   regs['eflags'], 0x100, 0, ram, len(memory))
                state.segment[:] = [regs[n] for n in asm_to_c.SEGMENTS]
                state.segment_profile = 1
                for i, name in enumerate(asm_to_c.SEGMENTS):
                    state.segment_cache[i] = diff.Segment(*asm_segments.descriptor(
                        regs[name], capture.table_context['descriptors'][name]))
                state.segment_cache[2].limit = regs['esp']-5
                before = list(state.r)
                self.assertEqual(function(ctypes.byref(state), 1), 7 if allocation else 1)
                self.assertEqual(state.pc, 0x100 if allocation else 0x104)
                self.assertEqual(state.r[4]-regs['esp'], observed['esp']-observed['initial_esp'])
                expected_bp = observed['ebp'] if allocation else regs['esp']-4
                self.assertEqual(state.r[5], expected_bp)
                if allocation:
                    self.assertEqual(list(state.r), before)
                self.assertEqual(state.flags, regs['eflags'])
                expected_memory = bytearray(capture.guest_ram)
                expected_memory[regs['esp']-4:regs['esp']] = (77).to_bytes(4, 'little')
                self.assertEqual(memory, expected_memory)
                if allocation == 8:
                    # Rejection of the saved-EBP push has no memory effects.
                    memory[:] = capture.guest_ram
                    state.r[:] = before
                    state.pc, state.fault = 0x100, 0
                    state.segment_cache[2].limit = regs['esp']-4
                    self.assertEqual(function(ctypes.byref(state), 1), 7)
                    self.assertEqual(list(state.r), before)
                    self.assertEqual(memory, capture.guest_ram)
                    # A final pointer outside modeled RAM faults after the push.
                    state.segment_profile = 0
                    state.segment[:] = [0] * 6
                    state.pc, state.fault, state.r[4] = 0x100, 0, 4
                    self.assertEqual(function(ctypes.byref(state), 1), 3)
                    self.assertEqual((state.r[4], state.r[5], state.pc), (4, 77, 0x100))
                    self.assertEqual(memory[:4], (77).to_bytes(4, 'little'))
