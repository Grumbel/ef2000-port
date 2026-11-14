# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Synthetic capture header tests; no original game data is needed."""

import hashlib
import os
from pathlib import Path
import struct
import subprocess
import tempfile
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


def fixture2():
    payload, old = fixture()
    fields = call_capture.HEADER.unpack_from(old)
    registers = list(fields[6:])+[7, 0x17, 0x2f]
    fp = [0x37f, 5 << 11, 0xffff, 0x123456, 0x2f, 0x234567, 7, 0x321]
    for i in range(8): fp += [i, 0x80000000, 0x3fff+i]
    context = [0x02010400, 32, 10, 6, 0, 0, 0]+fp+[0]*22
    header = call_capture.HEADER2.pack(call_capture.MAGIC2, fields[1], fields[2], fields[3],
                                      fields[4], 16, *registers)
    return payload, header+call_capture.CONTEXT2.pack(*context)+old[call_capture.HEADER.size:]


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
        self.assertEqual(capture.format_version, 1)
        self.assertIsNone(capture.x87)
        self.assertIsNone(capture.table_context)

    def test_extended_context_preserves_physical_empty_payloads(self):
        payload, blob = fixture2()
        capture = call_capture.decode(blob, payload, 0x100)
        self.assertEqual(capture.format_version, 2)
        self.assertEqual(capture.registers['cs'], 0x2f)
        self.assertEqual(capture.registers['fs'], 7)
        self.assertEqual(capture.x87['x87_status'], 5 << 11)
        self.assertEqual(capture.x87['x87_tag'], 0xffff)
        self.assertEqual(capture.x87['x87'], [[i, 0x80000000, 0x3fff+i] for i in range(8)])
        self.assertEqual(capture.x87['x87_opcode'], 0x321)
        self.assertTrue(all(v is None for v in capture.table_context['descriptors'].values()))
        self.assertIsNone(capture.table_context['gdtr']['bytes'])
        self.assertEqual(capture.producer_version, 0x02010400)
        with self.assertRaisesRegex(ValueError, 'EFCAP002 requires x87'):
            func_differential.scenarios_from_capture(capture, [])

    def test_extended_rejects_corrupt_counts_widths_and_unavailable_descriptors(self):
        payload, blob = fixture2()
        context_offset = call_capture.HEADER2.size
        original = list(call_capture.CONTEXT2.unpack_from(blob, context_offset))
        for index, value in ((1, 31), (2, 9), (3, 5), (4, 64), (5, 65537),
                             (7, 0x10000), (14, 0x800), (17, 0x10000),
                             (39, 0x10000), (49, 1)):
            changed = original.copy()
            changed[index] = value
            bad = bytearray(blob)
            call_capture.CONTEXT2.pack_into(bad, context_offset, *changed)
            with self.subTest(index=index), self.assertRaises(ValueError):
                call_capture.decode(bad, payload)
        bad = bytearray(blob)
        struct.pack_into('<I', bad, call_capture.HEADER2.size-4, 0x10000)
        with self.assertRaisesRegex(ValueError, 'field width'):
            call_capture.decode(bad, payload)
        for cut in (120, 200, len(blob)-1):
            with self.assertRaises(ValueError): call_capture.decode(blob[:cut], payload)
        with self.assertRaisesRegex(ValueError, 'different original'):
            call_capture.decode(blob, b'wrong')

    @unittest.skipUnless(os.environ.get('EF_CAPTURE_UNICORN_TEST'), 'requires built Unicorn capture fixture')
    def test_native_producer_matches_python_context_layout(self):
        payload, _ = fixture()
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary)/'capture.efcap'
            subprocess.run([os.environ['EF_CAPTURE_UNICORN_TEST'], str(path), '--keep',
                            hashlib.sha256(payload).hexdigest()], check=True)
            capture = call_capture.load(path, payload, 0x100)
            self.assertEqual(capture.x87['x87'], [[i, 0x80000000, 0x3fff+i] for i in range(8)])
            self.assertEqual(capture.x87['x87_status'], 5 << 11)
            self.assertEqual(capture.x87['x87_tag'], 0xffff)
            self.assertEqual(capture.registers['cs'], 8)
            self.assertEqual(capture.table_context['gdtr']['base'], 0x10000000)
            self.assertEqual(capture.table_context['gdtr']['limit'], 15)
            self.assertEqual(len(capture.table_context['gdtr']['bytes']), 16)
            self.assertEqual(len(capture.table_context['ldtr']['bytes']), 8)
            self.assertEqual(capture.table_context['descriptors']['cs'], 0x00cf9b000000ffff)
            self.assertEqual(capture.table_context['descriptors']['fs'], 0x0040f3345000ffff)
            self.assertIsNone(capture.table_context['descriptors']['ds'])
            bad = bytearray(path.read_bytes())
            bad[call_capture.HEADER2.size+call_capture.CONTEXT2.size+8] ^= 1
            with self.assertRaisesRegex(ValueError, 'inconsistent'):
                call_capture.decode(bad, payload)

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
