# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Asset-free, fail-closed tests of diagnostic ELF32-i386 C in-place patches."""

from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest

from tools import elf_inplace
from tools.efpatch import decode


def fixture(*, name='sample', symbol='g_fixture_a', relocation_type=1):
    """Minimal ET_REL with one code function, one R_386_32 and no private data."""
    text = b'\xa1\0\0\0\0\xc3'  # mov eax,[absolute]; ret
    names = b'\0.text\0.rel.text\0.symtab\0.strtab\0.shstrtab\0'
    strings = b'\0' + name.encode() + b'\0' + symbol.encode() + b'\0'
    symbol_offset = 1 + len(name) + 1
    symtab = (bytes(elf_inplace.SYMBOL.size) +
              elf_inplace.SYMBOL.pack(1, 0, len(text), 0x12, 0, 1) +
              elf_inplace.SYMBOL.pack(symbol_offset, 0, 0, 0x10, 0, 0))
    reloc = elf_inplace.RELOC.pack(1, (2 << 8) | relocation_type)
    chunks = [text, reloc, symtab, strings, names]
    out = bytearray(bytes(elf_inplace.HEADER.size))
    offsets = []
    for chunk in chunks:
        out.extend(bytes((-len(out)) % 4))
        offsets.append(len(out))
        out.extend(chunk)
    out.extend(bytes((-len(out)) % 4))
    shoff = len(out)
    out.extend(bytes(elf_inplace.SECTION.size))
    sections = [
        (1, 1, 6, 0, offsets[0], len(text), 0, 0, 1, 0),
        (7, 9, 0, 0, offsets[1], len(reloc), 3, 1, 4, 8),
        (17, 2, 0, 0, offsets[2], len(symtab), 4, 1, 4, 16),
        (25, 3, 0, 0, offsets[3], len(strings), 0, 0, 1, 0),
        (33, 3, 0, 0, offsets[4], len(names), 0, 0, 1, 0),
    ]
    for section in sections:
        out.extend(elf_inplace.SECTION.pack(*section))
    ident = b'\x7fELF\x01\x01\x01' + bytes(9)
    elf_inplace.HEADER.pack_into(out, 0, ident, 1, 3, 1, 0, 0, shoff, 0,
                                 52, 0, 0, 40, 6, 5)
    return bytes(out)


def tracker(name='sample', size=6, *, notes=''):
    functions = [{'name': name, 'addr': '0x00000100', 'size': str(size),
                  'sig_status': 'confirmed', 'state': 'compiles', 'notes': notes}]
    globals_ = [{'name': 'g_fixture_a', 'addr': '0x00000500'}]
    return functions, globals_


class ElfInplaceTests(unittest.TestCase):
    def test_synthetic_absolute_external_relocation_and_patch_identity(self):
        functions, globals_ = tracker()
        image = bytes(0x2000)
        blob, code = elf_inplace.build(fixture(), image, functions, globals_, 0x100)
        self.assertEqual(code, b'\xa1\x00\x05\x00\x00\xc3')
        self.assertEqual(decode(blob, image), [(0x100, bytes(code))])
        with self.assertRaisesRegex(ValueError, 'different initialized payload'):
            decode(blob, b'x' * len(image))

    def test_rejects_unreviewed_body_and_size(self):
        image = bytes(0x2000)
        functions, globals_ = tracker()
        with self.assertRaisesRegex(ValueError, 'explicitly reviewed'):
            elf_inplace.build(fixture(), image, functions, globals_, 0x101)
        with self.assertRaisesRegex(ValueError, 'reviewed original extent'):
            elf_inplace.build(fixture(), image, tracker(size=7)[0], globals_, 0x100)
        with self.assertRaisesRegex(ValueError, 'contiguous'):
            elf_inplace.build(fixture(), image, tracker(notes='noncontiguous')[0],
                              globals_, 0x100)
        functions[0]['sig_status'] = 'unverified'
        with self.assertRaisesRegex(ValueError, 'reviewed'):
            elf_inplace.build(fixture(), image, functions, globals_, 0x100)

    def test_rejects_unknown_symbol_unsupported_relocation_and_bad_elf(self):
        image = bytes(0x2000)
        functions, globals_ = tracker()
        with self.assertRaisesRegex(ValueError, 'canonical addresses'):
            elf_inplace.build(fixture(symbol='unknown'), image, functions, globals_, 0x100)
        with self.assertRaisesRegex(ValueError, 'R_386_32'):
            elf_inplace.build(fixture(relocation_type=2), image, functions, globals_, 0x100)
        with self.assertRaisesRegex(ValueError, 'truncated ELF header'):
            elf_inplace.read_elf32(b'\x7fELF')
        invalid = bytearray(fixture())
        invalid[18] = 0x28  # e_machine != EM_386
        with self.assertRaisesRegex(ValueError, 'ELF32 i386'):
            elf_inplace.read_elf32(bytes(invalid))

    def test_real_gcc_32bit_object_when_toolchain_is_available(self):
        compiler = shutil.which('gcc')
        if not compiler:
            self.skipTest('gcc unavailable')
        source = 'extern int g_fixture_a; unsigned sample(void) { return g_fixture_a != 0; }\n'
        with tempfile.TemporaryDirectory() as directory:
            obj = Path(directory) / 'sample.o'
            result = subprocess.run(
                [compiler, '-m32', '-std=c99', '-O2', '-fno-pie', '-fno-pic',
                 '-ffreestanding', '-x', 'c', '-c', '-o', str(obj), '-'],
                input=source, text=True, capture_output=True, check=False)
            if result.returncode:
                self.skipTest('GCC -m32 not supported by local compiler')
            object_bytes = obj.read_bytes()
            name, code, relocs = elf_inplace.read_elf32(object_bytes)
        self.assertEqual(name, 'sample')
        self.assertTrue(code)
        self.assertEqual({symbol[0] for _, _, symbol in relocs}, {'g_fixture_a'})
        self.assertEqual({kind for _, kind, _ in relocs}, {elf_inplace.R_386_32})
        functions, globals_ = tracker(size=len(code))
        patch, _ = elf_inplace.build(object_bytes, bytes(0x2000),
                                     functions, globals_, 0x100)
        self.assertEqual(decode(patch, bytes(0x2000))[0][0], 0x100)


if __name__ == '__main__':
    unittest.main()
