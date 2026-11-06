<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 111: verify runtime slot flag lookup

## `read_runtime_slot_flag` (`0x121cc`, 33 bytes)

The original body sign-extends DX, uses it to select an overlapping unaligned
dword at `0x121ec6 + 2*DX`, sign-extends that dword's high word, adds the
sign-extended AX subindex, and reads the byte at `0x121fb4 + 29*index`. Its
original instruction sequence is:

```text
movsx edx,dx
mov edx,[edx*2+0x121ec6]
cwde
sar edx,0x10
add edx,eax
lea eax,[edx*8]
sub eax,edx
mov al,[edx+eax*4+0x121fb4]
ret
```

The last two address operations implement a 29-byte stride. The confirmed
interface is `AX:int16_t, DX:int16_t -> AL:uint8_t`; EBX, ECX, ESI, EDI and EBP
are preserved, with no stack arguments or cleanup. The calls at `0x65a6` and
`0x1f8a8` test AL immediately. Nearby calls at `0x6565` and `0x6584` also
test AL immediately, so their flags are not live after return. The function
makes no memory writes; it reads one unaligned dword and one selected byte.

The focused OpenWatcom C body uses flat guest pointers and emits 26 bytes of
instructions. The reviewed in-place patch occupies the full 33-byte extent,
ends in `RET`, and has no external call or relocation. The production
module's OpenWatcom disassembly matches the focused fixture at the symbol.

### Evidence

- OpenWatcom 2.0 compiled `decomp/src/runtime_indexed.c` and
  `decomp/tests/ow_read_runtime_slot_flag.c` with zero warnings.
- The executable SHA-256 matches `docs/executable.md`:
  `f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd`.
- The SHA-bound 33-byte OMF patch passes **380/380** isolated cases. Cases
  cover signed AX/DX boundaries, large signed DX displacements, slot indices
  across 0..53 and 128 seeded variations. The comparison checks AL, preserved
  registers and all observed RAM writes.
- Three captured calls from the actual cockpit run pass the captured-state
  differential. They are calls through `0x6460`; caller `0x1f878` and its
  AL-test sequence were separately checked in the original disassembly.
- With the three earlier verified patches plus this patch, the deterministic
  500M main-menu and 700M quick-fight setup runs each match two repeated
  unpatched baselines for status, interrupts, timer ticks, selected 64 KiB
  BSS hash and framebuffer hash. A 1.5B cockpit run passes the same replay
  gate with **three hits at `0x121cc`** and matching BSS/framebuffer hashes.

These gameplay replays hash selected BSS, not all guest RAM. The isolated
tests compare the function's return and preserved-register contract and its
complete observed memory writes. The tracker is promoted to `verified`.
