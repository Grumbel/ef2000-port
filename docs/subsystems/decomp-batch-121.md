<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 121: set runtime phase five

## `set_runtime_phase_five` (`0x3dcc0`, 10 bytes)

The SHA-pinned original executable (`f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd`)
contains:

```text
66 c7 05 ec 35 1a 00 05 00    mov word [0x1a35ec], 5
c3                            ret
```

The routine has no arguments, writes exactly the word at `0x1a35ec`, returns
with all general registers and EFLAGS unchanged, and uses plain `ret` with no
stack arguments. The two direct caller functions are `0x3dd30` at `0x3ddbe`
and `0x8d11c` at `0x8d15d`. At the first callsite the instruction bytes are
`e8 fd fe ff ff`; at the second they are `e8 5e 0b fb ff`, both resolving to
`0x3dcc0`.

The OpenWatcom branch uses a private `#pragma aux` direct absolute-word
store inside `set_runtime_phase_five`; the non-Watcom path retains the typed
global assignment. The production module and standalone fixture compile
without warnings. The compiled replacement body is the same ten bytes as the
original, so it fits the original extent without a trampoline or padding.

## Differential and replay evidence

- OpenWatcom 2.0 commands:

  ```sh
  python3 tools/ef2000_wcc.py --compiler /nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386 decomp/src/runtime_control.c
  python3 tools/ef2000_wcc.py --compiler /nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386 --output local/decomp-batch-121-phase-five/fixture.obj decomp/tests/ow_set_runtime_phase_five.c
  ```

- The SHA-bound OMF patch is `local/decomp-batch-121-phase-five/phase-five.efpatch`
  (SHA-256 `7c815ee98164270448bc416e18f02e40d02f2fc598ba43fa23e1b404caf5b49f`).
  With the Unicorn Python environment, the 37-case isolated differential and
  a captured cockpit call both pass:

  ```sh
  /nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 tools/func_differential.py 0x3dcc0 --executable local/reference-01/drive/EF2000/_EF2000.EXE --patch local/decomp-batch-121-phase-five/phase-five.efpatch --spec decomp/tests/0x0003dcc0.json
  /nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 tools/func_differential.py 0x3dcc0 --executable local/reference-01/drive/EF2000/_EF2000.EXE --patch local/decomp-batch-121-phase-five/phase-five.efpatch --captured local/decomp-batch-121-phase-five/full-replay-correct/cockpit/original/calls/0x003dcc0-0000.efcap
  ```

  Every case compares all ABI registers and the complete observed memory
  change; only the target two-byte word is written.
- The repeated-baseline replay suite passes menu (500M instructions), setup
  (700M) and cockpit (1.5B), using the eight previously verified patches,
  the fixture-derived current encoder patch, and this candidate. The cockpit
  candidate is hit once. Original/repeated-original/patched agree on the
  selected 64 KiB BSS hash, framebuffer, stop status, interrupts, ticks and
  IRQ0. The suite summary is under ignored
  `local/decomp-batch-121-phase-five/full-replay-correct/summary.json`.
- `python3 tools/decomp_status.py check` passes after the tracker update.

The replay compares the selected BSS window and observable checkpoint
outputs, not all 64 MiB at gameplay checkpoints. The actual captured call
and full register/memory differential cover the candidate boundary.

The tracker state is `verified`.
