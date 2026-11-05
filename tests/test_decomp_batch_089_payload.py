# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
"""Audit original five-byte dispatch jumps against pinned X-32 bytes.

This consumes an external payload and commits no original executable data.
"""

import csv
import hashlib
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import unittest


ROOT = Path(__file__).resolve().parent.parent
PAYLOAD_SHA = "3fd26b189efe289feb9e6ac701d592a5d48d7c1e152527aa00c01611e87e0fdb"
JUMP_PATTERN = re.compile(r"\bX\(\s*(\w+)\s*,\s*([0-9a-f]{5})\s*,\s*([0-9a-f]{4,5})\s*\)")

ADDITIONAL_BODIES = {
    0x4B54: (39, ("call   0xa4b80", "call   0x432c")),
    0x4B1A4: (75, ("call   0x50770", "call   0x4f388")),
    0x4D9CC: (95, ("mov    BYTE PTR ds:0x1a5598,dl", "call   0x4db60")),
    0x4DA2C: (99, ("cmp    eax,0xfe", "call   0x4db60")),
    0x4DA90: (85, ("call   0x4d9cc", "call   0x4db60")),
    0x50638: (15, ("call   0xac9e8", "jmp    0xa4db0")),
    0x5089C: (96, ("call   0x50a1c", "call   0x50b68")),
    0x508FC: (96, ("call   0x50a1c", "call   0x50b68")),
    0x5095C: (96, ("call   0x50a1c", "call   0x50b68")),
    0x84DDC: (13, ("and    eax,0xff", "jmp    0xa4b80")),
    0xDA2B0: (8, ("mov    BYTE PTR ds:0x425c88,0x1",)),
    0x37260: (51, ("jmp    0xb1e0",)),
    0x37254: (12, ("mov    BYTE PTR ds:0x125846,0x1",)),
}


def mapping():
    header = (ROOT / "decomp/include/ef_ui_thunks_089.h").read_text()
    return [(name, int(address, 16), int(target, 16))
            for name, address, target in JUMP_PATTERN.findall(header)]


class ThunkTableStructureTest(unittest.TestCase):
    def test_48_tracked_named_entries(self):
        expected = mapping()
        self.assertEqual(len(expected), 48)
        self.assertEqual(len({address for _, address, _ in expected}), 48)
        with (ROOT / "decomp/symbols/functions.tsv").open(newline="") as stream:
            tracked = {int(row["addr"], 16): row
                       for row in csv.DictReader(stream, delimiter="\t")}
        for name, address, target in expected:
            with self.subTest(address=hex(address)):
                self.assertEqual(tracked[address]["size"], "5")
                self.assertEqual(tracked[address]["name"], name)
                self.assertNotEqual(address, target)
        # An adjacent, still-uncatalogued JMP should not silently be counted.
        self.assertNotIn(0x84C4C, {address for _, address, _ in expected})


@unittest.skipUnless(os.environ.get("EF2000_PAYLOAD"), "EF2000_PAYLOAD not set")
class OriginalThunkBytesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        payload = Path(os.environ["EF2000_PAYLOAD"]).read_bytes()
        if hashlib.sha256(payload).hexdigest() != PAYLOAD_SHA:
            raise AssertionError("original payload hash does not match")
        cls.payload = payload

    def test_48_relative_jump_targets(self):
        for name, address, target in mapping():
            with self.subTest(name=name, address=hex(address)):
                self.assertEqual(self.payload[address], 0xE9)
                displacement, = struct.unpack_from("<i", self.payload, address + 1)
                self.assertEqual(address + 5 + displacement, target)

    def test_two_quit_wrapper_calls_and_tail_jumps(self):
        # Both 15-byte bodies have one direct near call and one final JMP.
        for address, first, last in ((0x84AC4, 0xA4D90, 0xAC9A0),
                                     (0x84AEC, 0x830B8, 0xAC7C8)):
            with self.subTest(address=hex(address)):
                call_at = address + (5 if address == 0x84AC4 else 0)
                jump_at = address + 10
                self.assertEqual(self.payload[call_at], 0xE8)
                rel_call, = struct.unpack_from("<i", self.payload, call_at + 1)
                self.assertEqual(call_at + 5 + rel_call, first)
                self.assertEqual(self.payload[jump_at], 0xE9)
                rel_jump, = struct.unpack_from("<i", self.payload, jump_at + 1)
                self.assertEqual(jump_at + 5 + rel_jump, last)

    def test_13_additional_entry_bodies(self):
        if shutil.which("objdump") is None:
            self.skipTest("objdump unavailable")
        self.assertEqual(len(ADDITIONAL_BODIES), 13)
        for address, (size, operations) in ADDITIONAL_BODIES.items():
            with self.subTest(address=hex(address)):
                completed = subprocess.run(
                    ["objdump", "-D", "-b", "binary", "-m", "i386", "-M", "intel",
                     f"--start-address={address}", f"--stop-address={address+size}",
                     os.environ["EF2000_PAYLOAD"]], capture_output=True,
                    text=True, check=True)
                for operation in operations:
                    self.assertIn(operation, completed.stdout)


if __name__ == "__main__":
    unittest.main()
