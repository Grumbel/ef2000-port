# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Pinned original-byte and inventory audit of the OPTIONS display loop."""

import csv
import hashlib
import os
from pathlib import Path
import struct
import unittest


ROOT = Path(__file__).resolve().parents[1]
SHA256 = "3fd26b189efe289feb9e6ac701d592a5d48d7c1e152527aa00c01611e87e0fdb"
ENTRIES = {0xF3D54: (560, "gui_cursor_step_f3d54", "gui_cursor_097"),
           0xF3F84: (497, "gui_options_display_f3f84", "gui_display_097")}


class Inventory097(unittest.TestCase):
    def test_original_entries_are_drafted(self):
        with (ROOT / "decomp/symbols/functions.tsv").open(newline="") as stream:
            entries = {int(row["addr"], 16): row
                       for row in csv.DictReader(stream, delimiter="\t")}
        for address, (size, name, module) in ENTRIES.items():
            with self.subTest(address=hex(address)):
                row = entries[address]
                self.assertEqual(int(row["size"]), size)
                self.assertEqual(row["name"], name)
                self.assertEqual(row["module"], module)
                self.assertEqual(row["state"], "drafted")
                self.assertEqual(row["sig_status"], "unverified")


class OriginalBytes097(unittest.TestCase):
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
        return [address + index + 5 + struct.unpack_from("<i", body, index + 1)[0]
                for index in range(len(body) - 4) if body[index] == 0xE8]

    def test_cursor_poll_timers_thresholds_and_clamping(self):
        body = self.body(0xF3D54)
        self.assertEqual(body[:5], b"\x51\x56\x57\x55\x83")
        calls = self.calls(0xF3D54)
        self.assertEqual(calls.count(0x1065C), 1)
        self.assertEqual(calls.count(0xA28F2), 6)
        self.assertIn(b"\xff\x15\xb8\xba\x15\x00", body)
        for address in (0x420DC4, 0x420DC8, 0x161E24, 0x161E28,
                        0x425FC6, 0x425FC8, 0x15BA2C):
            with self.subTest(address=hex(address)):
                self.assertIn(struct.pack("<I", address), body)
        for compare in (b"\x83\xf8\x23", b"\x83\xf8\x14", b"\x83\xf8\x0a"):
            self.assertEqual(body.count(compare), 2)
        self.assertIn(b"\x66\x83\x3d\xc8\x5f\x42\x00\x00", body)
        self.assertIn(b"\x66\x83\x3d\xc6\x5f\x42\x00\x00", body)

    def test_display_parameter_loop_and_shutdown(self):
        body = self.body(0xF3F84)
        self.assertEqual(body[:6], b"\x53\x51\x52\x56\x57\x83")
        calls = self.calls(0xF3F84)
        for callee in (0xF41C4, 0xF34CC, 0xDD9E4, 0xF2AC0,
                       0xCDFE8, 0xF3714, 0xF36A4, 0xF3AA8,
                       0xF3B78, 0xF34A0, 0xDDAE0, 0x3EFA0,
                       0xF3D54, 0xDC2E0, 0xDD8D0, 0xDE648,
                       0xC3834, 0xC39AC, 0xC386C, 0xEAE10,
                       0xDD870):
            with self.subTest(callee=hex(callee)):
                self.assertEqual(calls.count(callee), 1)
        self.assertEqual(calls.count(0xDC250), 2)
        for address in (0x161E22, 0x161E1C, 0x161E20, 0x161E18,
                        0x1616EE, 0x1616F0, 0x4859DC, 0x4859DE,
                        0x425FC4, 0x425FC6):
            with self.subTest(address=hex(address)):
                self.assertIn(struct.pack("<I", address), body)


if __name__ == "__main__":
    unittest.main()
