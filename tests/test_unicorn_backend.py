# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

import os
from pathlib import Path
import unittest

from tools.unicorn_backend import (ENTRY, IMAGE_SIZE, UnsupportedGuestOperation,
                                   run, serve_startup_int21, startup_memory)


class UnicornStartupServiceTests(unittest.TestCase):
    def setUp(self):
        self.regs = dict(ax=0x3505, bx=4, cx=0x260, dx=0, si=0, di=0,
                         bp=0, sp=0x3fff000, ip=0x95, flags=0x202)

    def test_first_x32_selector_limit_request(self):
        result = serve_startup_int21(0x93, self.regs)
        self.assertEqual(result['cx'], 0x260)
        self.assertEqual(result['flags'] & 0x41, 0x40)
        self.assertEqual(self.regs['flags'], 0x202)

    def test_dos_version_request(self):
        regs = dict(self.regs, ax=0x3005, bx=0x1234, cx=0x12345678)
        result = serve_startup_int21(0xa0, regs)
        self.assertEqual((result['ax'], result['bx'], result['cx']),
                         (5, 0x1200, 0x12340000))

    def test_unknown_interface_stops(self):
        with self.assertRaises(UnsupportedGuestOperation):
            serve_startup_int21(0x19a, dict(self.regs, ax=0x3509))

    def test_image_validation(self):
        with self.assertRaises(ValueError):
            startup_memory(b'')
        with self.assertRaises(ValueError):
            startup_memory(b'\0' * (IMAGE_SIZE + 1))


try:
    import unicorn  # noqa: F401
except ImportError:
    HAS_UNICORN = False
else:
    HAS_UNICORN = True


@unittest.skipUnless(HAS_UNICORN, 'requires Unicorn Python bindings')
class UnicornGuestTests(unittest.TestCase):
    def test_synthetic_original_instruction_execution(self):
        image = bytearray(0x195fb0)
        # Real x86 instructions, not a mocked Unicorn API.
        image[ENTRY:ENTRY + 8] = bytes.fromhex('66 b8 34 12 cd 21 eb fe')
        result = run(bytes(image), instruction_limit=50)
        self.assertEqual(result['status'], 'unsupported_interface')
        self.assertEqual(result['events'][0]['ip'], ENTRY + 4)
        self.assertEqual(result['events'][0]['ax'], 0x1234)

    @unittest.skipUnless(os.environ.get('EF2000_EXE'), 'requires original EF2000_EXE')
    def test_original_extracted_code_reaches_stack_allocation(self):
        from tools.extract_image import extract
        image = extract(Path(os.environ['EF2000_EXE']).read_bytes())[1]['payload.bin']
        result = run(image)
        self.assertEqual(result['status'], 'unsupported_interface')
        self.assertEqual([(e['ip'], e['ax'] & 0xffff) for e in result['events']],
                         [(0x93, 0x3505), (0xa0, 0x3005), (0x19a, 0x3509)])
        self.assertTrue(result['startup_checkpoint']['bss_zero'])
        self.assertEqual(result['startup_checkpoint']['cx'], 0x4000)
        self.assertEqual(result['startup_checkpoint']['si'], 0x4df000)
        self.assertEqual(result['startup_checkpoint']['di'], 0x4de2f0)
