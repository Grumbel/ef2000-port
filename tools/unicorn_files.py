# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Read-only game asset bridge for the experimental Unicorn guest.

No guest-generated filenames are passed directly to an OS open call. This
probe does not create writable overlays; mutation requests stop explicitly.
"""

from pathlib import Path


class DosFileError(Exception):
    def __init__(self, code):
        super().__init__(f'DOS error {code}')
        self.code = code


class ReadOnlyDosFiles:
    def __init__(self, root):
        self.root = Path(root).resolve()
        self.handles = {}

    def _resolve(self, filename):
        if not filename or len(filename) >= 260 or any(ord(c) < 32 or ord(c) > 126 for c in filename):
            raise DosFileError(3)
        path = filename.replace('\\', '/').upper()
        if len(path) >= 2 and path[1] == ':':
            if path[0] != 'C':
                raise DosFileError(15)
            path = path[2:]
        path = path.lstrip('/')
        if path.startswith('EF2000/'):
            path = path[7:]
        parts = path.split('/')
        if not parts or any(p in ('', '.', '..') or ':' in p for p in parts):
            raise DosFileError(3)
        current = self.root
        for part in parts:
            try:
                match = next(child for child in current.iterdir() if child.name.upper() == part)
            except (StopIteration, OSError):
                raise DosFileError(2) from None
            current = match
            if not current.resolve().is_relative_to(self.root):
                raise DosFileError(3)
        if not current.resolve().is_relative_to(self.root) or not current.is_file():
            raise DosFileError(3)
        return current

    def open(self, filename, access=0):
        if access != 0:
            raise DosFileError(5)
        path = self._resolve(filename)
        try:
            data = path.read_bytes()
        except OSError:
            raise DosFileError(5) from None
        for handle in range(5, 64):
            if handle not in self.handles:
                self.handles[handle] = [data, 0]
                return handle
        raise DosFileError(4)

    def read(self, handle, count):
        if handle not in self.handles:
            raise DosFileError(6)
        data, position = self.handles[handle]
        chunk = data[position:position + count]
        self.handles[handle][1] += len(chunk)
        return chunk

    def seek(self, handle, position, whence):
        if handle not in self.handles:
            raise DosFileError(6)
        data, current = self.handles[handle]
        base = (0, current, len(data))[whence] if whence in (0, 1, 2) else None
        if base is None or base + position < 0:
            raise DosFileError(1)
        self.handles[handle][1] = base + position
        return base + position

    def close(self, handle):
        if handle not in self.handles:
            raise DosFileError(6)
        del self.handles[handle]
