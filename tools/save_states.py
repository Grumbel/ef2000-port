# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Portable checkpoint bundles; never deserialize executable Python objects."""

import base64
from collections import deque
import json
import os
from pathlib import Path
import shutil
import tempfile
import zipfile
from datetime import datetime


def default_directory():
    configured = Path(os.environ.get('XDG_DATA_HOME', ''))
    data = configured if configured.is_absolute() else Path.home() / '.local' / 'share'
    return data / 'ef2000' / 'states'


def encode(value):
    if isinstance(value, (bytes, bytearray)):
        return {'bytes': base64.b64encode(value).decode()}
    if isinstance(value, (set, deque, tuple)):
        return list(value)
    raise TypeError(type(value).__name__)


def decode(value):
    if set(value) == {'bytes'}:
        return bytearray(base64.b64decode(value['bytes']))
    return value


class SaveStates:
    def __init__(self, directory, output, identity):
        self.directory, self.output, self.identity = Path(directory), Path(output), identity
        if self.directory.resolve().is_relative_to((self.output / 'drive').resolve()):
            raise ValueError('save-state directory must be outside the writable drive overlay')
        self.directory.mkdir(parents=True, exist_ok=True)
        self.labels = {}
        self.context = {}
        self.pending = None
        self.current = None

    def entries(self):
        return sorted(self.directory.glob('*.efstate'), reverse=True)

    def label(self, path):
        path = Path(path)
        try:
            stamp = path.stat().st_mtime_ns
            if self.labels.get(path, (None,))[0] != stamp:
                with zipfile.ZipFile(path) as archive:
                    info = json.loads(archive.read('metadata.json'))
                context = ' '.join(info.get('context', {}).values())
                label = f'{info["mode"]} {info["resolution"]} {context} - {info["created"][:19]}'
                self.labels[path] = stamp, label
            return self.labels[path][1]
        except (OSError, ValueError, KeyError, zipfile.BadZipFile):
            return path.stem

    def metadata(self, engine):
        width, height = engine.video_modes.get(engine.video_mode, (320, 200))
        mode = engine.game_phase
        return {'mode': mode, 'resolution': f'{width}x{height}',
                'created': datetime.now().astimezone().isoformat(timespec='seconds'),
                'context': self.context}

    def prepare_load(self, path):
        path = Path(path)
        with zipfile.ZipFile(path) as archive:
            metadata = json.loads(archive.read('metadata.json'))
            if metadata['identity'] != self.identity:
                raise ValueError('save state belongs to a different executable, runtime, or device configuration')
            native = archive.getinfo('checkpoint.bin')
            if not 0x4100000 <= native.file_size < 0x4200000:
                raise ValueError('invalid native checkpoint size')
            # Validate all names before modifying the disposable overlay.
            for name in archive.namelist():
                if name.startswith('drive/') and ('..' in Path(name).parts or Path(name).is_absolute()):
                    raise ValueError('unsafe save-state path')
            engine = json.loads(archive.read('engine.json'), object_hook=decode)
            engine['_save_context'] = metadata.get('context', {})
            # Fully read/CRC-check the bundle before changing the active state.
            with tempfile.TemporaryDirectory(prefix='.restore-', dir=self.output) as temporary:
                staged = Path(temporary)
                with archive.open('checkpoint.bin') as source, (staged / 'checkpoint.bin').open('wb') as target:
                    shutil.copyfileobj(source, target)
                for name in archive.namelist():
                    if name.startswith('drive/') and not name.endswith('/'):
                        target = staged / name
                        target.parent.mkdir(parents=True, exist_ok=True)
                        with archive.open(name) as source, target.open('wb') as destination:
                            shutil.copyfileobj(source, destination)
                drive = self.output / 'drive'
                drive.mkdir(exist_ok=True)
                # Preserve directory inodes: the native DOS service holds open
                # directory descriptors to drive/ and drive/EF2000/.
                for item in drive.rglob('*'):
                    if item.is_file() or item.is_symlink():
                        item.unlink()
                for item in (staged / 'drive').rglob('*'):
                    if item.is_file():
                        target = self.output / item.relative_to(staged)
                        target.parent.mkdir(parents=True, exist_ok=True)
                        item.replace(target)
                (staged / 'checkpoint.bin').replace(self.output / 'checkpoint.bin')
        self.current = path
        return engine

    def save(self, engine, path=None):
        metadata = self.metadata(engine)
        if path is None:
            stamp = datetime.now().strftime('%Y%m%d-%H%M%S-%f')
            context = '-'.join(self.context.values())
            context = ''.join(c if c.isalnum() or c == '-' else '-' for c in context)[:60]
            path = self.directory / f'{stamp}-{metadata["mode"]}-{metadata["resolution"]}{"-" + context if context else ""}.efstate'
        path = Path(path)
        fields = {key: value for key, value in vars(engine).items()
                  if key not in ('cpu', 'sound_device', 'cd_audio', 'io')}
        if getattr(engine, 'cd_audio', None):
            fields['cd_state'] = dict(position=engine.cd_audio.position(), end=engine.cd_audio.end,
                                      playing=engine.cd_audio.playing())
        fields['sound_state'] = engine.sound_device.save_state() if engine.sound_device else None
        temporary = path.with_suffix('.tmp')
        with zipfile.ZipFile(temporary, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=1) as archive:
            archive.writestr('metadata.json', json.dumps(dict(metadata, identity=self.identity)))
            archive.writestr('engine.json', json.dumps(fields, default=encode))
            archive.write(self.output / 'checkpoint.bin', 'checkpoint.bin')
            for item in (self.output / 'drive').rglob('*'):
                if item.is_file():
                    archive.write(item, item.relative_to(self.output))
        temporary.replace(path)
        self.current = path
        print(f'state: saved {path}', flush=True)

    def restore_engine(self, engine, fields):
        self.context = fields.pop('_save_context', {})
        cd = fields.pop('cd_state', None)
        if cd is not None and engine.cd_audio:
            import time
            engine.cd_audio.start = engine.cd_audio.cursor = cd['position']
            engine.cd_audio.end = cd['end']
            engine.cd_audio.started = time.monotonic() if cd['playing'] else None
            engine.cd_audio.generation += 1
        sound = fields.pop('sound_state')
        engine.__dict__.update(fields)
        # Per-request diagnostics use tuple keys and are not emulated state.
        engine.io = {}
        engine.scan_queue = deque(engine.scan_queue)
        engine.pressed_keys = set(engine.pressed_keys)
        engine.video_modes = {int(key): tuple(value) for key, value in engine.video_modes.items()}
        if sound is not None:
            engine.sound_device.restore_state(sound)
        engine.release_keys()
        engine.mouse_buttons = 0
        print(f'state: loaded {self.current}', flush=True)
