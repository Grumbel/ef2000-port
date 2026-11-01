# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""gamepkgstd DIR-4..12 user directories for the Linux-only EF2000 port."""

import os
from pathlib import Path


XDG = {
    'config': ('XDG_CONFIG_HOME', '.config'),
    'state': ('XDG_STATE_HOME', '.local/state'),
    'user_data': ('XDG_DATA_HOME', '.local/share'),
    'cache': ('XDG_CACHE_HOME', '.cache'),
}


def resolve_paths(overrides=None, portable=False, executable=None, environ=None):
    environ = os.environ if environ is None else environ
    overrides = overrides or {}
    if not portable and executable is not None:
        portable = (Path(executable).resolve().parent / 'portable.txt').is_file()
    paths = {}
    for category, (variable, default) in XDG.items():
        override = overrides.get(category)
        if override is not None:
            path, source = Path(override).expanduser().resolve(), 'option'
        elif portable:
            if executable is None:
                raise ValueError('portable mode requires a game executable')
            path, source = Path(executable).resolve().parent / 'userdata' / category.replace('user_data', 'data'), 'portable'
        elif environ.get(variable) and Path(environ[variable]).is_absolute():
            path, source = Path(environ[variable]) / 'ef2000', variable
        else:
            home = Path(environ.get('HOME', ''))
            if not environ.get('HOME') or not home.is_absolute():
                raise ValueError(f'no absolute {variable} or HOME for {category}')
            path, source = home / default / 'ef2000', 'default'
        paths[category] = (path, source)
    return paths


def format_paths(paths, data_dir=None):
    lines = [f'{category.replace("user_data", "user-data")}: {path} ({source})'
             for category, (path, source) in paths.items()]
    if data_dir:
        lines.insert(0, f'game-data: {data_dir} (option)')
    return '\n'.join(lines)
