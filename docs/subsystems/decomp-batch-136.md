<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 136: exchange audio setting slot

## `exchange_slot_1041e4` (`0x1041e4`, 25 bytes)

Original disassembly from the SHA-pinned payload:

```text
8b 44 24 04       mov eax,[esp+4]                 ; index
8b 5c 24 08       mov ebx,[esp+8]                 ; new value
8b 14 85 cc d7 4d 00  mov edx,[0x4dd7cc+eax*4]
89 1c 85 cc d7 4d 00  mov [0x4dd7cc+eax*4],ebx
89 d0             mov eax,edx
c3                ret
```

The function takes two caller-clean stack arguments, returns the previous
slot value in EAX, and intentionally clobbers EBX and EDX. It preserves ECX,
ESI, EDI and EBP. `0x101e14` calls it with index/value at
`0x101e14@0x101e40`. That trace wrapper was also mis-modeled: the original
accepts two stack arguments, returns the old value in EAX, and is called from
both `0xd6e58` (sound setup) and `0x104580` (19-slot settings reset). The
wrapper signature, C declaration, guest-call prototype and source call sites
now use the two-argument cdecl ABI.

## Verification evidence

- The OpenWatcom fixture emits the original 25-byte body, including its EBX
  clobber. In-place OMF splicing succeeds.
- Four isolated table-swap cases pass, including slot 0, slot 5, the final
  slot 18, and storing the existing value. Each observes the expected
  four-byte write and previous value.
- Four captured original startup calls pass function differential (4/4).
  The captured calls are from the 19-setting startup initialization path.
- Repeated-baseline main-menu, quick-fight setup and cockpit replays pass
  3/3 with matching selected BSS, framebuffer and execution counters. The
  candidate is reached 19 times in each scenario.
- `runtime_helpers.c`, `trace_wrappers1.c`, and `misc_drafts1.c` compile with
  OpenWatcom 2.0 and zero warnings. The focused differential/splicer tests
  pass 12/12. Tracker validation and `git diff --check` pass.

The source executable SHA-256 is
`f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd`.
Captures, objects, patch and replay output remain under ignored `local/`.

Commands:

```sh
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  tools/ef2000_wcc.py decomp/tests/ow_exchange_slot_1041e4.c \
  --compiler /nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386 \
  --output local/decomp-batch-136-exchange/exchange.obj
python3 tools/omf_inplace.py local/decomp-batch-136-exchange/exchange.obj \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --reviewed-flow 0x1041e4 --functions decomp/symbols/functions.tsv \
  --globals decomp/symbols/globals.tsv \
  -o local/decomp-batch-136-exchange/exchange.efpatch
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  tools/func_differential.py 0x1041e4 \
  --spec decomp/tests/0x001041e4.json \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --patch local/decomp-batch-136-exchange/exchange.efpatch
# The startup runner used --capture-calls 0x1041e4:4; captured cases 0..3
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  -m unittest tests.test_func_differential tests.test_omf_inplace -v
python3 tools/decomp_status.py check
git diff --check
```

The full replay used all 17 previously verified patches plus this patch and
`local/decomp-batch-128-refresh-link/full-manifest.json`, requiring
`0x1041e4` in every case. It passed 3/3 with 19 hits per scenario.
