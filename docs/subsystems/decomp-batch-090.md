<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Batch 090: twenty READCOLS environment and world parser entries

This batch drafts **20 original, previously `named` game functions** in the
German TACTCOM payload. Unlike the 48 dispatch-only aliases in batch 089,
these are real parser and configuration bodies (74–225 bytes). They all
scan the next text line, invoke the original sscanf-like callee at
`0xdf26f`, and mostly write guest state, dispatch another subsystem or
report a diagnostic. The functions belong to the original READCOLS
configuration path, judging by its embedded diagnostics.

The common inline original scanner reads the cursor at `0x425af8`, scans
to LF or NUL, zeroes the terminal byte **and the byte immediately before
it**, and then stores `end+1` as the next cursor. The last-byte write is
retained deliberately, even for NUL-terminated input, since it can remove
real data rather than only CR. We observed two code orders for the write,
one before and one after incrementing the terminal pointer; both touch
the same address. This behavior also appears in batches 087–088.

## Entry points and recovered effects

| Entry | Original name | Host model |
| --- | --- | --- |
| `da1b4` | `darkside_fade` | Save fade ID, forward signed depth to `c486c` |
| `da224` | `coltab` | Save color table ID, invoke `e7ea4`, report invalid selection |
| `da2b8` | `altitude` | Record negative altitude and copied string in 20-byte indexed slot, advance count |
| `da360` | `altitude2` | Negate altitude and optional second parameter; set presence word and call texture helper |
| `da408` | `ground` | Corresponding ground fields and copy to different texture target |
| `da4b0` | `ceiling` | Negative 16-bit value and presence flag |
| `da518` | `planet_number` | Negative 16-bit value and presence flag |
| `da580` | `game_type` | Store one `%hd` value at `425be4` (despite its name, **not** a string) |
| `da7fc` | `load_missile_pal` | Five-way destination/slot switch, missing-palette warning, selector increment |
| `da964` | `set_world_name` | Four numeric parsed outputs, including two 16-bit name fields |
| `da9d4` | `set_xmap_name` | Validate nonempty name, copy and append map-image extension |
| `daa5c` | `set_mmap_name` | Indexed 25-byte name slots, report bad index, copy and append extension |
| `dab40` | `ref_point` | Validate reference-point name, copy and append extension |
| `dabc8` | `set_map_name` | Invoke unresolved string transformation, save name |
| `db0b4` | `load_player_virtual` | Invoke `80d8c`, store returned DH byte |
| `db114` | `load_player_sort` | Decode line then call `392fc` |
| `db16c` | `load_player` | Decode line then call `39300` |
| `db1c4` | `set_nb_flak` | Decode line then call `3930c` |
| `db21c` | `load_flak` | Store flak label, signed bound warning and invoke `2a794` |
| `db420` | `cloud_fade` | Write five 32-bit fields in 20-byte indexed record, decrement final field, increment count |

## Evidence and boundaries

- The pinned original 1,662,896-byte payload has SHA-256
  `3fd26b189efe289feb9e6ac701d592a5d48d7c1e152527aa00c01611e87e0fdb`.
  `tests/test_decomp_batch_090_payload.py`, with `EF2000_PAYLOAD`, verifies
  each original entry pattern, original scanner call destination, format
  string address and a known state/global reference. No original bytes are
  stored in git.
- `decomp/src/environment_scanner_090.c` reconstructs the destructive line
  cursor behavior. `environment_values_090.c` and `environment_world_090.c`
  reconstruct distinct state writes, guards, selectors and guest call
  ordering. `tests/decomp_environment_090_test.c` exercises all twenty.
- `env_scan_fields_090` is **a host test adapter for the unresolved Watcom
  sscanf stack/output layout**, not a proven drop-in signature. Numeric
  `number[]` fields and `text` stand for parsed output positions. Missing
  scanf conversions, integer overflow, and malformed text require native
  guest differential comparison before those conversions can be trusted.
- Calls to e.g. `0xe7cec`, `0x80d8c`, `0xa4421`, `0x9ca57`, `0x9d749` and
  the error sink remain opaque adapters. The host name-copy/append helpers
  are a **hypothesis from call-site behavior**, not a verified replacement
  for the original string helpers. `set_map_name` does not yet reproduce the
  unresolved in-place text transformation; `load_player` and its siblings
  do not yet reproduce their downstream side effects.
- This is an incremental **host draft batch**. No new function is marked
  `compiles`, `spliced` or `verified`. No OpenWatcom/x87/guest ABI evidence
  is asserted. Differential function fixtures and golden replay remain
  necessary before any original entry can safely be replaced.

The tracker increases from 190 to **210 drafted entries**, retains 3,257
`compiles` and 4,287 original inventoried entries. A future batch can
continue this contiguous READCOLS cluster with craters, flak and sun/sky
data, but several remaining functions have complicated indirect calls and
should not be counted as fully recovered on static evidence alone.
