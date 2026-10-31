# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

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


@unittest.skipUnless(os.environ.get('EF2000_EXE') and platform.machine() == 'x86_64'
                     and all(shutil.which(name) for name in ('gcc', 'as', 'ld')),
                     'requires EF2000_EXE and native development tools')
class NativeSaveStateTests(unittest.TestCase):
    def test_save_restore_and_cli_loading_preserve_assets(self):
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
            result = subprocess.run([sys.executable, '-c', script, str(executable),
                                     '--output', str(output), '--headless', '--seconds', '22'],
                                    cwd=root, capture_output=True, text=True, timeout=45)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('state: loaded', result.stdout)
            state, = (output / 'states').glob('*.efstate')
            self.assertIn('-menu-640x400', state.name)
            self.assertEqual((output / 'drive/checkpoint-test.txt').read_text(), 'saved overlay')
            with zipfile.ZipFile(state) as archive:
                self.assertEqual(json.loads(archive.read('metadata.json'))['mode'], 'menu')
                self.assertIsNone(archive.testzip())
            restored = Path(temporary) / 'restore'
            result = subprocess.run([sys.executable, str(root / 'tools/run_startup.py'),
                                     str(executable), '--output', str(restored), '--headless',
                                     '--seconds', '7', '--load-state', str(state)],
                                    cwd=root, capture_output=True, text=True, timeout=20)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('game: restored menu', result.stdout)
            self.assertEqual((restored / 'drive/checkpoint-test.txt').read_text(), 'saved overlay')
            self.assertFalse(any('error' in request for request in
                             json.loads((restored / 'report.json').read_text())['real_mode_execution']))
        self.assertEqual(before, [hashlib.sha256(path.read_bytes()).digest() for path in originals])
