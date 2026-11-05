# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Audit 20 drafted READCOLS function entries against the pinned payload.

The exact game byte sequence is never committed. The asset-free inventory
portion always runs; the original-byte checks need EF2000_PAYLOAD.
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
    0xDA1B4: ("darkside_fade", 112, 0x18FF6C),
    0xDA224: ("coltab", 138, 0x18FF74),
    0xDA2B8: ("altitude", 168, 0x18FFE8),
    0xDA360: ("altitude2", 167, 0x18FFF0),
    0xDA408: ("ground", 167, 0x18FFF0),
    0xDA4B0: ("ceiling", 102, 0x18FFFC),
    0xDA518: ("planet_number", 102, 0x18FFFC),
    0xDA580: ("game_type", 74, 0x18FFFC),
    0xDA7FC: ("load_missile_pal", 197, 0x190000),
    0xDA964: ("set_world_name", 112, 0x1900DC),
    0xDA9D4: ("set_xmap_name", 136, 0x190000),
    0xDAA5C: ("set_mmap_name", 225, 0x190120),
    0xDAB40: ("ref_point", 136, 0x190000),
    0xDABC8: ("set_map_name", 97, 0x190000),
    0xDB0B4: ("load_player_virtual", 93, 0x190000),
    0xDB114: ("load_player_sort", 85, 0x190000),
    0xDB16C: ("load_player", 85, 0x190000),
    0xDB1C4: ("set_nb_flak", 85, 0x190000),
    0xDB21C: ("load_flak", 110, 0x1902D8),
    0xDB420: ("cloud_fade", 179, 0x19034C),
}


class BatchInventory090(unittest.TestCase):
    def test_exact_entries_are_drafted(self):
        with (ROOT / "decomp/symbols/functions.tsv").open(newline="") as f:
            rows = {int(row["addr"], 16): row for row in csv.DictReader(f, delimiter="\t")}
        self.assertEqual(len(ENTRIES), 20)
        for address, (name, size, _format) in ENTRIES.items():
            with self.subTest(address=hex(address)):
                row = rows[address]
                self.assertEqual(row["size"], str(size))
                self.assertEqual(row["name"], name)
                self.assertEqual(row["module"], "environment_values_090" if address <= 0xDA580 else "environment_world_090")
                self.assertEqual(row["state"], "drafted")
                self.assertEqual(row["kind"], "game")


class OriginalBytes090(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        name = os.environ.get("EF2000_PAYLOAD")
        if not name:
            raise unittest.SkipTest("set EF2000_PAYLOAD for original-byte audit")
        cls.payload = Path(name).read_bytes()
        if hashlib.sha256(cls.payload).hexdigest() != SHA256:
            raise AssertionError("unrecognized EF2000 payload SHA-256")

    def test_scanner_calls_and_format_offsets(self):
        for address, (_name, size, format_address) in ENTRIES.items():
            with self.subTest(address=hex(address)):
                body = self.payload[address:address + size]
                self.assertEqual(body[:3], bytes.fromhex("535152"))
                # The scanned format is either PUSH imm32 or equivalent data
                # move, always including the little-endian format pointer.
                self.assertIn(struct.pack("<I", format_address), body)
                self.assertIn(struct.pack("<I", 0x425AF8), body)
                # Recover CALL rel32 -> 0xdf26f without committing the bytes.
                calls = [
                    address + i + 5 + struct.unpack_from("<i", body, i + 1)[0]
                    for i in range(len(body) - 4)
                    if body[i] == 0xE8
                ]
                self.assertIn(0xDF26F, calls)

    def test_verified_store_addresses(self):
        expected = {
            0xDA1B4: 0x12AFD8, 0xDA224: 0x12AFD8,
            0xDA2B8: 0x425B18, 0xDA360: 0x425BD0,
            0xDA408: 0x425BAE, 0xDA4B0: 0x425BBE,
            0xDA518: 0x425BC0, 0xDA580: 0x425BE4,
            0xDA7FC: 0x425BA0, 0xDA964: 0x425BC6,
            0xDA9D4: 0x425BF0, 0xDAA5C: 0x425BF0,
            0xDAB40: 0x425C6D, 0xDABC8: 0x425B28,
            0xDB0B4: 0x15BB5C, 0xDB114: 0x425AF8,
            0xDB16C: 0x425AF8, 0xDB1C4: 0x425AF8,
            0xDB21C: 0x425AE4, 0xDB420: 0x15BB60,
        }
        for address, target in expected.items():
            with self.subTest(address=hex(address)):
                size = ENTRIES[address][1]
                self.assertIn(struct.pack("<I", target), self.payload[address:address + size])


if __name__ == "__main__":
    unittest.main()
