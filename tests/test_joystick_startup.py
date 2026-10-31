# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Original digital-sound startup must preserve usable joystick calibration."""

import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile
import unittest


@unittest.skipUnless(os.environ.get('EF2000_EXE') and platform.machine() == 'x86_64'
                     and all(shutil.which(name) for name in ('gcc', 'as', 'ld')),
                     'requires EF2000_EXE and native development tools')
class JoystickStartupTests(unittest.TestCase):
    def test_sound_startup_preserves_original_joystick_ranges(self):
        root = Path(__file__).resolve().parent.parent
        executable = Path(os.environ['EF2000_EXE']).resolve()
        original = (executable.parent / 'EF2000.CFG').read_bytes()
        script = '''
import os, struct
from tools import real_mode, run_startup
spawn, initialize, execute = (real_mode.subprocess.Popen,
                              real_mode.RealMode.__init__, real_mode.RealMode.execute)
observed = {}
def popen(command, *args, **kwargs):
    child = spawn(command, *args, **kwargs)
    if str(command[0]).endswith('/startup.elf'):
        observed['pid'] = child.pid
    return child
def init(self, *args, **kwargs):
    initialize(self, *args, **kwargs)
    self.joystick_axes = [0, 0, 0, 0]
def check(self, *args):
    result = execute(self, *args)
    if self.video_mode == 0x100:
        with open(f"/proc/{observed['pid']}/mem", 'rb', buffering=0) as memory:
            # D6969h/D6977h skip joystick polling when either range is zero.
            ranges = [struct.unpack('<H', os.pread(memory.fileno(), 2, 0x10000000 + address))[0]
                      for address in (0x420d96, 0x420d9e)]
        assert all(ranges), ranges
        observed['checked'] = True
    return result
real_mode.subprocess.Popen = popen
real_mode.RealMode.__init__ = init
real_mode.RealMode.execute = check
run_startup.main()
assert observed.get('checked'), observed
'''
        with tempfile.TemporaryDirectory(prefix='ef2000-joystick-') as temporary:
            output = Path(temporary) / 'run'
            result = subprocess.run(
                [sys.executable, '-c', script, str(executable), '--output', str(output),
                 '--controls', 'joystick', '--sound', 'sb16', '--headless', '--seconds', '16'],
                cwd=root, capture_output=True, text=True, timeout=35)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('GAMEPORT ', (output / 'calls.log').read_text())
            self.assertIn(b'JOY_FACTOR=100\r\n', (output / 'drive/EF2000/EF2000.CFG').read_bytes())
        self.assertEqual((executable.parent / 'EF2000.CFG').read_bytes(), original)
