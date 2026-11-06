<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 138: exchange object callback

## `exchange_callback_106938` (`0x106938`, 28 bytes)

Original disassembly from the SHA-pinned payload:

```text
8b 44 24 04       mov eax,[esp+4]
85 c0             test eax,eax
75 01              jne nonnull
c3                 ret
nonnull:
8b 5c 24 08       mov ebx,[esp+8]
8b 90 50 08 00 00  mov edx,[eax+0x850]
89 98 50 08 00 00  mov [eax+0x850],ebx
89 d0              mov eax,edx
c3                 ret
```

The object and callback are caller-clean cdecl stack arguments. EAX returns the
old callback; null objects return zero. The non-null path clobbers EBX and
EDX. The direct caller is `0x102dbc@0x102de8`, an AIL trace wrapper. Its
actual ABI is also two caller-clean stack arguments, and callers in
`0xd7158` and `0xd72b4` pass the object and callback. Corrected the wrapper's
C signature, header declaration, guest-call prototype, test spec, and both
source call sites.

## Evidence and remaining gate

- The OpenWatcom fixture emits the exact 28-byte body and splices in place.
- Five isolated cases pass: null object, nonzero replacement, replacement
  with zero, same value, and an unaligned object. The four non-null cases
  compare the exact four-byte write.
- Four captured live sound-channel calls pass differential (4/4).
- Sound-off menu/setup/cockpit replay does not execute the target. The
  SB16-DMA cockpit path reaches it six times, but the strict gate fails:
  repeated original baselines differ (42,342 vs. 42,372 interrupts, with
  different selected BSS); the patched run has 43,133 interrupts and a
  selected BSS hash mismatch. Keep the target `spliced`, not `verified`.
- `runtime_helpers.c`, `trace_wrappers1.c`, and `misc_drafts1.c` compile with
  OpenWatcom 2.0 and zero warnings. Focused differential/splicer tests pass
  12/12; tracker validation and `git diff --check` pass.

Original executable SHA-256:
`f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd`.
All captures, objects, patches and replay output stay under ignored `local/`.
