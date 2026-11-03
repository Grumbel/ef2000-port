# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Synthetic capture header tests; no original game data is needed."""

import hashlib
import struct
import unittest

from tools import call_capture
from tools import func_differential


def fixture():
    payload = b'original-code' * 300
    regs = [11, 22, 33, 44, 55, 66, 77, 0x2000, 0x100, 0x202, 7, 7, 7]
    memory = bytearray(0x3000)
    memory[0x2000:0x2004] = struct.pack('<I', 0x400)
    blob = call_capture.HEADER.pack(call_capture.MAGIC, hashlib.sha256(payload).digest(),
                                    0x100, 0, len(memory), len(regs), *regs) + memory
    return payload, bytes(blob)


class CallCaptureTests(unittest.TestCase):
    def test_header_and_original_image_binding(self):
        payload, blob = fixture()
        capture = call_capture.decode(blob, payload, 0x100)
        self.assertEqual(capture.registers['eax'], 11)
        self.assertEqual(capture.registers['esp'], 0x2000)
        self.assertEqual(len(capture.guest_ram), 0x3000)
        left, right = func_differential.scenarios_from_capture(
            capture, [(0x100, b'\xc3')])
        self.assertEqual(left[1]['eax'], 11)
        self.assertEqual(left[1]['eflags'], 0x202)
        self.assertEqual(left[3], [(0, capture.guest_ram)])
        self.assertEqual(right[3][-1], (0x100, b'\xc3'))

    def test_reject_wrong_image_function_and_invalid_layout(self):
        payload, blob = fixture()
        for original, address, error in ((b'wrong', 0x100, 'different original'),
                                         (payload, 0x101, 'different function')):
            with self.assertRaisesRegex(ValueError, error):
                call_capture.decode(blob, original, address)
        with self.assertRaisesRegex(ValueError, 'truncated'):
            call_capture.decode(blob[:4], payload)
        with self.assertRaisesRegex(ValueError, 'RAM size'):
            call_capture.decode(blob[:-1], payload)
        with self.assertRaisesRegex(ValueError, 'magic'):
            call_capture.decode(b'BADMAGIC' + blob[8:], payload)
        corrupt = bytearray(blob)
        struct.pack_into('<I', corrupt, 8 + 32 + 12, 99)  # register count
        with self.assertRaisesRegex(ValueError, 'register count'):
            call_capture.decode(corrupt, payload)

    def test_segmented_capture_rejected_by_flat_oracle(self):
        payload, blob = fixture()
        capture = call_capture.decode(blob, payload)
        from dataclasses import replace
        segment_changed = replace(capture, registers=dict(capture.registers, es=0x17))
        with self.assertRaisesRegex(ValueError, 'non-flat'):
            func_differential.scenarios_from_capture(segment_changed, [])


if __name__ == '__main__':
    unittest.main()
