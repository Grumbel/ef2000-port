<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 135: pause object state

## `pause_object_10687c` (`0x10687c`, 22 bytes)

Original disassembly from the SHA-pinned payload:

```text
8b 44 24 04       mov eax,[esp+4]
85 c0             test eax,eax
74 0d             je 0x106891
83 78 04 04       cmp dword [eax+4],4
75 07             jne 0x106891
c7 40 04 08 00 00 00  mov dword [eax+4],8
c3                ret
```

The parameter is a caller-clean cdecl stack pointer. Null pointers and states
other than 4 leave guest memory unchanged; state 4 becomes 8. EAX is a
scratch/result register; the function preserves EBX, ECX, EDX, ESI, EDI and
EBP. The only known direct caller is the cdecl trace wrapper
`0x1027a0@0x1027c3`. That wrapper is called from `0xd7490@0xd74c1`; its path
is gated by the global flag at `0x15bac4`. Both the helper and wrapper
signatures and source declarations now use `EF_CDECL`.

## Evidence and remaining gate

- The production `runtime_helpers.c` and `misc_helpers2.c` both compile with
  OpenWatcom 2.0 and zero warnings.
- The focused fixture uses a `#pragma aux` instruction sequence to keep the
  22-byte body within its reviewed in-place extent. Disassembly matches the
  original body byte for byte, including caller-clean `RET`.
- The in-place OMF patch succeeds. Five isolated original-vs-patched cases
  pass: null, state 4, state 8, another state, and an unaligned object. The
  state-4 cases compare the four-byte write.
- A repeated-baseline SB16-DMA cockpit replay has zero hits at `0x10687c`.
  Baseline interrupts differ (42,279 vs. 42,324); the patched run reports
  43,133 and the selected BSS hash differs. This path does not satisfy the
  runtime gate, so retain `spliced`, not `verified`. No live captured call is
  claimed.

Commands:

```sh
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  tools/ef2000_wcc.py decomp/tests/ow_pause_object_10687c.c \
  --compiler /nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386 \
  --output local/decomp-batch-135-pause/pause.obj
python3 tools/omf_inplace.py local/decomp-batch-135-pause/pause.obj \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --reviewed-flow 0x10687c --functions decomp/symbols/functions.tsv \
  --globals decomp/symbols/globals.tsv \
  -o local/decomp-batch-135-pause/pause.efpatch
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  tools/func_differential.py 0x10687c \
  --spec decomp/tests/0x0010687c.json \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --patch local/decomp-batch-135-pause/pause.efpatch
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  -m unittest tests.test_func_differential tests.test_omf_inplace -v
python3 tools/decomp_status.py check
git diff --check
```

The full sound-DMA replay inputs and report remain under ignored
`local/decomp-batch-135-pause/`.
