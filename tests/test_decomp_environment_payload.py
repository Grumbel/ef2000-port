# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
"""Optional byte-source audit for the twenty batch-087 original entries.

This tests disassembly *facts*, without copying any original instruction
bytes into the repository. Use EF2000_PAYLOAD=.../payload.bin to opt in.
"""

import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import unittest


PAYLOAD_SHA256 = "3fd26b189efe289feb9e6ac701d592a5d48d7c1e152527aa00c01611e87e0fdb"
PARSERS = {
    0xDB95C: 0x425BDC, 0xDB9A8: 0x425BE0, 0xDB9F4: 0x425BDA,
    0xDBA40: 0x425BDE, 0xDBA8C: 0x425BEA, 0xDBAD8: 0x425BEE,
    0xDBB24: 0x425BD4, 0xDBB70: 0x425BE6, 0xDBBBC: 0x425BEC,
    0xDBC08: 0x425BD6, 0xDBC54: 0x425BD8, 0xDBCA0: 0x425BD2,
    0xDBCEC: 0x425BE2, 0xDBD38: 0x425BE8, 0xDBD84: 0x425BCE,
}
FLAGS = (0xDB8A8, 0xDB8B0, 0xDBE28, 0xDBE30, 0xDBE54)


@unittest.skipUnless(os.environ.get("EF2000_PAYLOAD"), "set EF2000_PAYLOAD to raw X-32 payload")
class OriginalEnvironmentEntriesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if shutil.which("objdump") is None:
            raise unittest.SkipTest("objdump missing")
        cls.path = Path(os.environ["EF2000_PAYLOAD"])
        if hashlib.sha256(cls.path.read_bytes()).hexdigest() != PAYLOAD_SHA256:
            raise AssertionError("payload SHA-256 does not match pinned German build")

    def disassemble(self, address, size):
        result = subprocess.run(
            ["objdump", "-D", "-b", "binary", "-m", "i386", "-M", "intel",
             f"--start-address=0x{address:x}", f"--stop-address=0x{address+size:x}",
             str(self.path)], capture_output=True, text=True, check=True)
        return result.stdout

    def test_15_parser_boundaries_and_destinations(self):
        for entry, destination in PARSERS.items():
            with self.subTest(entry=hex(entry)):
                body = self.disassemble(entry, 74)
                self.assertIn(f"push   0x{destination:x}", body)
                self.assertIn("push   0x18fffc", body)
                self.assertRegex(body, r"call\s+0xdf26f")
                self.assertRegex(body, r"mov\s+DWORD PTR ds:0x425af8,ebx")
                self.assertRegex(body, r"mov\s+BYTE PTR \[ebx\],0x0")
                self.assertRegex(body, r"mov\s+BYTE PTR \[ebx-0x2\],0x0")
                self.assertRegex(body, r"ret\s*$")

    def test_flag_boundaries(self):
        self.assertEqual(len(PARSERS) + len(FLAGS), 20)
        checks = {
            0xDB8A8: (8, "mov    BYTE PTR ds:0x425c86,0x1"),
            0xDB8B0: (9, "mov    BYTE PTR ds:0x425c86,ah"),
            0xDBE28: (8, "or     BYTE PTR ds:0x425bc4,0x2"),
            0xDBE30: (33, "or     ah,0x4"),
            0xDBE54: (8, "or     BYTE PTR ds:0x425bc4,0x42"),
        }
        for entry, (size, operation) in checks.items():
            with self.subTest(entry=hex(entry)):
                body = self.disassemble(entry, size)
                self.assertIn(operation, body)
                self.assertTrue(re.search(r"ret\s*$", body))


if __name__ == "__main__":
    unittest.main()
