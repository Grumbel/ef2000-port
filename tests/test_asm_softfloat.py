# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Pinned dependency checks and opt-in independent raw-80 arithmetic comparisons."""

import ctypes
import itertools
import json
import os
import platform
import subprocess
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch
import zipfile

from tools import asm_softfloat
from tests.test_asm_to_c import assemble, unicorn


class SoftFloatBuildTests(unittest.TestCase):
    def test_dependency_validation_rejects_profile_paths_and_corruption_before_build(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            sources = []
            for index in range(237):
                source = root / ('source_%d.c' % index)
                source.write_text('/* synthetic dependency inventory */')
                sources.append(dict(source=source.name, source_sha256=asm_softfloat.digest(source)))
            for name in ('platform.h', 'softfloat.so', 'types.h'):
                (root / name).write_bytes(b'synthetic validation fixture')
            report = dict(schema=1, archive_sha256=asm_softfloat.ARCHIVE_SHA256,
                          specialization='8086', fast_int64=False, inline_level=0, round_odd=True,
                          platform_sha256=asm_softfloat.digest(root/'platform.h'),
                          library_sha256=asm_softfloat.digest(root/'softfloat.so'),
                          headers={'types.h': asm_softfloat.digest(root/'types.h')}, sources=sources)
            (root/'report.json').write_text(json.dumps(report))
            self.assertEqual(asm_softfloat.validated_build(root), report)
            for change in ('header', 'path', 'profile', 'duplicate', 'binding', 'watcom'):
                with self.subTest(change=change):
                    current = json.loads(json.dumps(report))
                    if change == 'header':
                        current['headers']['types.h'] = '0'*64
                    elif change == 'path':
                        current['headers']['../outside'] = '0'*64
                    elif change == 'profile':
                        current['specialization'] = 'ARM-VFPv2'
                    elif change == 'duplicate':
                        current['sources'][-1] = current['sources'][0]
                    elif change == 'binding':
                        current['headers']['platform.h'] = current['platform_sha256']
                    (root/'report.json').write_text(json.dumps(current))
                    for builder in (asm_softfloat.build_narrow, asm_softfloat.build_binary):
                        with patch.object(asm_softfloat.subprocess, 'run') as compile_:
                            with self.assertRaises(ValueError):
                                builder(root, root/'out', watcom='wcc' if change == 'watcom' else None)
                            compile_.assert_not_called()
                        self.assertFalse((root/'out').exists())

    def test_bad_digest_creates_no_build(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = root / 'bad.zip'
            archive.write_bytes(b'not the pinned dependency')
            with self.assertRaisesRegex(ValueError, 'digest mismatch'):
                asm_softfloat.build(archive, root / 'build')
            self.assertFalse((root / 'build').exists())

    def test_archive_paths_and_links_are_rejected(self):
        for name, symlink in [('SoftFloat-3e/../../outside', False),
                              ('/outside', False), ('SoftFloat-3e/link', True)]:
            with self.subTest(name=name), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                archive = root / 'fixture.zip'
                with zipfile.ZipFile(archive, 'w') as package:
                    info = zipfile.ZipInfo(name)
                    if symlink:
                        info.external_attr = 0o120777 << 16
                    package.writestr(info, 'outside')
                with patch.object(asm_softfloat, 'ARCHIVE_SHA256', asm_softfloat.digest(archive)):
                    with self.assertRaisesRegex(ValueError, 'unsafe'):
                        asm_softfloat.extract(archive, root / 'build')
                self.assertFalse((root / 'outside').exists())

    def test_label_repair_retains_statements_and_refuses_referenced_labels(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / 'unit.c'
            source.write_text('/* license */\n roundPack:\n    call();\n')
            asm_softfloat.remove_unused_label(source)
            self.assertEqual(source.read_text(), '/* license */\n    call();\n')
            for text in ('goto roundPack;\n roundPack:\n call();\n', 'call();\n'):
                source.write_text(text)
                with self.assertRaisesRegex(ValueError, 'unexpected'):
                    asm_softfloat.remove_unused_label(source)
                self.assertEqual(source.read_text(), text)

    @unittest.skipUnless(unicorn and os.environ.get('ASM_SOFTFLOAT_BUILD') and
                         shutil.which('as') and shutil.which('objcopy') and shutil.which('cc') and
                         platform.machine() == 'x86_64',
                         'requires x86-64, Unicorn, binutils, cc and ASM_SOFTFLOAT_BUILD')
    def test_raw80_arithmetic_precision_rounding_and_exceptions(self):
        directory = Path(os.environ['ASM_SOFTFLOAT_BUILD']).resolve()
        report = json.loads((directory / 'report.json').read_text())
        self.assertEqual(report['archive_sha256'], asm_softfloat.ARCHIVE_SHA256)
        self.assertEqual(asm_softfloat.digest(directory / 'softfloat.so'), report['library_sha256'])
        library = ctypes.CDLL(str(directory / 'softfloat.so'))

        class Extended(ctypes.Structure):
            _fields_ = [('signif', ctypes.c_uint64), ('signExp', ctypes.c_uint16)]

        rounding = ctypes.c_uint8.in_dll(library, 'softfloat_roundingMode')
        precision = ctypes.c_uint8.in_dll(library, 'extF80_roundingPrecision')
        exceptions = ctypes.c_uint8.in_dll(library, 'softfloat_exceptionFlags')
        # Canonical normals, signed zeros and values around rounding boundaries.
        values = [(0, 0), (0, 0x8000), (0x8000000000000000, 0x3fff),
                  (0x8000000000000001, 0x3fff), (0xffffffffffffffff, 0x3ffe),
                  (0x8000000000000000, 1), (0xffffffffffffffff, 0x7ffe),
                  (0x8123456789abcdef, 0xbfff)]
        x = unicorn.x86_const
        cases, status_gaps, gap_examples = 0, 0, []
        with tempfile.TemporaryDirectory() as temporary:
            for operation in ('add', 'sub', 'mul', 'div'):
                # Load left then right; pop form computes ST1 op ST0.
                image, record = assemble(Path(temporary),
                    'fld tbyte ptr [0x500]\nfld tbyte ptr [0x510]\n'
                    'f%sp st(1),st\nfstp tbyte ptr [0x520]\nfnstsw word ptr [0x540]' % operation)
                # Execute the identical synthetic arithmetic opcode on host
                # x87. Save/restore its entire environment and register stack.
                opcode = ','.join('0x' + byte for byte in
                                  [record['instructions'][2]['bytes'][i:i+2]
                                   for i in range(0, len(record['instructions'][2]['bytes']), 2)])
                hardware = Path(temporary) / (operation + '.c')
                hardware.write_text(r'''#include <stdint.h>
void probe(const void *a, const void *b, void *out, uint16_t control, uint16_t *status) {
    unsigned char saved[108];
    __asm__ volatile("fnsave %[save]\n\tfldcw %[cw]\n\tfldt (%[a])\n\tfldt (%[b])\n\t"
                     ".byte OPCODE\n\tfstpt (%[out])\n\tfnstsw %[sw]\n\tfrstor %[save]"
                     : [save] "+m"(saved), [sw] "=m"(*status)
                     : [cw] "m"(control), [a] "r"(a), [b] "r"(b), [out] "r"(out)
                     : "memory");
}
'''.replace('OPCODE', opcode))
                shared = hardware.with_suffix('.so')
                subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                                str(hardware), '-o', str(shared)], check=True)
                host = ctypes.CDLL(str(shared)).probe
                host.argtypes = [ctypes.c_void_p] * 3 + [ctypes.c_uint16, ctypes.POINTER(ctypes.c_uint16)]
                host.restype = None
                cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
                cpu.mem_map(0, 4096)
                cpu.mem_write(0, image.read_bytes())
                function = getattr(library, 'extF80M_' + operation)
                function.argtypes = [ctypes.POINTER(Extended)] * 3
                function.restype = None
                for (pc, bits), (rc, mode), left, right in itertools.product(
                        [(0, 32), (2, 64), (3, 80)], enumerate([0, 2, 3, 1]), values, values):
                    with self.subTest(operation=operation, precision=bits, rounding=mode,
                                      left=left, right=right):
                        a, b, result = Extended(*left), Extended(*right), Extended()
                        rounding.value, precision.value, exceptions.value = mode, bits, 0
                        function(ctypes.byref(a), ctypes.byref(b), ctypes.byref(result))
                        control = 0x7f | (pc << 8) | (rc << 10)
                        hardware_result, hardware_status = Extended(), ctypes.c_uint16()
                        host(ctypes.byref(a), ctypes.byref(b), ctypes.byref(hardware_result),
                             control, ctypes.byref(hardware_status))
                        self.assertEqual((result.signif, result.signExp),
                                         (hardware_result.signif, hardware_result.signExp))
                        cpu.reg_write(x.UC_X86_REG_FPCW, control)
                        cpu.reg_write(x.UC_X86_REG_FPSW, 0)
                        cpu.reg_write(x.UC_X86_REG_FPTAG, 0xffff)
                        for address, value in [(0x500, left), (0x510, right)]:
                            cpu.mem_write(address, value[0].to_bytes(8, 'little') +
                                          value[1].to_bytes(2, 'little'))
                        cpu.emu_start(0x100, image.stat().st_size)
                        original = bytes(cpu.mem_read(0x520, 10))
                        self.assertEqual(result.signif.to_bytes(8, 'little') +
                                         result.signExp.to_bytes(2, 'little'), original)
                        flags = exceptions.value
                        mapped = ((flags & 16) >> 4) | ((flags & 8) >> 1) | ((flags & 4) << 1) | \
                                 ((flags & 2) << 3) | ((flags & 1) << 5)
                        # Unicorn's exception-status gap is checked separately;
                        # hardware supplies the independent flag oracle here.
                        self.assertEqual(mapped, hardware_status.value & 0x3d)
                        cases += 1
                        observed = int.from_bytes(cpu.mem_read(0x540, 2), 'little') & 0x3d
                        if observed != mapped:
                            status_gaps += 1
                            if len(gap_examples) < 4:
                                gap_examples.append(dict(operation=operation, precision=bits,
                                    rounding=mode, left=left, right=right,
                                    hardware_exception_flags=mapped, unicorn_exception_flags=observed))
        self.assertEqual(cases, 3072)
        evidence = dict(library_sha256=report['library_sha256'], unicorn_version=unicorn.__version__,
                        hardware=platform.machine(), arithmetic_cases=cases,
                        hardware_raw80_and_exception_matches=cases, unicorn_raw80_matches=cases,
                        unicorn_exception_status_disagreements=status_gaps, examples=gap_examples,
                        limits='Arithmetic dependency only; Unicorn exception status is not a validated oracle.')
        (directory / 'arithmetic-evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
