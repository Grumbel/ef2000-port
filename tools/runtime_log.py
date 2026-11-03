# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Promote useful events from the full native transcript into the startup log."""

import time
from pathlib import Path


class RuntimeLog:
    def __init__(self, path, verbose=False):
        self.path, self.verbose = Path(path), verbose
        self.position = 0
        self.next_poll = 0
        self.context = {}

    def poll(self, force=False):
        if not force and time.monotonic() < self.next_poll:
            return
        self.next_poll = time.monotonic() + 0.25
        with self.path.open(errors='replace') as source:
            source.seek(self.position)
            lines = source.readlines()
            self.position = source.tell()
        for line in lines:
            line = line.rstrip()
            if line.startswith(('FILE OPEN ', 'FILE CREATE ')):
                name = line.split(' ', 2)[2]
                suffix = Path(name).suffix.upper()
                topic = {'.CG': 'campaign', '.MIS': 'mission', '.NAV': 'navigation'}.get(suffix)
                if topic and self.context.get(topic) != Path(name).stem:
                    self.context[topic] = Path(name).stem
                    print(f'game: {topic} data={name}', flush=True)
            if line.startswith('FILE CREATE '):
                print(f'files: game created {line[12:]} (drive overlay)', flush=True)
            elif line.startswith(('FAULT ', 'STOP ', 'EXIT ', 'EXCEPTION ENTER ')):
                print(f'runtime: {line}', flush=True)
            elif self.verbose and line.startswith(('FILE ', 'CONSOLE ', 'PIT ', 'REALMODE ', 'IO ', 'JOYSTICK ', 'GAMEPORT ')):
                print(f'trace: {line}', flush=True)
