# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

import struct
import json
from pathlib import Path
import unittest

from tools.read_runtime_probe import decode


class RuntimeReferenceTests(unittest.TestCase):
    def test_original_divide_exception_context_and_resume_evidence(self):
        reference = json.loads((Path(__file__).resolve().parent.parent /
                                'docs/exception-reference.json').read_text())
        self.assertTrue(reference['source_unchanged'])
        frame = reference['handler_entry_frame_words']
        context = reference['exception_context_words']
        self.assertEqual(context[3] >> 16, 2)
        self.assertEqual(context[5], frame[1] & 65535)
        self.assertEqual(reference['resumed_registers'],
                         dict(eax=0x12345678, ebx=0x11223344, ecx=0, edx=0))
        self.assertEqual(reference['instrumentation']['source_sha256'],
                         'f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd')

    def test_packed_vector_selector_and_full_registers(self):
        record = bytearray(0x1a4)
        record[:4] = b'XVEC'
        struct.pack_into('<HHII', record, 4, 0x2b, 1, 0x12345678, 0xdfffef00)
        struct.pack_into('<10I', record, 16, 0x12342502, 0xdfffef00, 0x55667780,
                         0x99aabbcc, 0x12345678, 0x89abcdef, 0x3286, 0x3247, 0x2b, 0x33)
        record[-4:] = b'DONE'
        result = decode(record)
        self.assertEqual(result['saved_pm_selector'], 0x2b)
        self.assertEqual(result['saved_rm_vector'], 0x12345678)
        self.assertEqual(result['samples'][0]['ecx'], 0x55667780)
        self.assertEqual(result['samples'][0]['es'], 0x2b)
        for invalid in (record[:-1], record + b'\0', bytes(len(record))):
            with self.assertRaises(ValueError):
                decode(invalid)

    def test_memory_and_file_record_versions_and_completion(self):
        for magic, count in ((b'DMEM', 2), (b'DFIL', 7)):
            record = bytearray(20 + count * 32)
            record[:4] = magic
            struct.pack_into('<I', record, 4, 1)
            record[-4:] = b'DONE'
            self.assertEqual(len(decode(record)['samples']), count)
            record[-1] = 0
            with self.assertRaises(ValueError):
                decode(record)
