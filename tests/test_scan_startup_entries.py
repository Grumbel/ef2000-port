# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Asset-free tests for the dynamic startup entry survey."""

import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools.efpatch import decode
from tools.scan_startup_entries import entries, scan


class StartupEntrySurveyTests(unittest.TestCase):
    def test_parse_tracker_and_reject_duplicates(self):
        with tempfile.TemporaryDirectory() as root:
            tracker = Path(root) / 'functions.tsv'
            tracker.write_text('addr\tsize\tname\tstate\tsig_status\tmodule\n'
                               '0x10\t6\talpha\tcompiles\tconfirmed\tutil\n'
                               '0x20\t0\tempty\tdrafted\tunverified\tutil\n'
                               '0x30\t3\tout\tdrafted\tconfirmed\tutil\n')
            self.assertEqual(list(entries(tracker, 0x30)), [0x10])
            tracker.write_text('addr\tsize\tname\tstate\tsig_status\tmodule\n'
                               '0x10\t6\talpha\tcompiles\tconfirmed\tutil\n'
                               '0x10\t3\tbeta\tdrafted\tconfirmed\tutil\n')
            with self.assertRaisesRegex(ValueError, 'duplicate entry'):
                entries(tracker, 128)

    def test_identity_patch_chunks_and_runner_status(self):
        data = bytes(range(128))
        indexed = {address: {'name': f'function_{address}'}
                   for address in (0x10, 0x20, 0x30)}
        status = 'status=guest_exit interrupts=56 ticks=43 irq0=0 ip=0x222\n'
        observations = []

        def emulate(command, **kwargs):
            self.assertEqual(kwargs['check'], False)
            self.assertIn('--patch', command)
            path = Path(command[command.index('--patch') + 1])
            writes = decode(path.read_bytes(), data)
            observations.append(writes)
            self.assertTrue(all(value == data[offset:offset + 1]
                                for offset, value in writes))
            hits = ''.join(f'patch_hit: addr=0x{offset:x} count={offset // 16}\n'
                           for offset, _ in writes)
            total = sum(offset // 16 for offset, _ in writes)
            return subprocess.CompletedProcess(command, 0,
                                               stdout=(status + hits +
                                                       f'patch_hits: total={total}\n').encode())

        with tempfile.TemporaryDirectory() as root:
            with patch('tools.scan_startup_entries.subprocess.run', emulate):
                counts, final = scan(data, indexed, Path('private.exe'), Path('runner'),
                                     100, 2, 30, Path(root))
        self.assertEqual(len(observations), 2)
        self.assertEqual(counts, {0x10: 1, 0x20: 2, 0x30: 3})
        self.assertEqual(final, ('guest_exit', '56', '43', '0', '0x222'))

    def test_changed_identity_replay_is_rejected(self):
        data = bytes(range(128))
        indexed = {0x10: {}, 0x20: {}}
        outputs = iter([
            b'status=guest_exit interrupts=56 ticks=43 irq0=0 ip=0x222\n'
            b'patch_hit: addr=0x10 count=1\npatch_hits: total=1\n',
            b'status=guest_exit interrupts=55 ticks=43 irq0=0 ip=0x222\n'
            b'patch_hit: addr=0x20 count=1\npatch_hits: total=1\n',
        ])

        def emulate(command, **_):
            return subprocess.CompletedProcess(command, 0, stdout=next(outputs))

        with tempfile.TemporaryDirectory() as root:
            with patch('tools.scan_startup_entries.subprocess.run', emulate):
                with self.assertRaisesRegex(ValueError, 'divergent identity replay'):
                    scan(data, indexed, Path('private.exe'), Path('runner'),
                         100, 1, 30, Path(root))


if __name__ == '__main__':
    unittest.main()
