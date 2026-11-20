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
    def test_variadic_declarations_are_distinct_from_fixed_stack_aliases(self):
        with tempfile.TemporaryDirectory() as directory:
            header = Path(directory) / 'abi.h'
            header.write_text('int EF_CDECL variadic(int count,\n ...);\n'
                              'void fixed(int count /* ... */);\n')
            self.assertEqual(hybrid_link.load_variadic_names(header), {'variadic'})
    def test_only_leading_address_annotation_is_an_entry_alias(self):
        with tempfile.TemporaryDirectory() as directory:
            header = Path(directory) / 'abi.h'
            header.write_text(
                'void direct_entry(void); /* 0x1234 EAX */\n'
                'void indirect_entry(void); /* calls through 0x5678 */\n'
                'void data_reference(void); /* [0x9abc] -> EAX */\n')

            self.assertEqual(hybrid_link.load_abi_aliases(header),
                             {'direct_entry': 0x1234})

    def test_comment_on_a_following_line_is_not_an_alias(self):
        with tempfile.TemporaryDirectory() as directory:
            header = Path(directory) / 'abi.h'
            header.write_text(
                'unsigned char qualifies(void);\n\n'
                '/* 0x263e0: preserves all registers. */\n'
                'void reset_count(void);\n')

            self.assertEqual(hybrid_link.load_abi_aliases(header), {})

    def test_routine_pragma_must_match_register_and_stack_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            header = Path(directory) / 'abi.h'
            header.write_text(
                '#pragma aux mixed parm routine [eax] modify [ebx]\n'
                '#pragma aux stack_only parm routine []\n'
                '#pragma aux caller_cleans parm caller [eax]\n')
            pragmas = hybrid_link.load_routine_pragmas(header)

        self.assertEqual(pragmas, {'mixed': ['eax'], 'stack_only': []})
        mixed = {'inputs': 'AX:int16_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t,[ESP+12]:uint32_t',
                 'stack_bytes': '12'}
        self.assertEqual(hybrid_link.routine_stack_bytes(mixed, ['eax']), 12)
        self.assertEqual(hybrid_link.routine_stack_bytes(mixed, ['edx']), 0)
        self.assertEqual(hybrid_link.routine_stack_bytes(mixed, None), 0)
        short = dict(mixed, stack_bytes='16')
        self.assertEqual(hybrid_link.routine_stack_bytes(short, ['eax']), 0)
        named = {'inputs': 'DX:int16_t,stack_index:uint32_t', 'stack_bytes': '4'}
        self.assertEqual(hybrid_link.routine_stack_bytes(named, ['edx']), 0)

    def test_default_watcall_declarations_use_four_registers_then_the_stack(self):
        with tempfile.TemporaryDirectory() as directory:
            header = Path(directory) / 'abi.h'
            header.write_text(
                'void five(int16_t a, uint32_t b, ef_addr32 c, uint32_t d, double e); /* 0x10 */\n'
                'void four(uint32_t a, uint32_t b, uint32_t c, uint32_t d);\n'
                'void wide(double a, uint32_t b, uint32_t c, uint32_t d, uint32_t e);\n'
                'void EF_CDECL stacked(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e);\n'
                'void listed(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, ...);\n'
                'void pragma(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e);\n'
                '#pragma aux pragma parm caller []\n')
            pragmas = hybrid_link.load_routine_pragmas(header)

        self.assertEqual(pragmas, {'five': (['eax', 'edx', 'ebx', 'ecx'], 8)})
        signature = {'inputs': 'AX:int16_t,EDX:uint32_t,EBX:ef_addr32,ECX:uint32_t,'
                               '[ESP+4]:uint32_t,[ESP+8]:uint32_t', 'stack_bytes': '8'}
        self.assertEqual(hybrid_link.routine_stack_bytes(signature, pragmas['five']), 8)
        signature['stack_bytes'] = '4'
        signature['inputs'] = signature['inputs'].rsplit(',', 1)[0]
        self.assertEqual(hybrid_link.routine_stack_bytes(signature, pragmas['five']), 0)

    def test_real_header_does_not_alias_helpers_to_referenced_data_or_stubs(self):
        aliases = hybrid_link.load_abi_aliases(hybrid_link.ABI_HEADER)

        self.assertNotIn('guest_dos_int_x', aliases)
        self.assertNotIn('guest_allocate_parser_text', aliases)
        self.assertEqual(aliases['emit_command_packet'], 0xa3f74)


