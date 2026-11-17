# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Generated-state DOS services use the real copy-on-write drive."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from tools import asm_to_c


@unittest.skipUnless(shutil.which('c++'), 'requires C++ compiler')
class DosFileServiceTests(unittest.TestCase):
    def test_real_drive_and_register_contract(self):
        source = Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root/'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
            binary = root/'test'
            subprocess.run(['c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-Wno-unused-function', '-I'+str(root), '-I'+str(source/'runtime'),
                            str(source/'tests/asm_dos_file_service_test.cpp'),
                            str(source/'runtime/asm_dos_file_service.cpp'),
                            str(source/'runtime/dos_drive.cpp'), '-o', str(binary)], check=True)
            subprocess.run([str(binary), str(root/'drive')], check=True)
