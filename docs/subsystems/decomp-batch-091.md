<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Batch 091: twelve READCOLS palette, crater, sky and sun handlers

This continues batch 090's original READCOLS region with **twelve
substantive, previously named game functions** (143–244 original bytes,
plus a 157-byte sun flare and other entries). These are host-side drafts,
not ABI-compatible replacements or guest-differential-verified code.

| Entry | Original function | Observed effect in the host draft |
| --- | --- | --- |
| `da748` | `load_darkside_pal` | Scan palette filename, assemble/check path, warn if missing, load to `421750` |
| `da8c4` | `set_darkside_values` | Same filename path with different suffix and load destination `425190` |
| `dad4c` | `set_nb_craters` | Write one 18-byte crater entry, four 11-bit-shifted coordinates and one word; increment the overlapping high-half counter |
| `dae40` | `load_craters` | Parse three counts; report signed middle count greater than 43; allocate two arrays of 110-byte records |
| `db28c` | `sun_flare` | Parse flare filename/parameters; preserve two words, loaded texture ID and squared 16-bit value |
| `db654` | `sun_texture` | Allocate and head-link 16-byte list node; set two bytes, three words, and bound-warning check |
| `db730` | `cannon_hit` | Write one of two 18-byte hit records (four dwords and two bytes) or report overflow |
| `db808` | `fix_horizon` | Consume a signed number of extra lines; call `2a7ac` per line; warn for count `-1` |
| `db8bc` | `heavenly_body` | Set five-byte sky object record (negated short, mode byte, guest lookup word) when index < 40 |
| `dbff8` | `init3d_file` | Validate filename length 1–8, copy plus suffix, invoke 3D initialization |
| `dc094` | `east_west_palettes` | Validate palette name length 1–12, save name, invoke guest palette loader |
| `dc124` | `setsun` | Two different input scanners, space then LF; two file assembly/copy calls; set sun-present byte |

Important original details:

- Crater records have stride **18**, not 16 or 20 bytes. The next index
  is read from the high half of the 32-bit word at `425bba` and incremented
  through the overlapping 16-bit word at `425bbc`. Original error
  reporting at exactly 20 does *not* prevent an out-of-range store.
- The texture list stores its old head at node offset `+12` before
  updating `425b20`. It emits a warning on signed texture index >= the
  count at `15bb60` but still installs the node.
- The cannon-hit parser preserves the signed `index < 2` test: at index
  2 it reports a warning and leaves the counter unchanged. Negative
  indices can fall through in the original; they are not used in the
  bounded host tests.
- `fix_horizon` reads the configured count again after each callback;
  the special `-1` reports an error then consumes no lines.
- `setsun` is intentionally **not** implemented using the common
  `env_scan_line_090`: it first splits at a *space* without stripping the
  previous byte; the second scan splits at LF/NUL and strips the previous
  byte, following the original inline loops.

## Verification and open boundaries

- `tests/decomp_environment_091_test.c` tests the twelve handlers using a
  synthetic little-endian guest RAM and fake scanf, allocator, and engine
  callbacks. It includes index limits, warning continuation, record
  layout, linked-list ordering, and `setsun`'s unusual two-token scanner.
- `tests/test_decomp_batch_091_payload.py` checks exact entry sizes,
  module/state assignments, referenced global addresses, and the original
  scanner CALL target at `df26f`. The SHA-256-pinned original is supplied
  only through `EF2000_PAYLOAD`; no proprietary bytes enter the repo.
- The scanf adapter `env_scan_fields_090` does **not yet decode Watcom
  stack outputs**. `number[]` and `text` are synthetic test fields. A
  missing scanf conversion, malformed input or unusual signedness can
  diverge from the guest. The file helpers `e7cec`, `132ab4`, `132ab8`,
  `de864`, `9ddfc`, and the allocator `e29e8` remain guest adapters.
  Path assembly in the host therefore does not yet produce real loaded
  pixels, the flare lookup is not native, and `setsun` does not fill the
  two destination images. The copy/append treatment of `9d749`/`9ca57`
  is a call-site inference requiring differential testing.
- This is a host logic and static original-byte audit, not an OpenWatcom
  object comparison, guest splice, or validated gameplay replay. None of
  these twelve functions is marked `compiles`, `spliced`, or `verified`.

Tracker after the batch: **4,287** inventoried entries, **3,257**
`compiles`, **222** `drafted` (+12), and 100 still `named`. The remaining
nearby READCOLS bodies (`load_pal`, `load_cock_pal`, `lightning_point`,
`load_smoke`, `tmsun_flare`, and a still-unclassified entry at `db32c`)
depend on further guest-call recovery and contain more complicated logic.
