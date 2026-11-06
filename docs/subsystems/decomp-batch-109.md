<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 109: refine the segment encoder C codegen

## `encode_runtime_segment_offset` (`0x604`, 36 bytes)

The production C now forms the offset first and adds the shifted segment:

```c
uint32_t encoded = delta & 0x0fu;
encoded += ((delta >> 4) & 0xffffu) << 16;
return encoded;
```

OpenWatcom emits a 33-byte body, padded to the reviewed 36-byte extent. Its
final `ADD` matches the original arithmetic flags. The body still saves EDX
but does not save EBX, so its stack writes are not byte-identical to the
original. Both direct call sites (`0xb1f`, `0xddd8e`) continue to pass EAX and
consume EAX.

The focused OpenWatcom compile completed without warnings. The in-place
SHA-bound OMF patch passed **1,024/1,024** differential cases with the complete
EFLAGS word compared. The spec limits excluded callee scratch to 8 bytes
below incoming ESP. With no scratch allowance the original and candidate
instead differ at stack address `0x3ffeff8`, confirming that larger default
exclusions were unnecessary for the isolated test.

A 500M-instruction main-menu replay passed two original runs and one patched
run. Status, interrupt/tick counts, IP, framebuffer and selected 64 KiB guest
RAM matched; the patched run reached the encoder seven times. A longer run
using the existing menu/setup/cockpit manifest completed the main-menu and
quick-fight setup cases before interruption. The follow-up cockpit-only run
completed, but its selected `0x195fb0:65536` guest RAM hash differs
(`93aec906…` original, `fc712136…` patched); status, interrupt/tick counts,
final IP, and framebuffer hash match, and the patched run reached the encoder
seven times. This is a behavioral mismatch, not a passing cockpit replay.

The 20M full-RAM replay still differs. Hash subdivision localized the
mismatches to `0x3ffecc4..0x3ffecf9`, within the high X-32 stack area. At that
checkpoint original and patched runs have the same interrupt/tick counts, IP
and ESP (`0x3ffecf8`), but one differing word is at ESP and other stack words
differ below it. The run therefore does not prove that all mismatches are dead
callee scratch. The tracker remains `spliced` until this replay difference is
explained or the standard full suite supplies stronger evidence.

A temporary C-plus-inline-assembly experiment explicitly pushed/popped EBX
and EDX around the C arithmetic. OpenWatcom compiled it without warnings, but
register allocation expanded it to 55 bytes, exceeding the original 36-byte
extent; it was discarded and not spliced. The pure C body remains the only
candidate under test.

## Short absolute-memory leaves

The `store_bytes_46437d`, `advance_counter_15f07d`, and `flag_is_one_15f930`
codegen/boundary blockers remain as recorded in batch 108. No assembly shim or
speculative candidate source was retained.
