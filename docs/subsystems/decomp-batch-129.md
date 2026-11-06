<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 129: runtime tick accessor

## `field_10dfe0` (`0x10dfe0`, 12 bytes)

Original instructions:

```text
a1 c6 0c 12 00          mov eax, [0x120cc6]
8b 80 6c 04 00 00       mov eax, [eax+0x46c]
c3                      ret
```

The function loads `g_runtime_segment_base` from `0x120cc6`, reads its dword
field at `+0x46c`, and returns it in EAX. It does not write memory. The
confirmed ABI preserves EBX, ECX, EDX, ESI, EDI and EBP. Callers are
`0xeab44@0xeab47` and `0xeac48@0xeadfa`.

The OpenWatcom-specific expression in `decomp/src/runtime_helpers.c` uses
the named absolute global. The focused fixture compiles without warnings and
splices to a safe 12-byte body. Five isolated differential cases pass,
including zero base, distinct values and a base at 49 MiB.

## Remaining verification boundary

An original cockpit call was captured at return address `0xeadff`. Its
captured global value is `0x05000000` (80 MiB). The current Unicorn runner
maps only `[0, 0x04000000)` for guest RAM; `tools/func_differential.py` has
the same 64 MiB limit. The captured differential therefore faults in the
original body at `0x10dfe5` when it reads `0x0500046c`. The runner's invalid
memory hook reports unmapped access and does not map or snapshot this X-32
segment. This is an emulator-memory-model limitation, not evidence that the
replacement differs.

Keep the tracker state `spliced`, not `verified`, until X-32 segment mapping
and live contents can be represented by both the original replay runner and
function differential. The full repeated replay has not been accepted as
evidence for this function.

## Commands and evidence

```sh
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  tools/ef2000_wcc.py decomp/tests/ow_field_10dfe0.c \
  --compiler /nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386 \
  --output local/decomp-batch-129-bios-tick/field-10dfe0.obj
python3 tools/omf_inplace.py \
  local/decomp-batch-129-bios-tick/field-10dfe0.obj \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --reviewed-flow 0x10dfe0 --functions decomp/symbols/functions.tsv \
  --globals decomp/symbols/globals.tsv \
  -o local/decomp-batch-129-bios-tick/field-10dfe0.efpatch
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  tools/func_differential.py 0x10dfe0 \
  --spec decomp/tests/0x0010dfe0.json \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --patch local/decomp-batch-129-bios-tick/field-10dfe0.efpatch
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  tools/func_differential.py 0x10dfe0 \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --patch local/decomp-batch-129-bios-tick/field-10dfe0.efpatch \
  --captured local/decomp-batch-129-bios-tick/cockpit-probe/calls/0x010dfe0-0000.efcap
```

The isolated differential passes 5/5. The captured differential fails at
`0x10dfe5` with `Invalid memory read (UC_ERR_READ_UNMAPPED)`, as expected from
the 80 MiB pointer and 64 MiB memory model. The SHA-256 of the original
`_EF2000.EXE` is
`f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd`.
All captures, objects and patches remain ignored under `local/`.
