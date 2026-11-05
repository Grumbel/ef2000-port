# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Original-byte audit for the OPTIONS dispatcher, lexer and rectangle reader."""

import csv
import hashlib
import os
from pathlib import Path
import struct
import unittest


ROOT = Path(__file__).resolve().parents[1]
SHA256 = "3fd26b189efe289feb9e6ac701d592a5d48d7c1e152527aa00c01611e87e0fdb"
ENTRIES = {
    0xF3B78: (476, "process_command_dispatch_loop", "confirmed"),
    0xF4347: (367, "store_option_assignment", "unverified"),
    0xF44B6: (224, "find_or_delete_option_assignment", "unverified"),
    0xF46CC: (190, "read_next_command_token", "confirmed"),
    0xF5484: (531, "read_six_option_coordinates", "unverified"),
}


class Inventory095(unittest.TestCase):
    def test_five_original_entries_are_drafted(self):
        with (ROOT / "decomp/symbols/functions.tsv").open(newline="") as stream:
            rows = {int(row["addr"], 16): row
                    for row in csv.DictReader(stream, delimiter="\t")}
        for address, (size, name, signature_status) in ENTRIES.items():
            with self.subTest(address=hex(address)):
                row = rows[address]
                self.assertEqual(int(row["size"]), size)
                self.assertEqual(row["name"], name)
                module = ("gui_options_labels_095" if address in (0xF4347, 0xF44B6)
                          else "gui_script_pipeline_095")
                self.assertEqual(row["module"], module)
                self.assertEqual(row["state"], "drafted")
                self.assertEqual(row["sig_status"], signature_status)


class OriginalBytes095(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = os.environ.get("EF2000_PAYLOAD")
        if not source:
            raise unittest.SkipTest("set EF2000_PAYLOAD for original-byte audit")
        cls.payload = Path(source).read_bytes()
        if hashlib.sha256(cls.payload).hexdigest() != SHA256:
            raise AssertionError("unrecognized EF2000 payload")

    def body(self, entry):
        return self.payload[entry:entry + ENTRIES[entry][0]]

    def calls(self, entry):
        body = self.body(entry)
        return [entry + index + 5 + struct.unpack_from("<i", body, index + 1)[0]
                for index in range(len(body) - 4) if body[index] == 0xE8]

    def test_lexer_cursor_buffer_and_limit(self):
        body = self.body(0xF46CC)
        self.assertEqual(body[:1], b"\x53")
        for address in (0x4859E0, 0x485E07, 0x485E08, 0x161EAC):
            with self.subTest(address=hex(address)):
                self.assertIn(struct.pack("<I", address), body)
        self.assertIn(b"\x83\xf8\x50", body)  # 80-byte token bound
        self.assertIn(b"\x83\xfb\x2c", body)  # comma delimiter
        self.assertIn(b"\x83\xfb\x22", body)  # quote delimiter

    def test_six_token_parser_calls_and_output(self):
        body = self.body(0xF5484)
        calls = self.calls(0xF5484)
        self.assertEqual(body[:1], b"\x53")
        for target, count in ((0xF46CC, 9), (0x9D524, 3),
                              (0xEBD30, 12), (0x9D4CD, 6), (0xF5E6C, 1)):
            with self.subTest(callee=hex(target)):
                self.assertEqual(calls.count(target), count)
        for data in (0x191BF8, 0x192544, 0x1925AC, 0x19261C):
            self.assertIn(struct.pack("<I", data), body)

    def test_options_keywords_and_handlers(self):
        body = self.body(0xF3B78)
        calls = self.calls(0xF3B78)
        self.assertEqual(body[:1], b"\x53")
        self.assertEqual(calls.count(0xA4380), 14)
        self.assertEqual(calls.count(0xF2D74), 1)
        for address in (0x48592C, 0x4859DB, 0x4859DC, 0x4859DD,
                        0x1919F8, 0x191A04, 0x191A0C, 0x191A18,
                        0x191A28, 0x191A34, 0x191A3C, 0x191A44,
                        0x191A4C, 0x191A5C, 0x191A64, 0x191A6C,
                        0x191A74, 0x1919C8):
            with self.subTest(data=hex(address)):
                self.assertIn(struct.pack("<I", address), body)
        for handler in (0xF2DFC, 0xF2E54, 0xF2E74, 0xF2FBC,
                        0xF314C, 0xF35D8, 0xF3F84, 0xF3B1C,
                        0xF3B40, 0xF3B58):
            with self.subTest(handler=hex(handler)):
                self.assertIn(handler, calls)

    def test_option_label_table_storage_and_deletion(self):
        store = self.body(0xF4347)
        find = self.body(0xF44B6)
        store_calls = self.calls(0xF4347)
        find_calls = self.calls(0xF44B6)
        self.assertEqual(store[:1], b"\x53")
        self.assertEqual(find[:1], b"\x53")
        self.assertIn(b"\x80\xfa\x3d", store)
        self.assertIn(0xF44B6, store_calls)
        self.assertIn(0xFC795, store_calls)
        self.assertIn(0xE8651, store_calls)
        self.assertIn(0x109E6F, store_calls)
        self.assertIn(0xB3FB0, store_calls)
        self.assertEqual(find_calls.count(0xDDBC8), 2)
        self.assertIn(0xFA9D2, find_calls)
        self.assertIn(0x109E6F, find_calls)
        for body in (store, find):
            for addr in (0x160A18, 0x160A1C):
                self.assertIn(struct.pack("<I", addr), body)


if __name__ == "__main__":
    unittest.main()
