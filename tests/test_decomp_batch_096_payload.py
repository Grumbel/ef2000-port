# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Pinned-byte audit for the OPTIONS line reader and format wrapper."""

import csv
import hashlib
import os
from pathlib import Path
import struct
import unittest


ROOT = Path(__file__).resolve().parents[1]
SHA256 = "3fd26b189efe289feb9e6ac701d592a5d48d7c1e152527aa00c01611e87e0fdb"
ENTRIES = {0xF2D74: (86, "gui_options_read_command_f2d74"),
           0xF6124: (38, "format_global_option_line")}


class Inventory096(unittest.TestCase):
    def test_two_original_entries_have_host_drafts(self):
        with (ROOT / "decomp/symbols/functions.tsv").open(newline="") as stream:
            entries = {int(row["addr"], 16): row
                       for row in csv.DictReader(stream, delimiter="\t")}
        for address, (size, name) in ENTRIES.items():
            with self.subTest(address=hex(address)):
                row = entries[address]
                self.assertEqual(int(row["size"]), size)
                self.assertEqual(row["name"], name)
                self.assertEqual(row["module"], "gui_options_reader_096")
                self.assertEqual(row["state"], "drafted")
                self.assertEqual(row["sig_status"], "unverified")


class OriginalBytes096(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = os.environ.get("EF2000_PAYLOAD")
        if not source:
            raise unittest.SkipTest("set EF2000_PAYLOAD for original-byte audit")
        cls.payload = Path(source).read_bytes()
        if hashlib.sha256(cls.payload).hexdigest() != SHA256:
            raise AssertionError("unrecognized EF2000 payload")

    def body(self, address):
        return self.payload[address:address + ENTRIES[address][0]]

    def calls(self, address):
        body = self.body(address)
        return [address + offset + 5 + struct.unpack_from("<i", body, offset + 1)[0]
                for offset in range(len(body) - 4) if body[offset] == 0xE8]

    def test_line_reader_skips_and_falls_through_to_lowercase(self):
        address = 0xF2D74
        body = self.body(address)
        self.assertEqual(len(body), 86)
        self.assertEqual(body[:5], b"\xe8\xff\xfe\xff\xff")
        for target in (0xF2C78, 0xB1B8B, 0xF2CD0, 0xF2D54, 0xE7835):
            with self.subTest(callee=hex(target)):
                self.assertEqual(self.calls(address).count(target), 1)
        self.assertIn(struct.pack("<I", 0x485956), body)
        self.assertIn(struct.pack("<I", 0x48592C), body)
        self.assertIn(b"\x83\xf8\x3b\x74", body)  # ';' => another line
        self.assertIn(b"\x83\xf8\x03\x72", body)  # length < 3 => retry
        self.assertEqual(body[0x2f:0x32], b"\x90\x53\x52")  # fallthrough
        self.assertEqual(body[-2:], b"\xeb\xde")  # loop back to f2da8

    def test_formatter_is_four_argument_indirect_call(self):
        body = self.body(0xF6124)
        self.assertEqual(len(body), 38)
        self.assertEqual(body[:2], b"\x52\x31")  # saves EDX, clears EAX
        for value in (0x464396, 0x16007C, 0x464390, 0x192C4C):
            with self.subTest(global_address=hex(value)):
                self.assertIn(struct.pack("<I", value), body)
        self.assertIn(b"\xff\x15\x74\x2a\x13\x00", body)
        self.assertEqual(body[-2:], b"\x5a\xc3")


if __name__ == "__main__":
    unittest.main()
