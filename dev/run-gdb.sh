#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
: "${EF2000_SOURCE:?EF2000_SOURCE is unset; enter the shell with: nix develop}"
: "${EF2000_BUILD_DIR:?EF2000_BUILD_DIR is unset; enter the shell with: nix develop}"
bash "$EF2000_SOURCE/dev/build.sh"
exec gdb -q \
  -ex 'set pagination off' \
  -ex 'set confirm off' \
  -ex 'set debuginfod enabled off' \
  -ex 'set follow-fork-mode child' \
  -ex run \
  -ex 'python
try:
  if int(gdb.parse_and_eval("$_exitcode")) == 0:
    gdb.execute("quit")
except Exception:
  pass
' \
  --args python3 "$EF2000_SOURCE/tools/run_startup.py" \
  --runtime-objects "$EF2000_BUILD_DIR/objects" "$@"
