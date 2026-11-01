#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Render a representative host debug overlay screenshot without proprietary assets."""

from pathlib import Path
from types import SimpleNamespace

from PIL import Image

try:
    from .debug_menu import DebugMenu
except ImportError:
    from debug_menu import DebugMenu

root = Path(__file__).resolve().parent.parent
display = SimpleNamespace(fullscreen=False, aspect_ratio='4:3', smooth_scaling=False)
menu = DebugMenu(display, None)
menu.open = True
pixels = menu.render(bytes([15, 33, 49]) * (640 * 400), 640, 400)
Image.frombytes('RGB', (640, 400), pixels).save(root / 'data/overlay-screenshot.png')
