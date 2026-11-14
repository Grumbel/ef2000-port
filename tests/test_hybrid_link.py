# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Asset-free tests for hybrid-link ABI entry aliases."""

import copy
import hashlib
import struct
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools import efpatch, hybrid_link
from tools.omf_splice import Fixup, Module, Public, Segment


class AbiAliasTests(unittest.TestCase):
    def test_only_leading_address_annotation_is_an_entry_alias(self):
        with tempfile.TemporaryDirectory() as directory:
            header = Path(directory) / 'abi.h'
            header.write_text(
                'void direct_entry(void); /* 0x1234 EAX */\n'
                'void indirect_entry(void); /* calls through 0x5678 */\n'
                'void data_reference(void); /* [0x9abc] -> EAX */\n')

            self.assertEqual(hybrid_link.load_abi_aliases(header),
                             {'direct_entry': 0x1234})

    def test_real_header_does_not_alias_helpers_to_referenced_data_or_stubs(self):
        aliases = hybrid_link.load_abi_aliases(hybrid_link.ABI_HEADER)

        self.assertNotIn('guest_dos_int_x', aliases)
        self.assertNotIn('guest_allocate_parser_text', aliases)
        self.assertEqual(aliases['emit_command_packet'], 0xa3f74)


class SignatureExclusionTests(unittest.TestCase):
    def test_unsupported_abi_keeps_entry_and_external_calls_original(self):
        # A supported compiled caller calls an unsupported callee in another
        # object. Check both the original entry bytes and the relocated CALL;
        # skipping only its entry redirect would still call the unsafe body.
        cases = (
            ({}, 'signature row missing'),
            ({'stack_bytes': '4'}, 'stack arguments'),
            ({'outputs': 'EAX:uint32_t,EDX:uint32_t'}, 'multiple return registers'),
            ({'preserved': 'ds'}, 'unsupported preserved-register set'),
        )
        for overrides, reason in cases:
            with self.subTest(reason=reason), tempfile.TemporaryDirectory() as directory:
                paths = [Path(directory) / name for name in ('callee.obj', 'caller.obj')]
                for number, path in enumerate(paths):
                    path.write_bytes(bytes([number]))
                callee = Module(segments=[None, Segment('text', 'CODE', 1, 1, True)],
                                publics=[Public('callee_', 1, 0)])
                callee.segments[1].data[:] = b'\xc3'
                caller = Module(segments=[None, Segment('text', 'CODE', 6, 1, True)],
                                publics=[Public('caller_', 1, 0)],
                                externs=['', 'callee_'],
                                fixups=[Fixup(1, 1, 9, 'self', 5, None, 2, 1, 0)])
                caller.segments[1].data[:] = b'\xe8\0\0\0\0\xc3'
                functions = [dict(addr=hex(address), size='8', name=name,
                                  state='compiles', sig_status='confirmed')
                             for address, name in ((0x20, 'callee'), (0x40, 'caller'))]
                signatures = [dict(addr='0x40', inputs='', outputs='EAX:uint32_t',
                                   preserved='', stack_bytes='0')]
                if overrides:
                    signatures.append(dict(signatures[0], addr='0x20', **overrides))
                with patch.object(hybrid_link, 'read_module',
                                  side_effect=lambda blob: copy.deepcopy((callee, caller)[blob[0]])), \
                        patch.object(hybrid_link, 'IMAGE_SHA256', hashlib.sha256(bytes(128)).hexdigest()):
                    blob, report = hybrid_link.link(
                        [(path, 'hand') for path in paths], functions, [], {}, bytes(128), {},
                        signatures=signatures)
                writes = dict(efpatch.decode(blob, bytes(128), hybrid_link.GUEST_SIZE))
                self.assertNotIn(0x20, writes)
                self.assertIn(0x40, writes)
                arena = writes[hybrid_link.DEFAULT_ARENA]
                # Callee occupies byte 0; the caller's E8 starts at byte 1.
                displacement = struct.unpack_from('<i', arena, 2)[0]
                self.assertEqual(hybrid_link.DEFAULT_ARENA + 6 + displacement, 0x20)
                self.assertEqual(report['functions_redirected'], 1)
                self.assertTrue(any(item['addr'] == '0x00000020' and reason in item['reason']
                                    for item in report['excluded']))


if __name__ == '__main__':
    unittest.main()
