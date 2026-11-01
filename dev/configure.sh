#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
: "${EF2000_SOURCE:?EF2000_SOURCE is unset; enter the shell with: nix develop}"
: "${EF2000_BUILD_DIR:?EF2000_BUILD_DIR is unset; enter the shell with: nix develop}"

cmake -S "$EF2000_SOURCE" -B "$EF2000_BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Debug}" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "$@"

if [[ ! -e "$EF2000_SOURCE/build" && ! -L "$EF2000_SOURCE/build" ]]; then
  ln -s "$EF2000_BUILD_DIR" "$EF2000_SOURCE/build"
fi
