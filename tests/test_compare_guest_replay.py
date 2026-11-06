# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Asset-free tests for the private deterministic startup replay comparator."""

import hashlib
import subprocess
import unittest
from pathlib import Path
from unittest.mock import patch

from tools.compare_guest_replay import (complementary_hash_ranges, parse_replay,
                                        replay_differences,
                                        run_replay)


class DeterministicGuestReplayTests(unittest.TestCase):
    def test_changed_bytes_are_excluded_and_spans_are_bounded(self):
        self.assertEqual(complementary_hash_ranges([(8, b'abc'), (3, b'xx')], 16),
                         [(0, 3), (5, 3), (11, 5)])
        self.assertEqual(complementary_hash_ranges([], 0x2000001),
                         [(0, 0x1000000), (0x1000000, 0x1000000),
                          (0x2000000, 1)])
        with self.assertRaisesRegex(ValueError, 'overlapping'):
            complementary_hash_ranges([(3, b'ab'), (4, b'cd')], 20)
        with self.assertRaisesRegex(ValueError, 'out-of-range'):
            complementary_hash_ranges([(18, b'abcd')], 20)

    def test_status_hashes_and_patch_entries_are_required(self):
        ranges = [(0, 3), (5, 3)]
        hashes = ''.join(f'guest_hash: addr=0x{addr:x} length={length} '
                         f'sha256={hashlib.sha256(bytes([addr]) * length).hexdigest()}\n'
                         for addr, length in ranges)
        status = 'status=guest_exit interrupts=56 ticks=43 irq0=0 ip=0x222 ax=0x1\n'
        memory = 'x32_memory: heap=3 mapped=67108864\n'
        hits = 'patch_hit: addr=0x10 count=2\npatch_hits: total=2\n'
        result = parse_replay(status + memory + hashes + hits, ranges, {0x10},
                              patched=True)
        self.assertEqual(len(result[2]), 2)
        with self.assertRaisesRegex(ValueError, 'never reached'):
            parse_replay(status + memory + hashes + hits.replace('count=2', 'count=0'),
                         ranges, {0x10}, patched=True)
        with self.assertRaisesRegex(ValueError, 'missing or unexpected'):
            parse_replay(status + memory + hashes.splitlines(keepends=True)[0],
                         ranges, set(), patched=False)
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            parse_replay(status + memory + hashes + hashes, ranges, set(),
                         patched=False)

    def test_replay_difference_names_guest_ram_region(self):
        reference = ('status', 'memory', {(0x1000, 16): 'before'})
        candidate = ('status', 'memory', {(0x1000, 16): 'after'})
        self.assertEqual(replay_differences(reference, candidate),
                         ['guest RAM hash differs at 0x1000+0x10'])

    def test_runner_invocation_is_fixed_clock_and_bounded(self):
        def fake_run(command, **kwargs):
            self.assertEqual(kwargs['timeout'], 12)
            self.assertEqual(command[0:2], ['runner', 'original.exe'])
            self.assertEqual(command.count('--patch'), 2)
            self.assertEqual(command[command.index('--dos-clock') + 1], 'guest')
            self.assertEqual(command[command.index('--vga-clock') + 1], 'guest')
            self.assertEqual(command[command.index('--timing') + 1], 'instruction')
            self.assertEqual(command[command.index('--sound') + 1], 'off')
            self.assertEqual(command[command.index('--instruction-limit') + 1],
                             '20000000')
            self.assertEqual(command.count('--hash-guest'), 2)
            return subprocess.CompletedProcess(command, 0, stdout=b'example\n')

        with patch('tools.compare_guest_replay.subprocess.run', fake_run):
            self.assertEqual(run_replay(Path('runner'), Path('original.exe'),
                                        [Path('a'), Path('b')], [(0, 3), (5, 3)],
                                        12), 'example\n')


if __name__ == '__main__':
    unittest.main()
