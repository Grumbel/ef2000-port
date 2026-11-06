<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 133: second runtime field setter

## `set_field_10682c` (`0x10682c`, 16 bytes)

Original body:

```text
8b 44 24 04       mov eax, [esp+4]
85 c0             test eax, eax
74 07             je 0x10683b
8b 54 24 08       mov edx, [esp+8]
89 50 30          mov [eax+0x30], edx
c3                ret
```

The object and value are caller-clean cdecl arguments at `[ESP+4]` and
`[ESP+8]`. A null pointer does nothing; otherwise the function stores the
value at object offset `0x30`. It clobbers EAX/EDX and preserves
EBX/ECX/ESI/EDI/EBP. The direct caller is the trace wrapper
`0x1028c0@0x1028ea`, which is called from `0xd7158@0xd7273`.

Both the setter and wrapper had been modeled with EAX/EDX inputs. Their
confirmed signatures, C declarations, wrapper definition and test specs now
use the actual stack ABI. The production C body has the same conditional
store behavior; an inline OpenWatcom `#pragma aux` lowering reproduces the
test/store prefix, and the C function supplies the original final `RET`.

## Evidence and remaining gate

- The focused OpenWatcom fixture emits the exact original 16-byte body and
  splices in place. Four isolated cases pass, including null, zero, nonzero
  replacement and unaligned object addresses. The observed write is exactly
  four bytes.
- A captured live call from `0x1028c0` returns to `0x1028ef` and passes
  function differential (1/1), including all observed memory writes and
  preserved registers. Captured arguments are object `0x00d8a92c`, value `1`.
- In the repeated sound-DMA cockpit replay, the patched run reaches
  `0x10682c` four times and `0x10677c` 27 times. However, the original
  baselines themselves differ by 42 interrupts (42,314 vs. 42,272). The
  patched run reports 43,133 interrupts and selected BSS hash
  `ead29174…`, while the original hash is `06ca2186…`. The replay therefore
  fails the strict golden gate for host-time-driven SB16-DMA variation.
  Keep `set_field_10682c` `spliced`, not `verified`.

The replay used the 17 verified patches from
`docs/subsystems/decomp-batch-128.md` plus the two adjacent setter patches
from batches 132 and 133, with `--sound sb16-dma --baseline-repeats 2`.
Captures, objects, patches and replay output remain under ignored `local/`.

Commands:

```sh
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  tools/ef2000_wcc.py decomp/tests/ow_set_field_10682c.c \
  --compiler /nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386 \
  --output local/decomp-batch-133-set-field/set-field.obj
python3 tools/omf_inplace.py local/decomp-batch-133-set-field/set-field.obj \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --reviewed-flow 0x10682c --functions decomp/symbols/functions.tsv \
  --globals decomp/symbols/globals.tsv \
  -o local/decomp-batch-133-set-field/set-field.efpatch
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  tools/func_differential.py 0x10682c --spec decomp/tests/0x0010682c.json \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --patch local/decomp-batch-133-set-field/set-field.efpatch
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  tools/func_differential.py 0x10682c \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --patch local/decomp-batch-133-set-field/set-field.efpatch \
  --captured local/decomp-batch-133-set-field/calls/0x010682c-0000.efcap
python3 tools/decomp_status.py check
git diff --check
```

The isolated and captured differentials pass 4/4 and 1/1. Tracker validation
passes for 4,287 functions. Original executable SHA-256:
`f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd`.
