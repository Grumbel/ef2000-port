# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Synthetic 32-bit OMF patch-planner tests, requiring no game or Watcom."""

import hashlib
import json
from dataclasses import replace
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from tools.flirt_match import OMFError
from tools.omf_splice import inspection, plan_patch, read_module, resolve_symbol


def record(kind, payload):
    length = len(payload) + 1
    header = bytes([kind]) + length.to_bytes(2, 'little')
    return header + payload + bytes([(-sum(header + payload)) & 0xff])


def symbol(value):
    return bytes([len(value)]) + value.encode('ascii')


def fixture(code, fixups=b'\xa4\x01\x56\x01\xe4\x06\x54\x02'):
    # __CHK() is an external near CALL at code byte 1; byte 6 is an
    # absolute DS-relative address of local 4-byte initialized data.
    return b''.join((
        record(0x80, symbol('example')),
        record(0x96, symbol('_TEXT') + symbol('CODE') +
               symbol('_DATA') + symbol('DATA')),
        record(0x99, b'\x61' + len(code).to_bytes(4, 'little') + b'\x01\x02\x00'),
        record(0x99, b'\x61\x04\x00\x00\x00\x03\x04\x00'),
        record(0x8c, symbol('__CHK') + b'\x00'),
        record(0x91, b'\x00\x01' + symbol('ef_foo_') + b'\x00\x00\x00\x00\x00'),
        record(0xa1, b'\x01\x00\x00\x00\x00' + code),
        # First: self-rel L9 32-bit, target external index 1, frame from target.
        # Second: segment-rel L9 32-bit, target segment index 2.
        record(0x9d, fixups),
        record(0xa1, b'\x02\x00\x00\x00\x00\x78\x56\x34\x12'),
        record(0x8a, b'\x00'),
    ))


def rows():
    return ([{'addr': '0x00000020', 'size': '24', 'name': 'ef_foo',
              'state': 'compiles', 'sig_status': 'confirmed'},
             {'addr': '0x00000070', 'size': '12', 'name': '__CHK',
              'state': 'library', 'sig_status': 'unverified'}], [])


def code(length=12):
    assert length >= 11
    return bytes.fromhex('e8 00000000 a1 00000000 c3') + b'\x90' * (length - 11)


