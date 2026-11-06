<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 126: control mode B/C

## `set_control_mode_b` (`0x810d0`, 25 bytes)

The original saves EDX, reads the mode gate byte at `0x15bac7`, selects AL if
the gate is nonzero and zero otherwise, clears EAX and returns the selected
byte zero-extended in EAX. It stores that byte at `0x12b104`, restores EDX,
and returns. The other GPRs are preserved. The final flags come from
`xor eax,eax`.

The inspected callers are `0x3dd30` at `0x3dd9f` and `0x81110` at `0x8114c`.
The source uses a private OpenWatcom `#pragma aux` sequence for the original
25-byte body because its high-level C lowering did not fit the function
extent; the non-Watcom fallback expresses the gate and latch as typed C.
`ef_abi.h` declares the full EAX return as `uint32_t`.

## `set_control_mode_c` (`0x810f4`, 25 bytes)

This entry uses the same gate and selection behavior, storing to `0x12b105`.
It preserves EDX and the other GPRs and returns the selected byte in EAX.
Its inspected callers are `0x3dd30` at `0x3ddb9` and `0x81110` at
`0x81153`. It uses the corresponding private OpenWatcom aux sequence and a
typed C fallback.

## Evidence

- OpenWatcom 2.0 compiled the production source and separate B/C fixtures
  with zero warnings. Each OMF fixture splices to the exact original 25-byte
  body. The separate objects are required because the splice format accepts
  one public code entry at offset zero per object.
- The original-vs-patched isolated tests pass 37/37 for each function,
  varying AL, the gate byte, full GPR sentinels and destination memory.
- Two independently captured original calls to B from repeated cockpit
  baselines pass the captured-state differential (2/2). C was not called in
  either capture.
- The repeated-baseline 500M menu / 700M setup / 1.5B cockpit replay passes
  3/3 with all preceding verified patches and both candidates. The 64 KiB
  BSS hashes, framebuffer hashes, status and execution counters match.
  `0x810d0` is reached once in cockpit. `0x810f4` is never reached; the
  manifest therefore does not require a C hit. Keep C `spliced` until a
  deterministic run reaches one of its gated callers and yields a captured
  original call.
- Full replay summary and call captures are under ignored
  `local/decomp-batch-126-control-bc/full-replay-rerun/`. The executable
  SHA-256 was verified as
  `f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd`.

The tracker marks B `verified` and C `spliced`; no C live-hit claim is made.

The following commands compiled the current production source and fixtures,
regenerated both patches, ran isolated and captured-call differentials, and
produced the replay summary described above:

```sh
python3 tools/ef2000_wcc.py decomp/src/runtime_control.c \
  --compiler /nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386
python3 tools/ef2000_wcc.py decomp/tests/ow_set_control_mode_b.c \
  --compiler /nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386 \
  --output local/decomp-batch-126-control-bc/recheck-b.obj
python3 tools/ef2000_wcc.py decomp/tests/ow_set_control_mode_c.c \
  --compiler /nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386 \
  --output local/decomp-batch-126-control-bc/recheck-c.obj
python3 tools/omf_inplace.py local/decomp-batch-126-control-bc/recheck-b.obj \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --reviewed-flow 0x810d0 --functions decomp/symbols/functions.tsv \
  --globals decomp/symbols/globals.tsv \
  -o local/decomp-batch-126-control-bc/recheck-b.efpatch
python3 tools/omf_inplace.py local/decomp-batch-126-control-bc/recheck-c.obj \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --reviewed-flow 0x810f4 --functions decomp/symbols/functions.tsv \
  --globals decomp/symbols/globals.tsv \
  -o local/decomp-batch-126-control-bc/recheck-c.efpatch
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  tools/func_differential.py 0x810d0 --spec decomp/tests/0x000810d0.json \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --patch local/decomp-batch-126-control-bc/control-mode-b.efpatch
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  tools/func_differential.py 0x810f4 --spec decomp/tests/0x000810f4.json \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --patch local/decomp-batch-126-control-bc/control-mode-c.efpatch
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  tools/func_differential.py 0x810d0 \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --patch local/decomp-batch-126-control-bc/control-mode-b.efpatch \
  --captured local/decomp-batch-126-control-bc/full-replay-rerun/cockpit/original/calls/0x00810d0-0000.efcap \
  --captured local/decomp-batch-126-control-bc/full-replay-rerun/cockpit/repeat-original/calls/0x00810d0-0000.efcap
patches=(
  --patch local/patches/ow-lower.efpatch
  --patch local/patches/ow-uppercase.efpatch
  --patch local/decomp-batch-121-phase-five/encoder-current.efpatch
  --patch local/decomp-batch-117/read-runtime-slot-flag.efpatch
  --patch local/decomp-batch-117/initialize-runtime-probe-latch.efpatch
  --patch local/decomp-batch-117/init-bounds-d0b68.efpatch
  --patch local/decomp-batch-118-runtime-slots/sync-flag-12ec8.efpatch
  --patch local/decomp-batch-120-reset-counter/reset-counter.efpatch
  --patch local/decomp-batch-121-phase-five/phase-five.efpatch
  --patch local/decomp-batch-122-runtime-store/store-runtime-control.efpatch
  --patch local/decomp-batch-123-mode-limits/mode-limits.efpatch
  --patch local/decomp-batch-124-copy-buffer/copy-buffer.efpatch
  --patch local/decomp-batch-125-control-a/control-a.efpatch
  --patch local/decomp-batch-126-control-bc/control-mode-b.efpatch
  --patch local/decomp-batch-126-control-bc/control-mode-c.efpatch
)
python3 tools/replay_suite.py \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --runner local/decomp-batch-126-control-bc/capture-runner.sh \
  --game-dir local/reference-01/drive/EF2000 --cd-dir local/cd-long/cd-audio \
  "${patches[@]}" \
  --manifest local/decomp-batch-126-control-bc/full-manifest.json \
  --output local/decomp-batch-126-control-bc/full-replay-rerun \
  --baseline-repeats 2
python3 tools/decomp_status.py check
git diff --check
```
