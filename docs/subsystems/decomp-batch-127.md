<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 127: link pointer resolution

## `resolve_link_pointer` (`0x38658`, 26 bytes)

Original bytes disassembled from the SHA-pinned payload:

```text
53                         push ebx
85 d2                      test edx, edx
74 0b                      je 0x38668
8b 5a 15                   mov ebx, [edx+0x15]
85 db                      test ebx, ebx
74 04                      je 0x38668
89 d8                      mov eax, ebx
5b                         pop ebx
c3                         ret
98                         cwde
8b 04 85 b0 f5 19 00       mov eax, [eax*4+0x19f5b0]
5b                         pop ebx
c3                         ret
```

The inputs are signed index in AX and optional candidate pointer in EDX. If
EDX is nonzero and the unaligned dword at EDX+`0x15` is nonzero, return it in
EAX; otherwise sign-extend AX and read the corresponding raw four-byte entry
from `0x19f5b0`. The function preserves EBX; its observed callers also rely
on the other GPRs remaining unchanged.

The two inspected callers confirm the register inputs. `0x37498` at
`0x374ed` sign-extends CX into EAX and clears EDX before the call.
`refresh_link_pointer` (`0x3856c`) loads its signed record word into AX and
the current guest pointer into EDX at `0x38574`, sign-extends AX, then calls
at `0x38578`. Its caller saves/restores EBX and EDX around the helper and
stores returned EAX to the record's +`0x18` field.

The recovered body in `decomp/src/link_resolution.c` is ordinary typed C with
the packed `ef_link_candidate` layout. Guest builds cast guest offsets to
pointers directly; the `EF_DECOMP_HOST_TEST` path retains the guest-memory
translator. The OpenWatcom fixture emits one 23-byte code body inside the
26-byte original extent, with no unresolved call or data segment.

## Evidence

- OpenWatcom 2.0 compiled the production module and focused fixture with zero
  warnings. OMF splicing generated one in-place 26-byte patch; the original
  executable SHA-256 is
  `f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd`.
- Ten isolated cases pass: null candidate; signed negative and nonnegative
  indices; minimum signed AX; unaligned nonzero alternate; zero alternate
  fallback; and sentinel register values. One actual original cockpit call
  also passes the captured-state differential (1/1).
- The repeated-baseline 500M main menu / 700M quick-fight setup / 1.5B
  cockpit suite passes 3/3 with all previous verified patches and this
  replacement. `0x38658` is hit 16 times in cockpit. Selected 64 KiB BSS,
  framebuffer, status, interrupts, ticks and IRQ0 match.
- Making this real object spliceable exposed two narrow OMF assumptions:
  Watcom data externals have a leading underscore, and an absolute external
  offset framed against the proven-zero-base FLAT group is valid. The splicer
  now resolves that decoration and accepts this relocation; synthetic tests
  cover both, while unsupported frames remain rejected.
- The existing host runtime link/control regression passes with strict GCC
  warnings. `python3 -m unittest discover -s tests -p 'test_omf_splice.py' -v`
  passes 10 tests.
- Replay outputs and call capture are ignored under
  `local/decomp-batch-127-link-resolution/`.

Commands:

```sh
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  tools/ef2000_wcc.py decomp/src/link_resolution.c \
  --compiler /nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  tools/ef2000_wcc.py decomp/tests/ow_resolve_link_pointer.c \
  --compiler /nix/store/rs0hwdf6mhndmsyhc9z5gbgsa43ps8vg-open-watcom-v2-0-unstable-2026-04-11/bin/wcc386 \
  --output local/decomp-batch-127-link-resolution/resolve-link-pointer.obj
python3 tools/omf_inplace.py \
  local/decomp-batch-127-link-resolution/resolve-link-pointer.obj \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --reviewed-flow 0x38658 --functions decomp/symbols/functions.tsv \
  --globals decomp/symbols/globals.tsv \
  -o local/decomp-batch-127-link-resolution/resolve-link-pointer.efpatch
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  tools/func_differential.py 0x38658 --spec decomp/tests/0x00038658.json \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --patch local/decomp-batch-127-link-resolution/resolve-link-pointer.efpatch
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  tools/func_differential.py 0x38658 \
  --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --patch local/decomp-batch-127-link-resolution/resolve-link-pointer.efpatch \
  --captured local/decomp-batch-127-link-resolution/cockpit-probe/calls/0x0038658-0000.efcap
gcc -std=c11 -Wall -Wextra -Werror -DEF_DECOMP_HOST_TEST -Idecomp/include \
  tests/decomp_runtime_link_controls_test.c decomp/src/link_resolution.c \
  decomp/src/runtime_control.c -o /tmp/decomp-runtime-link-controls
/tmp/decomp-runtime-link-controls
python3 -m unittest discover -s tests -p 'test_omf_splice.py' -v
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
)
python3 tools/replay_suite.py --executable local/reference-01/drive/EF2000/_EF2000.EXE \
  --runner local/codex-build/ef2000-unicorn-cpp \
  --game-dir local/reference-01/drive/EF2000 --cd-dir local/cd-long/cd-audio \
  "${patches[@]}" \
  --manifest local/decomp-batch-127-link-resolution/full-manifest.json \
  --output local/decomp-batch-127-link-resolution/full-replay --baseline-repeats 2
python3 tools/decomp_status.py check
git diff --check
```
