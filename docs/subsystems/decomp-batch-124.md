<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 124: copy active buffer pointer

## `copy_active_buffer_pointer` (`0x7a428`, 11 bytes)

The original body is:

```text
a1 c4 af 12 00    mov eax, [0x12afc4]
a3 9c 8e 1f 00    mov [0x1f8e9c], eax
c3                ret
```

It reads one dword, copies it to the destination dword, and returns that
value in EAX. All other GPRs and EFLAGS are unchanged. The two distinct
callers inspected were `0x7a738` at `0x7a744` and `0x7a844` at `0x7a911`.
Both immediately load a separate map pointer into EAX after the call, so the
return value is dead at those callsites, while the global destination copy
is the required side effect.

The Watcom branch uses a private aux helper with `__value [eax]` and
`__modify [eax]` to express the two absolute guest addresses without an
unresolved `_g_buffer_pointer_*` relocation. The ordinary typed global
assignment remains the non-Watcom path. OpenWatcom emits the exact original
11-byte sequence, with no additional stack or scratch-register effects.

## Differential and replay evidence

- OpenWatcom 2.0 compiled production `runtime_control.c` and the focused
  fixture with zero warnings. The OMF plan contains a single in-place write
  at `0x7a428` with the exact original body.
- The 37-case differential varies all GPRs, the source value, source/dest
  sentinels and EAX return values. All cases pass; only the destination dword
  changes.
- A captured actual cockpit call passes original-vs-patched guest
  differential (**1/1**).
- The repeated-baseline 500M menu / 700M setup / 1.5B cockpit replay passes
  all three cases with the eleven previous verified replacements plus this
  candidate. `0x7a428` is hit **320,002 times** in cockpit. The selected
  64 KiB BSS hash, framebuffer, stop status, interrupts, ticks and IRQ0 match
  the repeated original. The suite summary, capture and patch remain in
  ignored `local/decomp-batch-124-copy-buffer/`.
- The SHA-256 of the fixture patch is
  `b6fcd1e331115258c7786320a11324e3a792487d49520fa383baaee1c0b1937d`.
  `python3 tools/decomp_status.py check` passes after the tracker update.

Gameplay replay checks the selected BSS window and checkpoint outputs, not
all guest RAM. The isolated and captured call differentials cover the
function's complete ABI-visible boundary.

The tracker state is `verified`.
