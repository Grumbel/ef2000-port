<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 108: short absolute-memory leaves

This batch investigated two compact math/string entries after the first two
OpenWatcom replacements were verified. Neither investigated entry is promoted
or marked verified.

## `store_bytes_46437d` (`0xe85e4`, 12 bytes)

Original body:

```asm
mov ds:0x46437e, al
mov byte ptr ds:0x46437d, dl
ret
```

The C body expresses both writes through guest-memory lvalues. Compiling a
focused version with the repository OpenWatcom flags emits `push ebx`, saves AL
in BL, loads each absolute address into EAX, writes through `[eax]`, restores
EBX, and returns. The body exceeds the original 12-byte extent and changes
register state. OpenWatcom `__based(void)` pointer spellings emitted the same
sequence. The speculative signature/test expansion was reverted; this remains
at its prior `compiles` status.

## `advance_counter_15f07d` (`0xdf2e4`, 15 bytes)

Original body:

```asm
mov al, ds:0x15f07d
inc al
and al, 7
mov ds:0x15f07d, al
ret
```

The focused OpenWatcom C build emits a stack spill of AL, two address loads,
and a callee save/restore around the direct memory update. This exceeds the
15-byte extent and does not preserve the original incidental register effects.
No assembly shim was accepted as a C replacement. Its status remains
`compiles` pending a C expression/codegen route that reproduces the ABI and
fits the reviewed extent.

## `flag_is_one_15f930` (`0xe4304`)

The entry bytes are a five-byte jump to `0xe44e2`, while the cataloged extent is
14 bytes and the named body appears to begin inside a larger code region. A
nearby caller at `0xac0e0` confirms the entry is called, but the destination's
instruction boundary and body ownership are not established well enough to
splice this address safely. This candidate was left untouched.

These results do not change the two verified OpenWatcom replacements or the
unresolved `encode_runtime_segment_offset` investigation. No game data or
Ghidra output was added to the repository.
