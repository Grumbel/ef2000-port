<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 132: runtime field setter

## `set_field_10677c` (`0x10677c`, 16 bytes)

Original body:

```text
8b 44 24 04       mov eax, [esp+4]
85 c0             test eax, eax
74 07             je 0x10678b
8b 54 24 08       mov edx, [esp+8]
89 50 3c          mov [eax+0x3c], edx
c3                ret
```

This cdecl leaf reads the object pointer and value from caller-owned stack
arguments at `+4` and `+8`. A null pointer does nothing; otherwise it stores
the value at object offset `0x3c`. The function returns with plain `RET`,
clobbers EAX/EDX, and preserves EBX/ECX/ESI/EDI/EBP. Its only direct caller is
the trace wrapper `0x102824@0x10284e`; that wrapper is reached from
`0xd7158@0xd721e`, `0xd7158@0xd725f`, and `0xd72b4@0xd734c`.

Both target and wrapper had been modeled with register inputs. Their confirmed
signatures, C declarations, wrapper definition and test specs now use the
caller-clean stack ABI. The differential schema's stack-argument support is
used to exercise both words without confusing argument bytes with callee
cleanup.

## Evidence and remaining gate

- The focused OpenWatcom fixture emits the exact original 16-byte body and
  splices in place. The production `runtime_helpers.c` and `misc_helpers2.c`
  modules compile with zero warnings.
- Four isolated cases pass: null pointer, zero store, replacing a nonzero
  value, and an unaligned pointer. The observed four-byte write and ABI state
  match.
- A captured actual call from the SB16-DMA cockpit path returns to `0x102853`
  and passes function differential (1/1), including the four-byte write.
  Captured stack arguments were object `0x00d8a92c`, value `0x00005622`.
- With the 17 verified patches and this candidate, a repeated SB16-DMA
  cockpit golden replay cannot pass the repeatability gate. Original runs
  differed by two interrupts (42,312 vs. 42,314); the patched run had 43,133
  interrupts, reached this target 27 times, and differed in the selected
  64 KiB BSS hash (`06ca2186…` vs. `54085171…`). This is the existing
  host-time-driven SB16-DMA variation documented in batch 126, so the
  candidate stays `spliced`.
- SB16-probe stops before the target at unsupported real-mode IN port `0xa1`
  (`0x1112b9`, 246 interrupts). It cannot provide a deterministic replay for
  this path with the current harness.

The replay used the 17 verified patches and manifest setup from
`docs/subsystems/decomp-batch-128.md`, plus
`local/decomp-batch-132-set-field/set-field.efpatch`; it ran with
`--sound sb16-dma --baseline-repeats 2`. Captures, replay outputs, compiler
objects and patches remain ignored under `local/`.

Commands:

```sh
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  tools/ef2000_wcc.py decomp/tests/ow_set_field_10677c.c \
  --compiler /nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386 \
  --output local/decomp-batch-132-set-field/set-field.obj
python3 tools/omf_inplace.py local/decomp-batch-132-set-field/set-field.obj \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --reviewed-flow 0x10677c --functions decomp/symbols/functions.tsv \
  --globals decomp/symbols/globals.tsv \
  -o local/decomp-batch-132-set-field/set-field.efpatch
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  tools/func_differential.py 0x10677c --spec decomp/tests/0x0010677c.json \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --patch local/decomp-batch-132-set-field/set-field.efpatch
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  tools/func_differential.py 0x10677c \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --patch local/decomp-batch-132-set-field/set-field.efpatch \
  --captured local/decomp-batch-132-set-field/calls/0x010677c-0000.efcap
python3 tools/decomp_status.py check
git diff --check
```

The isolated and captured differentials pass 4/4 and 1/1. Tracker check
passes for 4,287 functions. Original executable SHA-256:
`f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd`.
