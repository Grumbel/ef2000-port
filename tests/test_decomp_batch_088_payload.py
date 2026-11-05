# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
"""Optional original-payload disassembly audit for decomp batch 088.

The original executable is neither shipped nor embedded. Set EF2000_PAYLOAD
to the extracted X-32 payload of the pinned German EF2000 build.
"""

import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import unittest


SHA256 = "3fd26b189efe289feb9e6ac701d592a5d48d7c1e152527aa00c01611e87e0fdb"
# Original inventory sizes, not the size of the host C implementations.
MODE = {
    0xDBDD0: (88, ("mov    DWORD PTR ds:0x425af0,ebx", "call   0xa4342")),
    0xDBE5C: (33, ("or     ah,0x8", "or     dl,0x84")),
    0xDBE80: (41, ("or     dl,0x84", "mov    BYTE PTR ds:0x425c89,dh")),
    0xDBEAC: (46, ("or     edx,0x108", "or     ah,0x80")),
    0xDBEDC: (8, ("or     BYTE PTR ds:0x425bc4,0x10",)),
    0xDBEE4: (8, ("or     BYTE PTR ds:0x425bc4,0x1",)),
    0xDBEEC: (9, ("or     BYTE PTR ds:0x425bc4,0x22",)),
    0xDBEF8: (41, ("mov    BYTE PTR ds:0x15bb5f,dl", "or     dh,0x8")),
}
GUI = {
    0x4318: (17, ("call   0x41fc", "call   0x40dc")),
    0x4CF4: (86, ("mov    ds:0x121e46,ax", "call   0x7c4fc")),
    0xA960: (19, ("jmp    0x4595c",)),
    0xA974: (49, ("sub    dx,bx", "call   0xa7588")),
    0xB574: (89, ("mov    BYTE PTR ds:0x121e95,0x2", "call   0xa5ef8")),
    0xBDE8: (93, ("mov    eax,0x1808f4", "jmp    0xa4b80")),
    0xBE48: (67, ("call   0x779f4", "call   0xbc38")),
    0xBE8C: (73, ("mov    edx,0x1", "call   0xbc38")),
    0xBED8: (75, ("mov    edx,0x2", "call   0xbc38")),
    0xBF24: (75, ("mov    edx,0x3", "call   0xbc38")),
    0xC838: (48, ("inc    edx", "call   0xc868")),
    0xCA40: (25, ("mov    ds:0x1a4ba2,ax", "mov    ds:0x1a4ba0,ax")),
}


@unittest.skipUnless(os.environ.get("EF2000_PAYLOAD"), "EF2000_PAYLOAD not set")
class OriginalBatch088Test(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if shutil.which("objdump") is None:
            raise unittest.SkipTest("objdump unavailable")
        cls.path = Path(os.environ["EF2000_PAYLOAD"])
        if hashlib.sha256(cls.path.read_bytes()).hexdigest() != SHA256:
            raise AssertionError("EF2000 payload does not match pinned build")

    def disassemble(self, entry, size):
        output = subprocess.run(
            ["objdump", "-D", "-b", "binary", "-m", "i386", "-M", "intel",
             f"--start-address={entry}", f"--stop-address={entry+size}",
             str(self.path)], capture_output=True, text=True, check=True)
        return output.stdout

    def test_20_original_boundaries_and_operations(self):
        self.assertEqual(len(MODE), 8)
        self.assertEqual(len(GUI), 12)
        for entry, (size, operations) in (MODE | GUI).items():
            with self.subTest(entry=hex(entry)):
                body = self.disassemble(entry, size)
                for operation in operations:
                    self.assertIn(operation, body)
                # The two terminal-jump bodies and the trailing GUI callback
                # are deliberately not assumed to end in a RET instruction.
                self.assertRegex(body, rf"(?m)^\s*{entry:x}:\s")

    def test_double_compare_is_in_original(self):
        body = self.disassemble(0xB574, 89)
        self.assertEqual(len(re.findall(r"call\s+0xa4380", body)), 2)
        self.assertEqual(len(re.findall(r"mov\s+edx,0x180834", body)), 2)


if __name__ == "__main__":
    unittest.main()
