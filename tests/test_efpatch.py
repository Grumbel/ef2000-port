# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Asset-free binary patch format and in-place-only policy regression tests."""

import hashlib
from pathlib import Path
import struct
import tempfile
import unittest

from tools.efpatch import PatchError, apply_patches, decode, pack, pack_writes, MAGIC


class EFPatchTests(unittest.TestCase):
    def setUp(self):
        self.image = bytes(range(128))
        self.base = {'format': 'ef2000-patch-v1',
                     'base_image_sha256': hashlib.sha256(self.image).hexdigest(),
                     'arena_base': 0x1000, 'arena_end': 0x1000}

    def plan(self, offset=32, data='cc90', kind='inline'):
        return dict(self.base, writes=[{'offset': offset, 'bytes': data, 'kind': kind}])

    def test_pack_and_decode_preserve_exact_bytes(self):
        binary = pack(self.plan(), self.image)
        self.assertEqual(binary[:8], MAGIC)
        self.assertEqual(decode(binary, self.image), [(32, b'\xcc\x90')])
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'test.efpatch'
            path.write_bytes(binary)
            output, digests = apply_patches(self.image, [path])
            self.assertEqual(output[:32], self.image[:32])
            self.assertEqual(output[32:34], b'\xcc\x90')
            self.assertEqual(self.image[32:34], bytes((32, 33)))
            self.assertEqual(digests, [hashlib.sha256(binary).hexdigest()])

    def test_refuse_arena_redirect_or_mismatched_image(self):
        for kind in ('arena', 'redirect'):
            with self.assertRaisesRegex(PatchError, 'arena/redirected'):
                pack(self.plan(kind=kind), self.image)
        binary = pack(self.plan(), self.image)
        with self.assertRaisesRegex(PatchError, 'different initialized payload'):
            decode(binary, bytes(128))
        with self.assertRaisesRegex(PatchError, 'different initialized payload'):
            pack(self.plan(), bytes(128))

    def test_truncation_overlap_and_trailing_data_fail_closed(self):
        binary = pack(self.plan(), self.image)
        for broken in (binary[:-1], binary + b'\x00', binary[:44]):
            with self.assertRaises(PatchError):
                decode(broken, self.image)
        p = self.plan()
        p['writes'].append({'offset': 33, 'kind': 'inline', 'bytes': '90'})
        with self.assertRaisesRegex(PatchError, 'overlapping'):
            pack(p, self.image)
        bad = bytearray(binary)
        struct.pack_into('<I', bad, 44, 127)
        with self.assertRaisesRegex(PatchError, 'beyond initialized payload'):
            decode(bad, self.image)

    def test_inter_patch_overlap_and_original_digest_binding(self):
        with tempfile.TemporaryDirectory() as tmp:
            a = Path(tmp) / 'a.efpatch'
            b = Path(tmp) / 'b.efpatch'
            a.write_bytes(pack(self.plan(offset=20), self.image))
            b.write_bytes(pack(self.plan(offset=21), self.image))
            with self.assertRaisesRegex(PatchError, 'overlapping'):
                apply_patches(self.image, [a, b])
            b.write_bytes(pack(self.plan(offset=40), self.image))
            result, hashes = apply_patches(self.image, [a, b])
            self.assertEqual(result[20:22], b'\xcc\x90')
            self.assertEqual(result[40:42], b'\xcc\x90')
            self.assertEqual(len(hashes), 2)

    def test_reserved_guest_arena_writes_expand_only_the_disposable_image(self):
        binary = pack_writes([(160, b'\x90\xc3')], self.image, 256)
        with self.assertRaisesRegex(PatchError, 'guest address space'):
            decode(binary, self.image)
        self.assertEqual(decode(binary, self.image, 256), [(160, b'\x90\xc3')])
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'arena.efpatch'
            path.write_bytes(binary)
            output, _ = apply_patches(self.image, [path], address_limit=256)
            self.assertEqual(len(output), 162)
            self.assertEqual(output[160:162], b'\x90\xc3')
            self.assertEqual(self.image[32:34], bytes((32, 33)))


if __name__ == '__main__':
    unittest.main()
