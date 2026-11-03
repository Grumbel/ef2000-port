#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Launch the experimental original-code Unicorn backend with an SDL3 window."""

try:
    from .unicorn_backend import main
except ImportError:
    from unicorn_backend import main


if __name__ == '__main__':
    raise SystemExit(main(interactive=True))
