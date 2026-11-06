<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 131: set runtime state

## `set_state_1_106634` (`0x106634`, 16 bytes)

Original disassembly:

```text
8b 44 24 04          mov eax, [esp+4]
85 c0                test eax, eax
74 07                je 0x106643
c7 40 04 01 00 00 00 mov dword [eax+4], 1
c3                   ret
```

This is a one-argument cdecl leaf with caller cleanup. It reads the pointer
from `[ESP+4]`, stores dword `1` at `object+4` when non-null, clobbers EAX,
and otherwise has no memory effect. Its direct call is from the trace wrapper
`0x102644@0x102667`, reached from `0x1115fc@0x11161e` when that object's
callback field at `+0x88c` is positive.

The behavioral C body is in `decomp/src/runtime_helpers.c`. The straightforward
OpenWatcom lowering is 17 bytes for a 16-byte extent because it duplicates
the return. Its guest branch uses an inline `#pragma aux` instruction sequence
for the original 15-byte test/store prefix; the enclosing cdecl C function
emits the final `ret`. The focused fixture compiles warning-free to the exact
original 16 bytes and splices safely.

## Evidence and reachability

- Four isolated guest cases pass: null pointer, zero-to-one store,
  replacement of a nonzero field, and unaligned object pointer. All observed
  guest writes and ABI-preserved registers match.
- The repeated-baseline suite with all 17 prior verified patches plus this
  candidate passes 3/3. The main-menu, setup and cockpit selected BSS hashes,
  frame hashes, status, ticks, interrupts and IRQ0 counters match. The entry
  hit count is zero in every case.
- Separate original captures at 500M startup instructions and 1.5B scripted
  cockpit instructions did not reach the entry. The tracker remains
  `spliced`; the gated positive callback-field path needs a replay that reaches
  it and a live captured-call differential.

The replay uses the 17-patch array and manifest from
`docs/subsystems/decomp-batch-128.md`, adding
`local/decomp-batch-130-get-state/get-state.efpatch` and
`local/decomp-batch-131-set-state/set-state.efpatch`. Artifacts remain under
ignored `local/`.

Checks:

```sh
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  tools/ef2000_wcc.py decomp/src/runtime_helpers.c \
  --compiler /nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386 \
  --output local/decomp-batch-131-set-state/runtime_helpers.obj
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  tools/ef2000_wcc.py decomp/tests/ow_set_state_1_106634.c \
  --compiler /nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386 \
  --output local/decomp-batch-131-set-state/set-state.obj
python3 tools/omf_inplace.py local/decomp-batch-131-set-state/set-state.obj \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --reviewed-flow 0x106634 --functions decomp/symbols/functions.tsv \
  --globals decomp/symbols/globals.tsv \
  -o local/decomp-batch-131-set-state/set-state.efpatch
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  tools/func_differential.py 0x106634 --spec decomp/tests/0x00106634.json \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --patch local/decomp-batch-131-set-state/set-state.efpatch
python3 tools/decomp_status.py check
git diff --check
```

Both OpenWatcom compilations have zero warnings; the isolated differential
passes 4/4; tracker check passes for 4,287 functions. Original executable
SHA-256: `f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd`.
