# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
"""Generated-state VGA DAC writes share the hybrid's palette policy."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path
from tools import asm_to_c


@unittest.skipUnless(shutil.which('c++'), 'requires C++ compiler')
class VgaPortServiceTests(unittest.TestCase):
    def test_shared_dac_and_state_adapter(self):
        source = Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root/'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
            binary = root/'test'
            subprocess.run(['c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-Wno-unused-function', '-I'+str(root), '-I'+str(source/'runtime'),
                            str(source/'tests/asm_vga_port_service_test.cpp'),
                            str(source/'runtime/asm_vga_port_service.cpp'), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)

    @unittest.skipUnless(all(shutil.which(t) for t in ('as', 'objcopy', 'nm', 'cc')),
                         'requires binutils and C compiler')
    def test_generated_palette_loop_uses_native_adapter(self):
        import ctypes
        from tools import asm_c_program, asm_c_differential
        from tests.test_asm_c_program import inventory
        from tests.test_asm_to_c import unicorn
        source = Path(__file__).resolve().parents[1]
        assembly = '''.type root,@function
root: mov edx,0x3c8
mov eax,255
out dx,al
inc edx
mov ecx,780
again: out dx,al
inc eax
loop again
ret
.size root, .-root'''
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image, directory, _ = inventory(root, assembly)
            out = root/'out'
            asm_c_program.generate(directory, out, image, port_boundaries=True)
            objects = []
            for path in sorted(out.glob('*.c')):
                obj = path.with_suffix('.o')
                subprocess.run(['cc', '-O2', '-fPIC', '-I'+str(out), '-c', str(path),
                                '-o', str(obj)], check=True, capture_output=True)
                objects.append(str(obj))
            binary = root/'program.so'
            subprocess.run(['c++', '-std=c++17', '-O2', '-fPIC', '-shared', '-I'+str(out),
                            '-I'+str(source/'runtime'), *objects,
                            str(source/'runtime/asm_vga_port_service.cpp'),
                            '-o', str(binary)], check=True, capture_output=True)
            library = ctypes.CDLL(str(binary))
            callback = ctypes.CFUNCTYPE(ctypes.c_uint, ctypes.POINTER(asm_c_differential.State),
                                        ctypes.c_uint32, ctypes.c_uint, ctypes.c_void_p)
            service = callback(('ef_asm_vga_port_service', library))
            run = library.ef_lift_run_services
            run.argtypes = [ctypes.POINTER(asm_c_differential.State), ctypes.c_uint32,
                            ctypes.c_uint32, callback, ctypes.c_void_p]
            ram = bytearray(4096)
            ram[:image.stat().st_size] = image.read_bytes()
            ram[0x800:0x804] = (0xf00).to_bytes(4, 'little')
            original_ram = bytes(ram)
            array = (ctypes.c_uint8 * len(ram)).from_buffer(ram)
            state = asm_c_differential.State((ctypes.c_uint32*8)(1, 2, 3, 4, 0x800, 6, 7, 8),
                                             0x246, 0x100, 0, array, len(ram))
            context = (ctypes.c_uint32*2)(0, 0x900)  # DAC cursor, palette address
            self.assertEqual(run(ctypes.byref(state), 5000, 0xf00, service, context), 0)
            expected = bytearray(768)
            for i in range(780):
                expected[(765+i) % 768] = (255+i) & 63
            self.assertEqual(ram[0x900:0xc00], expected)
            self.assertEqual(context[0], (765+780) % 768)
            if unicorn:
                from unicorn import x86_const as x86
                cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
                cpu.mem_map(0, 4096)
                cpu.mem_write(0, original_ram)
                registers = [x86.UC_X86_REG_EAX, x86.UC_X86_REG_ECX, x86.UC_X86_REG_EDX,
                             x86.UC_X86_REG_EBX, x86.UC_X86_REG_ESP, x86.UC_X86_REG_EBP,
                             x86.UC_X86_REG_ESI, x86.UC_X86_REG_EDI]
                for register, value in zip(registers, [1, 2, 3, 4, 0x800, 6, 7, 8]):
                    cpu.reg_write(register, value)
                cpu.reg_write(x86.UC_X86_REG_EFLAGS, 0x246)
                cursor = 0
                def port_out(cpu, port, size, value, user):
                    nonlocal cursor
                    if port == 0x3c8:
                        cursor = (value & 255)*3
                    else:
                        self.assertEqual(port, 0x3c9)
                        cpu.mem_write(0x900+cursor, bytes([value & 63]))
                        cursor = (cursor+1) % 768
                cpu.hook_add(unicorn.UC_HOOK_INSN, port_out, None, 1, 0, x86.UC_X86_INS_OUT)
                cpu.emu_start(0x100, 0xf00, count=5000)
                self.assertEqual([cpu.reg_read(r) for r in registers], list(state.r))
                self.assertEqual(cpu.reg_read(x86.UC_X86_REG_EFLAGS), state.flags)
                self.assertEqual(bytes(cpu.mem_read(0, 4096)), bytes(ram))
