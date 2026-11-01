#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
: "${EF2000_SOURCE:?EF2000_SOURCE is unset; enter the shell with: nix develop}"
: "${EF2000_BUILD_DIR:?EF2000_BUILD_DIR is unset; enter the shell with: nix develop}"

cache="$EF2000_BUILD_DIR/CMakeCache.txt"
cached_source=""
if [[ -f "$cache" ]]; then
  cached_source="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$cache")"
fi
if [[ "$cached_source" != "$EF2000_SOURCE" || ! -d "$cached_source" ]]; then
  bash "$EF2000_SOURCE/dev/configure.sh"
fi
cmake --build "$EF2000_BUILD_DIR" "$@"
