# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

import signal
import struct
import unittest

from tools.probe_entry import first_boundary, make_elf


class ProbeTests(unittest.TestCase):
    def test_elf_load_segments_preserve_payload_and_memory_contract(self):
        payload = bytes(range(256))
        elf = make_elf(payload)
        self.assertEqual(elf[:7], b'\x7fELF\x01\x01\x01')
        entry = struct.unpack_from('<I', elf, 24)[0]
        program_offset = struct.unpack_from('<I', elf, 28)[0]
        for index in range(2):
            kind, offset, address, _, file_size, memory_size, flags, align = struct.unpack_from(
                '<8I', elf, program_offset + index * 32)
            self.assertEqual(kind, 1)
            self.assertEqual(offset % align, address % align)
            self.assertLessEqual(file_size, memory_size)
            self.assertLessEqual(offset + file_size, len(elf))
            if address == 0:
                self.assertEqual(elf[offset:offset + file_size], payload)
                self.assertGreater(memory_size, 0x7ff000)
                self.assertTrue(flags & 1)
            else:
                self.assertTrue(address <= entry < address + file_size)

    def test_expected_fault_alone_is_not_success(self):
        self.assertIsNone(first_boundary('', -signal.SIGSEGV))
        self.assertIsNone(first_boundary('EIP=00000093 EAX=00003505', -signal.SIGSEGV))

    def test_boundary_requires_matching_registers_and_fault(self):
        trace = ('----------------\nIN:\n0x00000093:  cd 21  int  $0x21\n'
                 'EAX=00003505 EBX=0000002b ECX=00000260 ESP=007ff000\n'
                 'EIP=00000093 EFL=00000202\n')
        self.assertEqual(first_boundary(trace, -signal.SIGSEGV)['EAX'], 0x3505)
        self.assertIsNone(first_boundary(trace, 0))
        self.assertIsNone(first_boundary(trace.replace('ECX=00000260', 'ECX=00000000'),
                                        -signal.SIGSEGV))


if __name__ == '__main__':
    unittest.main()
