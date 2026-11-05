# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Inventory and optional original-byte audit for twelve READCOLS drafts.

The original EF2000 payload is supplied externally via EF2000_PAYLOAD;
this test never embeds or distributes original game bytes.
"""

import csv
import hashlib
import os
from pathlib import Path
import struct
import unittest


ROOT = Path(__file__).resolve().parents[1]
SHA256 = "3fd26b189efe289feb9e6ac701d592a5d48d7c1e152527aa00c01611e87e0fdb"
ENTRIES = {
    0xDA748: ("load_darkside_pal", 158, 0x190000, 0x421750, "environment_records_091"),
    0xDA8C4: ("set_darkside_values", 158, 0x190000, 0x425190, "environment_records_091"),
    0xDAD4C: ("set_nb_craters", 244, 0x190250, 0x4218E0, "environment_records_091"),
    0xDAE40: ("load_craters", 179, 0x18FF74, 0x425AF4, "environment_records_091"),
    0xDB28C: ("sun_flare", 157, 0x190310, 0x425BB8, "environment_sky_091"),
    0xDB654: ("sun_texture", 218, 0x190374, 0x425B20, "environment_records_091"),
    0xDB730: ("cannon_hit", 214, 0x1903A8, 0x425B8C, "environment_records_091"),
    0xDB808: ("fix_horizon", 158, 0x190004, 0x425AFC, "environment_sky_091"),
    0xDB8BC: ("heavenly_body", 160, 0x19043C, 0x422398, "environment_sky_091"),
    0xDBFF8: ("init3d_file", 153, 0x190000, 0x17D368, "environment_sky_091"),
    0xDC094: ("east_west_palettes", 143, 0x190000, 0x15BB64, "environment_sky_091"),
    0xDC124: ("setsun", 175, None, 0x420F90, "environment_sky_091"),
}


class BatchInventory091(unittest.TestCase):
    def test_all_twelve_entries_have_drafts(self):
        with (ROOT / "decomp/symbols/functions.tsv").open(newline="") as stream:
            rows = {int(row["addr"], 16): row for row in csv.DictReader(stream, delimiter="\t")}
        self.assertEqual(len(ENTRIES), 12)
        for address, (name, size, _format, _global, module) in ENTRIES.items():
            with self.subTest(address=hex(address)):
                row = rows[address]
                self.assertEqual(row["name"], name)
                self.assertEqual(int(row["size"]), size)
                self.assertEqual(row["module"], module)
                self.assertEqual(row["state"], "drafted")
                self.assertEqual(row["kind"], "game")
                self.assertEqual(row["sig_status"], "unverified")


class OriginalBytes091(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = os.environ.get("EF2000_PAYLOAD")
        if not path:
            raise unittest.SkipTest("set EF2000_PAYLOAD for original-byte audit")
        cls.payload = Path(path).read_bytes()
        if hashlib.sha256(cls.payload).hexdigest() != SHA256:
            raise AssertionError("unrecognized EF2000 payload SHA-256")

    def test_entry_scanners_and_globals(self):
        for entry, (_name, size, format_address, global_address, _module) in ENTRIES.items():
            with self.subTest(entry=hex(entry)):
                body = self.payload[entry:entry + size]
                self.assertEqual(body[:3], bytes.fromhex("535152"))
                self.assertIn(struct.pack("<I", global_address), body)
                self.assertIn(struct.pack("<I", 0x425AF8), body)
                calls = [
                    entry + i + 5 + struct.unpack_from("<i", body, i + 1)[0]
                    for i in range(len(body) - 4)
                    if body[i] == 0xE8
                ]
                if format_address is not None:
                    self.assertIn(struct.pack("<I", format_address), body)
                    self.assertIn(0xDF26F, calls)
                else:
                    # SETSUN is the non-sscanf two-part filename scanner.
                    self.assertNotIn(0xDF26F, calls)
                    self.assertEqual(calls.count(0xE7CEC), 2)
                    self.assertIn(struct.pack("<I", 0x421290), body)

    def test_record_boundaries_and_guest_calls(self):
        expected = {
            0xDAD4C: (0x425BBC, 0x190218),
            0xDAE40: (0x425AEC, 0x425AE8),
            0xDB28C: (0x425BA2, 0x425BA4),
            0xDB654: (0x15BB60, 0x190388),
            0xDB730: (0x425B64, 0x1903BC),
            0xDB808: (0x190400, 0x425AFC),
            0xDB8BC: (0x425C86, 0x422398),
            0xDBFF8: (0x1904D4, 0x190504),
            0xDC094: (0x19050C, 0x15BB64),
        }
        for entry, addresses in expected.items():
            body = self.payload[entry:entry + ENTRIES[entry][1]]
            for address in addresses:
                with self.subTest(entry=hex(entry), global_address=hex(address)):
                    self.assertIn(struct.pack("<I", address), body)


if __name__ == "__main__":
    unittest.main()