class OMFSplicerTests(unittest.TestCase):
    def test_resolves_watcom_data_external_leading_underscore(self):
        globals_ = [{'addr': '0x0019f5b0', 'name': 'g_link_pointer_table'}]
        self.assertEqual(resolve_symbol('_g_link_pointer_table', [], globals_),
                         0x0019f5b0)

    def test_inspect_tracks_code_data_externals_and_both_fixups(self):
        module = read_module(fixture(code()))
        report = inspection(module)
        self.assertEqual(report['module'], 'example')
        self.assertEqual(report['segments'][0]['initialized'], 12)
        self.assertEqual(report['externals'], ['__CHK'])
        self.assertEqual(report['publics'][0]['symbol'], 'ef_foo_')
        self.assertEqual([(f['mode'], f['offset']) for f in report['fixups']],
                         [('self', 1), ('segment', 6)])

    def test_inline_placement_and_relocations(self):
        module = read_module(fixture(code()))
        functions, globals_ = rows()
        patch = plan_patch(module, bytes(128), functions, globals_, 0x100, {0x20})
        self.assertEqual(patch['format'], 'ef2000-patch-v1')
        self.assertEqual([entry['kind'] for entry in patch['writes']], ['inline', 'arena'])
        inline = bytes.fromhex(patch['writes'][0]['bytes'])
        self.assertEqual(len(inline), 24)
        self.assertEqual(int.from_bytes(inline[1:5], 'little'), 0x70 - (0x20 + 5))
        self.assertEqual(int.from_bytes(inline[6:10], 'little'), 0x100)
        self.assertEqual(inline[12:], b'\xcc' * 12)
        self.assertEqual(bytes.fromhex(patch['writes'][1]['bytes']), b'\x78\x56\x34\x12')

    def test_flat_group_self_relative_external_call(self):
        # OpenWatcom emits external near calls framed against the FLAT group.
        # An inline replacement stays at its reviewed guest address, so the
        # call displacement is target - (guest address + fixup + width).
        module = read_module(fixture(code()))
        module.groups = [None, {'name': 'DGROUP', 'segments': [2]},
                         {'name': 'FLAT', 'segments': []}]
        module.fixups[0] = replace(module.fixups[0], frame_method=1, frame_index=2)
        functions, globals_ = rows()
        patch = plan_patch(module, bytes(128), functions, globals_, 0x100, {0x20})
        inline = bytes.fromhex(patch['writes'][0]['bytes'])
        self.assertEqual(int.from_bytes(inline[1:5], 'little'), 0x70 - (0x20 + 5))

    def test_flat_group_absolute_external_data_offset(self):
        module = read_module(fixture(code()))
        module.groups = [None, {'name': 'DGROUP', 'segments': [2]},
                         {'name': 'FLAT', 'segments': []}]
        module.fixups[0] = replace(module.fixups[0], mode='segment',
                                   frame_method=1, frame_index=2)
        functions, globals_ = rows()
        patch = plan_patch(module, bytes(128), functions, globals_, 0x100, {0x20})
        inline = bytes.fromhex(patch['writes'][0]['bytes'])
        self.assertEqual(int.from_bytes(inline[1:5], 'little'), 0x70)

    def test_arena_redirect_and_source_relative_fixup(self):
        module = read_module(fixture(code(40)))
        functions, globals_ = rows()
        patch = plan_patch(module, bytes(128), functions, globals_, 0x100, {0x20})
        redirect = bytes.fromhex(patch['writes'][0]['bytes'])
        self.assertEqual(redirect[0], 0xe9)
        self.assertEqual(int.from_bytes(redirect[1:5], 'little'), 0x100 - 0x20 - 5)
        self.assertEqual(patch['writes'][1]['offset'], 0x100)
        relocated = bytes.fromhex(patch['writes'][1]['bytes'])
        self.assertEqual(int.from_bytes(relocated[1:5], 'little'),
                         (0x70 - (0x100 + 5)) & 0xffffffff)
        self.assertEqual(int.from_bytes(relocated[6:10], 'little'), 0x130)
        self.assertEqual(patch['arena_end'], 0x134)

    def test_reject_unreviewed_flow_unconfirmed_abi_and_unresolved_external(self):
        module = read_module(fixture(code()))
        functions, globals_ = rows()
        with self.assertRaisesRegex(OMFError, 'control-flow boundary'):
            plan_patch(module, bytes(128), functions, globals_, 0x100, set())
        bad = [dict(row) for row in functions]
        bad[0]['sig_status'] = 'unverified'
        with self.assertRaisesRegex(OMFError, 'confirmed ABI'):
            plan_patch(module, bytes(128), bad, globals_, 0x100, {0x20})
        with self.assertRaisesRegex(OMFError, 'canonical addresses'):
            plan_patch(module, bytes(128), [functions[0]], globals_, 0x100, {0x20})

    def test_fails_closed_on_unknown_fixups_and_records(self):
        with self.assertRaisesRegex(OMFError, 'unrecognized OMF record'):
            read_module(fixture(code()) + record(0xf0, b'\x00'))
        with self.assertRaisesRegex(OMFError, 'undefined target thread'):
            read_module(fixture(code(), b'\xa4\x01\x5c\xe4\x06\x54\x02'))
        # Defining a TARGET thread allows a compact external relocation.
        threaded = read_module(fixture(code(), b'\x08\x01\xa4\x01\x5c\xe4\x06\x54\x02'))
        self.assertEqual(threaded.fixups[0].target_index, 1)
        self.assertEqual(threaded.fixups[0].target_method, 2)
        duplicates = read_module(fixture(code(),
                                         b'\xa4\x01\x56\x01\xa4\x01\x56\x01'))
        functions, globals_ = rows()
        with self.assertRaisesRegex(OMFError, 'duplicate fixup'):
            plan_patch(duplicates, bytes(128), functions, globals_, 0x100, {0x20})

    def test_cli_inspection_has_no_side_effects(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            obj = root / 'fixture.obj'
            obj.write_bytes(fixture(code()))
            output = root / 'inspection.json'
            script = Path(__file__).resolve().parents[1] / 'tools/omf_splice.py'
            subprocess.run([sys.executable, str(script), str(obj), '-o', str(output)],
                           check=True, capture_output=True)
            self.assertEqual(json.loads(output.read_text())['module'], 'example')
            self.assertEqual(obj.read_bytes(), fixture(code()))

    def test_cli_offline_plan_includes_original_image_digest(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            obj = root / 'fixture.obj'
            obj.write_bytes(fixture(code()))
            source = root / 'payload.bin'
            source.write_bytes(bytes(128))
            functions = root / 'functions.tsv'
            functions.write_text('addr\tsize\tname\tstate\tsig_status\n'
                                 '0x00000020\t24\tef_foo\tcompiles\tconfirmed\n'
                                 '0x00000070\t12\t__CHK\tlibrary\tunverified\n')
            globals_ = root / 'globals.tsv'
            globals_.write_text('addr\tname\n')
            output = root / 'fixture.efpatch'
            script = Path(__file__).resolve().parents[1] / 'tools/omf_splice.py'
            subprocess.run([sys.executable, str(script), str(obj), '--image',
                            str(source), '--functions', str(functions), '--globals',
                            str(globals_), '--arena-base', '0x100', '--reviewed-flow',
                            '0x20', '-o', str(output)], check=True, capture_output=True)
            patch = json.loads(output.read_text())
            self.assertEqual(patch['base_image_sha256'],
                             hashlib.sha256(bytes(128)).hexdigest())
            self.assertEqual(patch['writes'][0]['offset'], 0x20)
            self.assertEqual(source.read_bytes(), bytes(128))


if __name__ == '__main__':
    unittest.main()
