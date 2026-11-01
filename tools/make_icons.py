#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Regenerate standard hicolor icon sizes from the source SVG (requires FFmpeg)."""

from pathlib import Path
import subprocess


root = Path(__file__).resolve().parent.parent
source = root / 'data/icons/ef2000.svg'
for size in (16, 32, 48, 64, 128, 256):
    target = root / f'data/icons/hicolor/{size}x{size}/apps/ef2000.png'
    target.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y',
                    '-i', str(source), '-vf', f'scale={size}:{size}:flags=lanczos',
                    '-frames:v', '1', str(target)], check=True)

# SDL3's built-in BMP loader avoids adding SDL_image to the runtime dependencies.
subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y',
                '-i', str(source), '-vf', 'scale=32:32:flags=lanczos',
                '-frames:v', '1', str(root / 'data/icons/ef2000.bmp')], check=True)
