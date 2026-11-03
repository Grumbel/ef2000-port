# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

import subprocess
import unittest

from tools.abi_audit import analyze, parse_disassembly


class AbiAuditTests(unittest.TestCase):
    def test_disassembly_observations(self):
        asm = '''
  100: 53                   push ebx
  101: d9 e8                fld1
  103: f3 a5                rep movs DWORD PTR es:[edi],DWORD PTR ds:[esi]
  105: 5b                   pop ebx
  106: c2 08 00             ret 0x8
'''
        facts = parse_disassembly(asm)
        self.assertEqual(facts['push_regs'], 'ebx')
        self.assertEqual(facts['pop_regs'], 'ebx')
        self.assertEqual(facts['x87_instructions'], '1')
        self.assertEqual(facts['rep_instructions'], '1')
        self.assertEqual(facts['ret_forms'], 'ret 0x8')

    def test_requires_two_distinct_callers(self):
        sample = [dict(entry='0x00000100', shape='leaf', body_bytes='8')]
        calls = [dict(kind='direct', callee_entry='0x00000100',
                      caller_entry='0x00000020', call_site='0x00000023')]
        with self.assertRaisesRegex(ValueError, 'fewer than two'):
            analyze(sample, calls, b'\x90' * 0x200,
                    runner=lambda argv, **kw: subprocess.CompletedProcess(argv, 0, '', ''))

    def test_caller_samples(self):
        sample = [dict(entry='0x00000100', shape='leaf', body_bytes='8')]
        calls = [dict(kind='direct', callee_entry='0x00000100',
                      caller_entry=f'0x{addr:x}', call_site=f'0x{addr+3:x}')
                 for addr in (0x20, 0x40)]
        def run(argv, **kwargs):
            return subprocess.CompletedProcess(argv, 0,
                                               '  100: c3    ret\n', '')
        rows = analyze(sample, calls, b'\x90' * 0x200, runner=run)
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]['distinct_callers'], '2')
        self.assertEqual(rows[0]['caller_a'], '0x00000020@0x00000023')


if __name__ == '__main__':
    unittest.main()
