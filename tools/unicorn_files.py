# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""DOS file bridge with optional copy-on-write game and drive overlays.

Original game and CD assets are never opened for writing. The caller must
explicitly provide a separate overlay root to enable guest writes.
"""

from pathlib import Path


class DosFileError(Exception):
    def __init__(self, code):
        super().__init__(f'DOS error {code}')
        self.code = code


class DosFiles:
    def __init__(self, root, cd_root=None, overlay_root=None, config_data=None):
        self.root = Path(root).resolve()
        self.cd_root = Path(cd_root).resolve() if cd_root is not None else None
        self.overlay_root = Path(overlay_root).resolve() if overlay_root is not None else None
        self.config_data = config_data
        self.handles = {}

    def _resolve(self, filename):
        if not filename or len(filename) >= 260 or any(ord(c) < 32 or ord(c) > 126 for c in filename):
            raise DosFileError(3)
        path = filename.replace('\\', '/').upper()
        if len(path) >= 2 and path[1] == ':':
            if path[0] != 'C':
                raise DosFileError(15)
            path = path[2:]
        absolute = path.startswith('/')
        path = path.lstrip('/')
        if absolute and path.startswith('EF2000/'):
            path = path[7:]
            absolute = False
        root = (self.cd_root if absolute else self.root)
        if root is None:
            raise DosFileError(2)
        parts = [part for part in path.split('/') if part != '.']
        if not parts or any(p in ('', '..') or ':' in p for p in parts):
            raise DosFileError(3)
        overlay_base = (self.overlay_root if absolute else self.overlay_root / 'EF2000') if self.overlay_root else None
        if overlay_base is not None:
            # Reuse existing uppercase/case-insensitive overlay entries, while
            # new guest paths get canonical uppercase DOS filenames.
            overlay_path = self._walk(overlay_base, parts, must_exist=False)
        else:
            overlay_path = None
        original = self._walk(root, parts, must_exist=True)
        return original, overlay_path

    @staticmethod
    def _walk(root, parts, must_exist):
        current = root
        for part in parts:
            try:
                match = next(child for child in current.iterdir() if child.name.upper() == part)
            except (StopIteration, OSError):
                if must_exist:
                    raise DosFileError(2) from None
                match = current / part
            current = match
            if not current.resolve().is_relative_to(root):
                raise DosFileError(3)
        if must_exist and not current.is_file():
            raise DosFileError(3)
        return current

    def open(self, filename, access=0):
        if access not in (0, 1, 2):
            raise DosFileError(5)
        if access and self.overlay_root is None:
            raise DosFileError(5)
        try:
            original, overlay_path = self._resolve(filename)
        except DosFileError as exc:
            if exc.code != 2 or self.overlay_root is None:
                raise
            # Overlay-only files must be accessible after creation.
            original, overlay_path = None, self._overlay_path(filename)
            if not overlay_path.is_file():
                raise
        source = overlay_path if overlay_path is not None and overlay_path.is_file() else original
        if source is None:
            raise DosFileError(2)
        try:
            data = bytearray(self.config_data if (self.config_data is not None and
                             source == original and source.name.upper() == 'EF2000.CFG')
                             else source.read_bytes())
        except OSError:
            raise DosFileError(5) from None
        return self._open_handle(data, access, overlay_path if access else None)

    def _overlay_path(self, filename):
        path = filename.replace('\\', '/').upper()
        if len(path) >= 2 and path[1] == ':':
            path = path[2:]
        absolute = path.startswith('/')
        path = path.lstrip('/')
        if absolute and path.startswith('EF2000/'):
            path, absolute = path[7:], False
        parts = [part for part in path.split('/') if part != '.']
        if not parts or any(p in ('', '..') or ':' in p for p in parts):
            raise DosFileError(3)
        if self.overlay_root is None:
            raise DosFileError(5)
        base = self.overlay_root if absolute else self.overlay_root / 'EF2000'
        return self._walk(base, parts, must_exist=False)

    def create(self, filename):
        path = self._overlay_path(filename)
        self._save(path, b'')
        return self._open_handle(bytearray(), 2, path)

    def _open_handle(self, data, access, overlay_path):
        for handle in range(5, 64):
            if handle not in self.handles:
                self.handles[handle] = [data, 0, access, overlay_path]
                return handle
        raise DosFileError(4)

    def read(self, handle, count):
        if handle not in self.handles:
            raise DosFileError(6)
        data, position, access, _path = self.handles[handle]
        if access == 1:
            raise DosFileError(5)
        chunk = data[position:position + count]
        self.handles[handle][1] += len(chunk)
        return bytes(chunk)

    def write(self, handle, data):
        if handle not in self.handles:
            raise DosFileError(6)
        buffer, position, access, path = self.handles[handle]
        if access == 0 or path is None:
            raise DosFileError(5)
        if position + len(data) > 0x4000000:
            raise DosFileError(5)
        if position > len(buffer):
            buffer.extend(b'\0' * (position - len(buffer)))
        end = position + len(data)
        buffer[position:end] = data
        self.handles[handle][1] = end
        return len(data)

    def seek(self, handle, position, whence):
        if handle not in self.handles:
            raise DosFileError(6)
        data, current, _access, _path = self.handles[handle]
        base = (0, current, len(data))[whence] if whence in (0, 1, 2) else None
        if base is None or base + position < 0:
            raise DosFileError(1)
        self.handles[handle][1] = base + position
        return base + position

    def close(self, handle):
        if handle not in self.handles:
            raise DosFileError(6)
        self.flush(handle)
        del self.handles[handle]

    def flush(self, handle):
        if handle not in self.handles:
            raise DosFileError(6)
        data, _position, access, path = self.handles[handle]
        if access and path is not None:
            self._save(path, data)

    def _save(self, path, data):
        if self.overlay_root is None or not path.resolve().is_relative_to(self.overlay_root):
            raise DosFileError(5)
        try:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        except OSError:
            raise DosFileError(5) from None


# Source compatibility for tools/tests using the original probe class name.
ReadOnlyDosFiles = DosFiles
