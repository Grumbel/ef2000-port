# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Small OMF fixtures with relocations, independent of proprietary binaries."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from tools.flirt_match import OMFError, code_publics, find_matches


def record(kind, payload):
    length = len(payload) + 1
    header = bytes([kind]) + length.to_bytes(2, 'little')
    checksum = (-sum(header + payload)) & 0xff
    return header + payload + bytes([checksum])


def pascal(value):
    return bytes([len(value)]) + value.encode()


def omf_fixture(code, fixup=b'\xa4\x04\x8c'):
    # One 32-bit CODE segment, one public name and an external 32-bit call
    # displacement beginning at code offset 4.
    return b''.join((
        record(0x80, pascal('fixture')),
        record(0x96, pascal('_TEXT') + pascal('CODE')),
        record(0x99, b'\x60' + len(code).to_bytes(4, 'little') +
               b'\x01\x02\x00'),
        record(0x91, b'\x00\x01' + pascal('sample_fn') +
               b'\x00\x00\x00\x00\x00'),
        record(0xa1, b'\x01\x00\x00\x00\x00' + code),
        record(0x9d, fixup),
        record(0x8a, b'\x00'),
    ))


class OMFMatcherTests(unittest.TestCase):
    def test_relocation_mask_and_unique_match(self):
        code = bytes.fromhex('558bec e8 00000000 31c0 4040 9090 9090 '
                             '9090 9090 9090 9090 9090 9090')
        signatures = code_publics(omf_fixture(code), 'fictional.obj')
        self.assertEqual(len(signatures), 1)
        signature = signatures[0]
        self.assertEqual(signature['symbol'], 'sample_fn')
        self.assertEqual(signature['mask'], (b'\x00' * 4 + b'\x01' * 4 +
                                              b'\x00' * (len(code) - 8)).hex())
        image = bytearray(b'\x90' * 128)
        image[32:32 + len(code)] = code
        image[36:40] = b'\x01\xfe\x11\xff'
        result = find_matches(image, [(32, 64)], signatures)
        self.assertEqual(result[0]['addr'], '0x00000020')
        self.assertTrue(result[0]['unique'])
        self.assertEqual(len(find_matches(image, [(0, 32)], signatures)), 0)
        self.assertEqual(len(find_matches(image, [(33, 64)], signatures)), 0)

    def test_rejects_unbounded_relocations_and_bad_checksums(self):
        code = b'\x90' * 24
        with self.assertRaisesRegex(OMFError, 'fixup extends past LEDATA'):
            code_publics(omf_fixture(code, b'\xa4\x17\x8c'), 'fictional.obj')
        invalid = bytearray(omf_fixture(code))
        invalid[10] ^= 0x01
        with self.assertRaisesRegex(OMFError, 'checksum'):
            code_publics(bytes(invalid), 'fictional.obj')

    def test_cli_build_and_match_does_not_mutate_tracker(self):
        code = bytes.fromhex('558bec e8 00000000 31c0 4040 9090 9090 '
                             '9090 9090 9090 9090 9090 9090')
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            obj = root / 'fixture.obj'
            obj.write_bytes(omf_fixture(code))
            signatures = root / 'local' / 'library-signatures.json'
            image = root / 'payload.bin'
            image.write_bytes(b'\0' * 64 + code + b'\0' * 16)
            tracker = root / 'functions.tsv'
            tracker.write_text('addr\tsize\n0x00000040\t40\n')
            proposals = root / 'proposals.json'
            script = Path(__file__).resolve().parents[1] / 'tools/flirt_match.py'
            subprocess.run([sys.executable, str(script), 'build', str(obj),
                            '-o', str(signatures)], check=True, capture_output=True)
            subprocess.run([sys.executable, str(script), 'match', '--signatures',
                            str(signatures), '--image', str(image), '--functions',
                            str(tracker), '-o', str(proposals)],
                           check=True, capture_output=True)
            self.assertEqual(json.loads(proposals.read_text())['matches'][0]['addr'],
                             '0x00000040')
            self.assertEqual(tracker.read_text(), 'addr\tsize\n0x00000040\t40\n')


if __name__ == '__main__':
    unittest.main()
