# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Inventory and opt-in original-byte audit for the C high-score bodies."""

import csv
import hashlib
import os
from pathlib import Path
import struct
import unittest


ROOT = Path(__file__).resolve().parents[1]
SHA256 = "3fd26b189efe289feb9e6ac701d592a5d48d7c1e152527aa00c01611e87e0fdb"
ENTRIES = {
    0x4050: (138, "hiscore_data_093"),
    0x40DC: (288, "hiscore_display_093"),
    0x41FC: (165, "hiscore_io_093"),
    0x42A4: (114, "hiscore_io_093"),
    0x433C: (242, "hiscore_data_093"),
}


class Inventory093(unittest.TestCase):
    def test_five_c_bodies_in_tracker(self):
        with (ROOT / "decomp/symbols/functions.tsv").open(newline="") as file:
            rows = {int(row["addr"], 16): row
                    for row in csv.DictReader(file, delimiter="\t")}
        for address, (size, module) in ENTRIES.items():
            with self.subTest(address=hex(address)):
                self.assertEqual(int(rows[address]["size"]), size)
                self.assertEqual(rows[address]["module"], module)
                self.assertEqual(rows[address]["state"], "drafted")
                self.assertEqual(rows[address]["kind"], "game")


class OriginalBytes093(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = os.environ.get("EF2000_PAYLOAD")
        if not path:
            raise unittest.SkipTest("set EF2000_PAYLOAD for original-byte audit")
        cls.payload = Path(path).read_bytes()
        if hashlib.sha256(cls.payload).hexdigest() != SHA256:
            raise AssertionError("unrecognized EF2000 payload")

    def test_entry_prologues_and_calls(self):
        for address in ENTRIES:
            with self.subTest(address=hex(address)):
                self.assertEqual(self.payload[address], 0x53)
        # Both checksum loops repeatedly read [EBX], never advance EBX.
        for address in (0x426D, 0x42D4):
            self.assertEqual(self.payload[address:address + 13],
                             bytes.fromhex("31d28b0c248a1329d140890c24")
                             if address == 0x426D else
                             bytes.fromhex("31d28b0c248a1301d140890c24"))

    def test_packed_record_offsets_and_constants(self):
        checks = {
            0x4050: (0x121292, 0x1211AD, 0x1970A4),
            0x40DC: (0x1970A4, 0x1970B8, 0x1970B9),
            0x41FC: (0x1970A4, 0x17F694),
            0x42A4: (0x1970A4, 0x17F694),
            0x433C: (0x1970A0, 0x1970B8, 0x1970B9),
        }
        for entry, globals_ in checks.items():
            body = self.payload[entry:entry + ENTRIES[entry][0]]
            for address in globals_:
                with self.subTest(entry=hex(entry), global_=hex(address)):
                    self.assertIn(struct.pack("<I", address), body)


if __name__ == "__main__":
    unittest.main()