class EnglishLinkTests(unittest.TestCase):
    def test_english_redirects_only_the_checked_relocation_free_leaves(self):
        leaves = hybrid_link.ENGLISH_LEAVES
        bodies = (b'\x90' * 13 + b'\xc3', b'\x40' * 13 + b'\xc3')
        code = b''.join(bodies)
        image = bytearray(max(leaves) + len(bodies[-1]))
        functions = []
        signatures = []
        module = Module(segments=[None, Segment('text', 'CODE', len(code), 1, True)])
        module.segments[1].data[:] = code
        offset = 0
        for (address, name), body in zip(leaves.items(), bodies, strict=True):
            image[address:address + len(body)] = body
            module.publics.append(Public(name + '_', 1, offset))
            offset += len(body)
            functions.append(dict(addr=hex(address), size=str(len(body)), name=name,
                                  state='verified', sig_status='confirmed'))
            signatures.append(dict(addr=hex(address), stack_bytes='0',
                                   preserved='', outputs='EAX:int32_t'))
        image = bytes(image)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'ascii_case.obj'
            path.touch()
            objects = [(path, 'hand'), (path.with_name('german.obj'), 'hand')]
            with patch.object(hybrid_link, 'read_module', return_value=module) as parse, \
                    patch.object(hybrid_link, 'ENGLISH_IMAGE_SHA256',
                                 hashlib.sha256(image).hexdigest()):
                with self.assertRaisesRegex(ValueError, 'SHA-256'):
                    hybrid_link.link(objects, functions, [], {}, image, {},
                                     signatures=signatures)
                blob, report = hybrid_link.link(objects, functions, [], {}, image, {},
                                                 signatures=signatures, allow_english=True)
                parse.assert_called_once()
                self.assertEqual(report['abi_trampolines'], 0)
                writes = dict(efpatch.decode(blob, image, hybrid_link.GUEST_SIZE))
                self.assertEqual(set(writes), {*leaves, hybrid_link.DEFAULT_ARENA})
                self.assertEqual(writes[hybrid_link.DEFAULT_ARENA], code)
                target = hybrid_link.DEFAULT_ARENA
                for address, body in zip(leaves, bodies, strict=True):
                    self.assertEqual(writes[address][0], 0xe9)
                    self.assertEqual(address + 5 + struct.unpack_from('<i', writes[address], 1)[0],
                                     target)
                    target += len(body)
                module.segments[1].data[0] ^= 1
                with self.assertRaisesRegex(ValueError, 'differs from the original'):
                    hybrid_link.link(objects, functions, [], {}, image, {},
                                     signatures=signatures, allow_english=True)
                module.fixups.append(Fixup(1, 0, 9, 'self', 5, None, 0, 1, 0))
                with self.assertRaisesRegex(ValueError, 'relocation-free'):
                    hybrid_link.link(objects, functions, [], {}, image, {},
                                     signatures=signatures, allow_english=True)


