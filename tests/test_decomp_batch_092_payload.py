# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Inventory and opt-in original-byte audit for the READCOLS tail.

Requires the SHA-pinned original payload supplied via EF2000_PAYLOAD for
disassembly checks; no original game bytes are embedded in these tests.
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
    0xDA5CC: ("load_pal", 119, 0x190000, "environment_palette_092"),
    0xDA644: ("load_cock_pal", 258, 0x190004, "environment_palette_092"),
    0xDAC2C: ("lightning_point", 286, 0x1901E4, "environment_dynamic_092"),
    0xDAEF4: ("load_smoke", 448, 0x190004, "environment_dynamic_092"),
}


class Inventory092(unittest.TestCase):
    def test_all_entries_are_drafts(self):
        with (ROOT / "decomp/symbols/functions.tsv").open(newline="") as file:
            rows = {int(row["addr"], 16): row for row in csv.DictReader(file, delimiter="\t")}
        self.assertEqual(len(ENTRIES), 4)
        for address, (name, size, _format, module) in ENTRIES.items():
            with self.subTest(address=hex(address)):
                row = rows[address]
                self.assertEqual(row["name"], name)
                self.assertEqual(int(row["size"]), size)
                self.assertEqual(row["module"], module)
                self.assertEqual(row["state"], "drafted")
                self.assertEqual(row["kind"], "game")
                self.assertEqual(row["sig_status"], "unverified")


class OriginalBytes092(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = os.environ.get("EF2000_PAYLOAD")
        if not path:
            raise unittest.SkipTest("set EF2000_PAYLOAD for original-byte audit")
        cls.payload = Path(path).read_bytes()
        if hashlib.sha256(cls.payload).hexdigest() != SHA256:
            raise AssertionError("unrecognized EF2000 payload SHA-256")

    def test_scanner_addresses(self):
        for address, (_name, size, format_address, _module) in ENTRIES.items():
            with self.subTest(address=hex(address)):
                body = self.payload[address:address + size]
                self.assertEqual(body[:3], bytes.fromhex("535152"))
                self.assertIn(struct.pack("<I", format_address), body)
                self.assertIn(struct.pack("<I", 0x425AF8), body)
                calls = [
                    address + i + 5 + struct.unpack_from("<i", body, i + 1)[0]
                    for i in range(len(body) - 4)
                    if body[i] == 0xE8
                ]
                self.assertIn(0xDF26F, calls)

    def test_palette_table_and_fallback(self):
        table = self.payload[0x15BD40:0x15BDB8]
        self.assertEqual(len(table), 15 * 8)
        pointers = [struct.unpack_from("<II", table, i * 8) for i in range(15)]
        self.assertEqual(pointers[-1], (0x18FE48, 0xDBF2C))
        # The callback at 15bdb4 stays valid when the loader writes its
        # input-name pointer over the descriptor key at 15bdb0.
        body = self.payload[0xDA5CC:0xDA5CC + 119]
        self.assertIn(struct.pack("<I", 0x15BDB0), body)
        self.assertIn(struct.pack("<I", 0x15BD40), body)
        self.assertTrue(all(0x18FE00 <= key < 0x18FF00 for key, _ in pointers))

    def test_record_sizes_and_links(self):
        expected = {
            0xDA644: [0x425BCA, 0x422760, 0x433F30, 0x424E60],
            0xDAC2C: [0x421A48, 0x425C87, 0x1901F4],
            0xDAEF4: [0x425AE0, 0x425AE4, 0x425AE8, 0x425AF4,
                      0x425AEC, 0x425B88, 0x425B24],
        }
        for address, refs in expected.items():
            body = self.payload[address:address + ENTRIES[address][1]]
            for ref in refs:
                with self.subTest(address=hex(address), global_address=hex(ref)):
                    self.assertIn(struct.pack("<I", ref), body)


if __name__ == "__main__":
    unittest.main()
