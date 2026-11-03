# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Opt-in original-game checkpoint and fresh-process restore regression."""

import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile
from unittest.mock import patch

from tools.save_states import encode, decode
from tools.sound_blaster import SB16Probe, SoundBlaster16


class SoundStateTests(unittest.TestCase):
    def test_json_roundtrip_preserves_discovery_queue_and_registers(self):
        for kind in (SB16Probe, SoundBlaster16):
            with self.subTest(device=kind.__name__):
                device = kind()
                device.write(0x226, 1)
                device.write(0x226, 0)
                device.write(0x22c, 0xe1)
                state = json.loads(json.dumps(device.save_state(), default=encode), object_hook=decode)
                restored = kind()
                restored.restore_state(state)
                self.assertEqual([restored.read(0x22a) for _ in range(3)], [0xaa, 4, 5])
                restored.write(0x224, 0x81)
                self.assertEqual(restored.read(0x225), 0x22)

    def test_restored_dma_completes_after_remaining_time_and_raises_irq(self):
        device = SoundBlaster16()
        device.write(0x226, 1)
        device.write(0x226, 0)
        for port, value in ((0xd8, 0), (0xc4, 0), (0xc4, 0x80),
                            (0xc6, 3), (0xc6, 0), (0x8b, 0),
                            (0xd6, 0x49), (0xd4, 1), (0x21, 0x78)):
            device.write(port, value)
        with patch('time.monotonic', return_value=100):
            for value in (0xb0, 0x30, 3, 0):
                device.write(0x22c, value)
            state = json.loads(json.dumps(device.save_state(), default=encode), object_hook=decode)
        self.assertGreater(device.active['deadline'], 100)
        restored = SoundBlaster16()
        with patch('time.monotonic', return_value=1000):
            restored.restore_state(state)
        restored.advance(1000, lambda address, length: self.fail('DMA completed early'))
        reads = []
        def memory(address, length):
            reads.append((address, length))
            return bytes(range(length))
        restored.advance(restored.active['deadline'], memory)
        self.assertEqual(reads, [(0x10000, 8)])
        self.assertEqual(restored.completed[0]['pcm'], bytes(range(8)))
        self.assertEqual(restored.pic.next_irq(), 7)
        restored.read(0x22f)
        self.assertEqual(restored.resources[0x82] & 3, 0)
        self.assertIsNone(restored.active)


@unittest.skipUnless(os.environ.get('EF2000_EXE') and platform.machine() == 'x86_64'
                     and all(shutil.which(name) for name in ('gcc', 'as', 'ld')),
                     'requires EF2000_EXE and native development tools')
class NativeSaveStateTests(unittest.TestCase):
    def test_save_restore_and_cli_loading_preserve_assets(self):
        self.check_save_restore('off')

    def test_sb16_save_restore_and_cli_loading_preserve_assets(self):
        self.check_save_restore('sb16')

    def check_save_restore(self, sound):
        root = Path(__file__).resolve().parent.parent
        executable = Path(os.environ['EF2000_EXE']).resolve()
        originals = [executable, executable.parent / 'EF2000.CFG']
        before = [hashlib.sha256(path.read_bytes()).digest() for path in originals]
        # Exercise the production bridge without injecting into a desktop. The
        # test schedules one save after the menu settles, then an in-process load.
        script = '''
import sys, time
from tools import real_mode, save_states, run_startup
original_init = save_states.SaveStates.__init__
original_execute = real_mode.RealMode.execute
state = {}
def init(self, *args):
    original_init(self, *args)
    state['store'] = self
save_states.SaveStates.__init__ = init
def execute(self, *args):
    result = original_execute(self, *args)
    store = state['store']
    now = time.monotonic()
    if self.video_mode == 0x100 and 'stage' not in state:
        state.update(stage='wait', at=now)
    if state.get('stage') == 'wait' and now - state['at'] > 1:
        (store.output / 'drive' / 'checkpoint-test.txt').write_text('saved overlay')
        store.pending = ('save', None)
        state['stage'] = 'saving'
    if state.get('stage') == 'saving' and store.entries():
        (store.output / 'drive' / 'checkpoint-test.txt').write_text('changed overlay')
        state.update(stage='saved', at=now)
    if state.get('stage') == 'saved' and now - state['at'] > 1:
        store.pending = ('load', store.entries()[0])
        state['stage'] = 'loaded'
    return result
real_mode.RealMode.execute = execute
run_startup.main()
assert state.get('stage') == 'loaded', state
'''
        with tempfile.TemporaryDirectory(prefix='ef2000-state-') as temporary:
            output = Path(temporary) / 'save'
            environment = dict(os.environ, XDG_DATA_HOME=str(Path(temporary) / 'data'))
            result = subprocess.run([sys.executable, '-c', script, str(executable),
                                     '--output', str(output), '--headless', '--seconds', '22', '--sound', sound],
                                    cwd=root, env=environment, capture_output=True, text=True, timeout=45)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('state: loaded', result.stdout)
            state, = (Path(temporary) / 'data/ef2000/states').glob('*.efstate')
            self.assertIn('-menu-640x400', state.name)
            self.assertEqual((output / 'drive/checkpoint-test.txt').read_text(), 'saved overlay')
            with zipfile.ZipFile(state) as archive:
                self.assertEqual(json.loads(archive.read('metadata.json'))['mode'], 'menu')
                self.assertIsNone(archive.testzip())
            restored = Path(temporary) / 'restore'
            result = subprocess.run([sys.executable, str(root / 'tools/run_startup.py'),
                                     str(executable), '--output', str(restored), '--headless',
                                     '--seconds', '7', '--load-state', str(state), '--sound', sound],
                                    cwd=root, env=environment, capture_output=True, text=True, timeout=20)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('game: restored menu', result.stdout)
            self.assertEqual((restored / 'drive/checkpoint-test.txt').read_text(), 'saved overlay')
            self.assertFalse(any('error' in request for request in
                             json.loads((restored / 'report.json').read_text())['real_mode_execution']))
        self.assertEqual(before, [hashlib.sha256(path.read_bytes()).digest() for path in originals])
