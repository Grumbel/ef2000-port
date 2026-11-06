<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 115: verify bounds-record initialization

## `init_bounds_d0b68` (`0xd0b68`, 63 bytes)

The original writes nine dwords to EAX in this exact order: offsets
`0x0c, 0x18, 0x04, 0x1c, 0x08, 0x14` receive zero; offsets `0, 0x10, 0x20`
receive `0x7fff`; then it returns. All three direct callers pass the record
in EAX (`0x1b4a4@0x1b5e3`, `0x26b60@0x26f51`, `0x27348@0x27355`). They
replace EAX before reading it again. The function changes no flags or other
registers and performs no reads.

The OpenWatcom branch uses a volatile dword pointer and retains the original
store order. The host-test branch keeps the existing translated guest-memory
lvalues.

### Evidence

- The verified executable is SHA-256
  `f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd`.
- OpenWatcom 2.0 compiled `decomp/src/engine_helpers3.c` and
  `decomp/tests/ow_init_bounds_d0b68.c` with zero warnings.
- The production symbol body and focused fixture are identical across all 63
  bytes, and both equal the original payload extent. The SHA-bound OMF splice
  writes exactly 63 bytes at `0xd0b68`.
- `func_differential.py` passes **12/12** cases with varied initial bytes,
  including unaligned record addresses. It compares all 36 intended output
  bytes and confirms no extra writes.
- Three call snapshots captured during the 1.5B cockpit path each pass the
  captured-state differential (**3/3**).
- The repeated-baseline 1.5B cockpit replay passes with all five previously
  verified patches plus this one. It reports **155 hits** at `0xd0b68`;
  status, interrupts, ticks, IRQ0 count, framebuffer SHA-256 and the selected
  64 KiB BSS hash match both repeated originals. The BSS SHA-256 is
  `93aec90671d6ae27204af5dbba82a1da835a7a4c20d27aa1fb38b4698df32ab7` and
  the framebuffer SHA-256 is
  `2350746271f7a95ac1ecc1508f4eca506d18e8624585422ae5f24b9d7609ab0b`.

The function is promoted to `verified`.
