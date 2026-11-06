<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 128: refresh link pointer

## `refresh_link_pointer` (`0x3856c`, 23 bytes)

Original disassembly from the SHA-pinned payload:

```text
53                         push ebx
52                         push edx
89 c3                      mov ebx, eax
66 8b 40 16                mov ax, [eax+0x16]
8b 53 18                   mov edx, [ebx+0x18]
98                         cwde
e8 db 00 00 00             call 0x38658
89 43 18                   mov [ebx+0x18], eax
5a                         pop edx
5b                         pop ebx
c3                         ret
```

EAX supplies a record pointer. The function reads signed index at +`0x16`
and current pointer at +`0x18`, calls the now-verified
`resolve_link_pointer` (`0x38658`) with AX/EDX, writes returned EAX back to
+`0x18`, returns it in EAX, and preserves EBX and EDX.

Three inspected caller sites are `0x37fe4@0x3818c`,
`0x381a0@0x38276`, and `0x4aa4c@0x4ae20`. The captured live call's return
address is `0x3827b`, confirming the second path in the original executable.

The source C body in `decomp/src/link_resolution.c` compiles through the
single-entry fixture `decomp/tests/ow_refresh_link_pointer.c`. It emits a
23-byte in-place body, including the resolved relative call to `0x38658`.

## Evidence

- OpenWatcom 2.0 fixture compilation has zero warnings; OMF splicing produces
  a safe in-place 23-byte patch at `0x3856c`.
- Two isolated original-vs-patched cases pass: table fallback and candidate
  alternate override. The captured original call from caller `0x381a0` passes
  (1/1), including the four-byte record write and full observed register
  state.
- With all 16 previous verified patches and this replacement, repeated
  baseline menu/setup/cockpit replay passes 3/3. The cockpit run reaches
  `0x3856c` eight times and `0x38658` 16 times; selected 64 KiB BSS,
  framebuffer, status, interrupt, tick and IRQ0 state match.
- The SHA-256 of the original `_EF2000.EXE` is
  `f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd`.
  Captures and replay outputs are ignored under
  `local/decomp-batch-128-refresh-link/`.

Commands:

```sh
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  tools/ef2000_wcc.py decomp/tests/ow_refresh_link_pointer.c \
  --compiler /nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386 \
  --output local/decomp-batch-128-refresh-link/refresh-link-pointer.obj
python3 tools/omf_inplace.py \
  local/decomp-batch-128-refresh-link/refresh-link-pointer.obj \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --reviewed-flow 0x3856c --functions decomp/symbols/functions.tsv \
  --globals decomp/symbols/globals.tsv \
  -o local/decomp-batch-128-refresh-link/refresh-link-pointer.efpatch
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  tools/func_differential.py 0x3856c --spec decomp/tests/0x0003856c.json \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --patch local/decomp-batch-128-refresh-link/refresh-link-pointer.efpatch
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  tools/func_differential.py 0x3856c \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --patch local/decomp-batch-128-refresh-link/refresh-link-pointer.efpatch \
  --captured local/decomp-batch-128-refresh-link/cockpit-probe/calls/0x003856c-0000.efcap
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
  --patch local/decomp-batch-127-link-resolution/resolve-link-pointer.efpatch
  --patch local/decomp-batch-128-refresh-link/refresh-link-pointer.efpatch
)
python3 tools/replay_suite.py \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --runner local/codex-build/ef2000-unicorn-cpp \
  --game-dir local/reference-01/drive/EF2000 --cd-dir local/cd-long/cd-audio \
  "${patches[@]}" \
  --manifest local/decomp-batch-128-refresh-link/full-manifest.json \
  --output local/decomp-batch-128-refresh-link/full-replay \
  --baseline-repeats 2
python3 tools/decomp_status.py check
git diff --check
```
