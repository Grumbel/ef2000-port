# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Seven GUI script handler declarations and SHA-pinned original-byte checks."""

import csv
import hashlib
import os
from pathlib import Path
import struct
import unittest


ROOT = Path(__file__).resolve().parents[1]
SHA256 = "3fd26b189efe289feb9e6ac701d592a5d48d7c1e152527aa00c01611e87e0fdb"
ENTRIES = {
    0xF4DB8: (281, "SIZE", "gui_script_properties_094"),
    0xF4ED4: (281, "FROM", "gui_script_properties_094"),
    0xF4FF0: (249, "BACKDROP", "gui_script_properties_094"),
    0xF50EC: (145, "ACTION", "gui_script_properties_094"),
    0xF5180: (145, "POINTER", "gui_script_properties_094"),
    0xF5214: (423, "GADGETRY", "gui_script_assets_094"),
    0xF53BC: (200, "NAME", "gui_script_assets_094"),
}


class Inventory094(unittest.TestCase):
    def test_seven_original_entries_are_drafted(self):
        with (ROOT / "decomp/symbols/functions.tsv").open(newline="") as stream:
            rows = {int(row["addr"], 16): row
                    for row in csv.DictReader(stream, delimiter="\t")}
        for address, (size, name, module) in ENTRIES.items():
            with self.subTest(address=hex(address)):
                row = rows[address]
                self.assertEqual(int(row["size"]), size)
                self.assertEqual(row["name"], name)
                self.assertEqual(row["module"], module)
                self.assertEqual(row["state"], "drafted")
                self.assertEqual(row["kind"], "game")
                self.assertEqual(row["sig_status"], "unverified")


class OriginalBytes094(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = os.environ.get("EF2000_PAYLOAD")
        if not path:
            raise unittest.SkipTest("set EF2000_PAYLOAD for original-byte audit")
        cls.payload = Path(path).read_bytes()
        if hashlib.sha256(cls.payload).hexdigest() != SHA256:
            raise AssertionError("unrecognized EF2000 payload")

    def body(self, entry):
        return self.payload[entry:entry + ENTRIES[entry][0]]

    def calls(self, entry):
        body = self.body(entry)
        return [entry + offset + 5 + struct.unpack_from("<i", body, offset + 1)[0]
                for offset in range(len(body) - 4) if body[offset] == 0xE8]

    def test_entry_boundaries_and_shared_parser_calls(self):
        for address in ENTRIES:
            with self.subTest(address=hex(address)):
                body = self.body(address)
                self.assertEqual(body[:1], b"\x53")
                self.assertIn(struct.pack("<I", 0x161EA8), body)
                self.assertIn(0xF46CC, self.calls(address))

        for entry in (0xF4DB8, 0xF4ED4, 0xF4FF0):
            with self.subTest(entry=hex(entry)):
                calls = self.calls(entry)
                self.assertEqual(calls.count(0xF46CC), 2)
                self.assertEqual(calls.count(0xEBD30), 3)
                self.assertEqual(calls.count(0x9D4CD), 2)
                self.assertIn(struct.pack("<I", 0x485DFC), self.body(entry))

    def test_text_and_asset_dispatch(self):
        for entry, offset, flag in ((0xF50EC, 0x08, 0x485F76),
                                    (0xF5180, 0x29, 0x485F78)):
            with self.subTest(entry=hex(entry)):
                body = self.body(entry)
                self.assertIn(bytes((0x83, 0xC0, offset)), body)
                self.assertIn(struct.pack("<I", flag), body)
                self.assertIn(struct.pack("<I", 0x485F78), body)
                self.assertIn(0x9D749, self.calls(entry))

        for entry, selector, label in ((0xF5214, 0xC3974, 0x485F5A),
                                       (0xF53BC, 0xC3980, 0x485F63)):
            with self.subTest(entry=hex(entry)):
                calls = self.calls(entry)
                self.assertIn(struct.pack("<I", label), self.body(entry))
                self.assertEqual(calls.count(0x9CA57), 3)
                for callee in (0x9C980, selector, 0xC34CC, 0xC39AC):
                    self.assertIn(callee, calls)

    def test_rgb_max_min_loop(self):
        body = self.body(0xF5214)
        for address in (0x425C8C, 0x425C8D, 0x425C8E,
                        0x485DFE, 0x485E00, 0x485E02):
            with self.subTest(address=hex(address)):
                self.assertIn(struct.pack("<I", address), body)
        self.assertIn(bytes.fromhex("81fa00010000"), body)


if __name__ == "__main__":
    unittest.main()