class SignatureExclusionTests(unittest.TestCase):
    def test_variadic_entry_keeps_the_complete_caller_argument_frame(self):
        try:
            import unicorn as uc
            from unicorn import x86_const as x86
        except ImportError:
            self.skipTest('requires Unicorn')
        # Synthetic cdecl body: preserve ECX/EDX, then sum COUNT stack words.
        code = bytes.fromhex('51528b4c240c8d54241031c085c97407030283c204e2f95a59c3')
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'variadic.obj'
            path.write_bytes(b'fixture')
            module = Module(segments=[None, Segment('text', 'CODE', len(code), 1, True)],
                            publics=[Public('variadic_', 1, 0)])
            module.segments[1].data[:] = code
            functions = [dict(addr='0x20', size='8', name='variadic',
                              state='compiles', sig_status='confirmed')]
            signatures = [dict(addr='0x20', name='variadic', inputs='[ESP+4]:uint32_t',
                               outputs='EAX:uint32_t', preserved='ebx,ecx,edx,esi,edi,ebp',
                               stack_bytes='0')]
            source = bytes(128)
            with patch.object(hybrid_link, 'read_module', return_value=module), \
                    patch.object(hybrid_link, 'IMAGE_SHA256', hashlib.sha256(source).hexdigest()):
                blob, report = hybrid_link.link(
                    [(path, 'hand')], functions, [], {}, source, {},
                    signatures=signatures, variadic_names={'variadic'})
            self.assertEqual(report['abi_trampolines'], 0)
            self.assertEqual(report['variadic_direct_entries'], ['0x00000020'])
            for values in ((), (17,), (11, 22, 33), (1, 2, 3, 4, 5, 6)):
                with self.subTest(values=values):
                    cpu = uc.Uc(uc.UC_ARCH_X86, uc.UC_MODE_32)
                    cpu.mem_map(0, hybrid_link.GUEST_SIZE)
                    cpu.mem_write(0, source)
                    for address, data in efpatch.decode(blob, source, hybrid_link.GUEST_SIZE):
                        cpu.mem_write(address, data)
                    stack, sentinel = 0x3fff000, 0x3ffe000
                    args = (sentinel, len(values), *values)
                    frame = struct.pack('<' + 'I' * len(args), *args)
                    cpu.mem_write(stack, frame)
                    cpu.reg_write(x86.UC_X86_REG_ESP, stack)
                    saved = {reg: 0x12345678 + index for index, reg in enumerate(
                        ('EBX', 'ECX', 'EDX', 'ESI', 'EDI', 'EBP'))}
                    for reg, value in saved.items():
                        cpu.reg_write(getattr(x86, 'UC_X86_REG_' + reg), value)
                    cpu.emu_start(0x20, sentinel, count=1000)
                    self.assertEqual(cpu.reg_read(x86.UC_X86_REG_EIP), sentinel)
                    self.assertEqual(cpu.reg_read(x86.UC_X86_REG_ESP), stack + 4)
                    self.assertEqual(cpu.reg_read(x86.UC_X86_REG_EAX), sum(values))
                    self.assertEqual(cpu.mem_read(stack, len(frame)), frame)
                    for reg, value in saved.items():
                        self.assertEqual(cpu.reg_read(getattr(x86, 'UC_X86_REG_' + reg)), value)

    def test_routine_trampoline_forwards_and_removes_stack_arguments(self):
        try:
            import unicorn as uc
            from unicorn import x86_const as x86
        except ImportError:
            self.skipTest('requires Unicorn')
        # Synthetic parm routine [eax] body: EAX += two stack words, ret 8,
        # clobbering ECX/EDX, which the original entry preserves.
        code = bytes.fromhex('8b4c240401c88b54240801d0c20800')
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'routine.obj'
            path.write_bytes(b'fixture')
            module = Module(segments=[None, Segment('text', 'CODE', len(code), 1, True)],
                            publics=[Public('routine_', 1, 0)])
            module.segments[1].data[:] = code
            functions = [dict(addr='0x20', size='8', name='routine',
                              state='compiles', sig_status='confirmed')]
            signatures = [dict(addr='0x20', name='routine',
                               inputs='EAX:uint32_t,[ESP+4]:uint32_t,[ESP+8]:uint32_t',
                               outputs='EAX:uint32_t', preserved='ebx,ecx,edx,esi,edi,ebp',
                               stack_bytes='8')]
            source = bytes(128)
            with patch.object(hybrid_link, 'read_module', return_value=module), \
                    patch.object(hybrid_link, 'IMAGE_SHA256', hashlib.sha256(source).hexdigest()):
                blob, report = hybrid_link.link(
                    [(path, 'hand')], functions, [], {}, source, {},
                    signatures=signatures, routine_pragmas={'routine': ['eax']})
                _, unmatched = hybrid_link.link(
                    [(path, 'hand')], functions, [], {}, source, {},
                    signatures=signatures, routine_pragmas={'routine': ['edx']})
        self.assertEqual(report['abi_trampolines'], 1)
        self.assertIn({'addr': '0x00000020',
                       'reason': 'stack arguments need a stack-aware trampoline'},
                      unmatched['excluded'])
        cpu = uc.Uc(uc.UC_ARCH_X86, uc.UC_MODE_32)
        cpu.mem_map(0, hybrid_link.GUEST_SIZE)
        cpu.mem_write(0, source)
        for address, data in efpatch.decode(blob, source, hybrid_link.GUEST_SIZE):
            cpu.mem_write(address, data)
        stack, sentinel = 0x3fff000, 0x3ffe000
        cpu.mem_write(stack, struct.pack('<III', sentinel, 5, 7))
        cpu.reg_write(x86.UC_X86_REG_ESP, stack)
        cpu.reg_write(x86.UC_X86_REG_EAX, 1)
        cpu.reg_write(x86.UC_X86_REG_ECX, 0x11111111)
        cpu.reg_write(x86.UC_X86_REG_EDX, 0x22222222)
        cpu.emu_start(0x20, sentinel, count=1000)
        self.assertEqual(cpu.reg_read(x86.UC_X86_REG_EAX), 13)
        self.assertEqual(cpu.reg_read(x86.UC_X86_REG_ESP), stack + 12)
        self.assertEqual(cpu.reg_read(x86.UC_X86_REG_ECX), 0x11111111)
        self.assertEqual(cpu.reg_read(x86.UC_X86_REG_EDX), 0x22222222)

    def test_unsupported_abi_keeps_entry_and_external_calls_original(self):
        # A supported compiled caller calls an unsupported callee in another
        # object. Check both the original entry bytes and the relocated CALL;
        # skipping only its entry redirect would still call the unsafe body.
        cases = (
            ({}, 'signature row missing'),
            ({'stack_bytes': '4'}, 'stack arguments'),
            ({'outputs': 'EAX:uint32_t,EDX:uint32_t'}, 'multiple return registers'),
            ({'preserved': 'ds'}, 'unsupported preserved-register set'),
            ({'inputs': 'EAX:uint32_t'}, 'native C stack/result ABI'),
            ({'preserved': 'eax'}, 'native C stack/result ABI'),
            ({'outputs': 'EDX:uint32_t'}, 'native C stack/result ABI'),
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
                        signatures=signatures, variadic_names={'callee'})
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
