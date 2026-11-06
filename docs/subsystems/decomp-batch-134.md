<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 134: runtime multiply helper

## `times_100` (`0x106be8`, 18 bytes)

Original disassembly from the SHA-pinned payload:

```text
52                 push edx
89 c2              mov edx,eax
c1 e0 02           shl eax,2
29 d0              sub eax,edx
c1 e0 03           shl eax,3
01 d0              add eax,edx
c1 e0 02           shl eax,2
5a                 pop edx
c3                 ret
```

The function returns the low 32 bits of `EAX * 100` in EAX and preserves
EDX explicitly. It preserves EBX, ECX, ESI, EDI and EBP as well. Inspected
callers at `0xd9c30@0xd9c45`, `0xd9c30@0xd9c7e`, and
`0xd9cb4@0xd9cd5` set EAX to 1, 2, and 1 respectively. The multiplication
is modeled with unsigned arithmetic in C so signed overflow has the original
modulo-2^32 result.

## Evidence and remaining gate

- OpenWatcom 2.0 compiles `decomp/tests/ow_times_100.c` with zero warnings.
  The emitted body is `imul eax,100; ret`; it is five bytes, fits the
  18-byte extent, returns the correct low 32 bits, and does not touch EDX.
- OMF in-place splicing succeeds at `0x106be8`.
- The isolated original-vs-patched differential passes 8/8 cases, including
  signed extrema, negative inputs, and overflow, while checking EDX sentinels.
- The repeated-baseline menu/setup/cockpit replay preserves the existing
  checkpoint state but reaches `0x106be8` zero times in all three paths. The
  required-hit gate therefore fails. The entry remains `spliced`, not
  `verified`, until a deterministic caller path can be exercised.
- The original caller chain reaches `0xd9c30` from `0xe9ac0`, selected by
  the peripheral selector at `0x464472`; the ordinary replay configuration
  does not take this virtual-I/O path. No runtime hit or captured call is
  claimed.

The original executable SHA-256 is
`f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd`.
All objects, patches, and replay output remain ignored under `local/`.

Commands:

```sh
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  tools/ef2000_wcc.py decomp/tests/ow_times_100.c \
  --compiler /nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386 \
  --output local/decomp-batch-134-times-100/times-100.obj
python3 tools/omf_inplace.py local/decomp-batch-134-times-100/times-100.obj \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --reviewed-flow 0x106be8 --functions decomp/symbols/functions.tsv \
  --globals decomp/symbols/globals.tsv \
  -o local/decomp-batch-134-times-100/times-100.efpatch
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  tools/func_differential.py 0x106be8 \
  --spec decomp/tests/0x00106be8.json \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --patch local/decomp-batch-134-times-100/times-100.efpatch
python3 tools/decomp_status.py check
```

The full replay command and exact zero-hit summary are reproducible from
`local/decomp-batch-134-times-100/full-manifest.json` and
`local/decomp-batch-134-times-100/full-replay/summary.json` while local assets
remain available.
