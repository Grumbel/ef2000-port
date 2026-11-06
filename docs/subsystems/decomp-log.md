<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation log

Append M0 mechanical translation, hybrid milestone, and bulk recovery progress
here. Ghidra-derived function bodies remain only under ignored `local/`.

- M0 baseline, 2026-11-06: `ghidra_to_c.py --skip-recovered --compile` examined
  1,022 unrecovered functions. 287 generated (66,726 bytes), 735 manual
  (477,703 bytes), 0/287 compiled (0 bytes). All first errors came from five
  stack inputs such as `[ESP+4]` being emitted as invalid C parameter names in
  the shared ABI header. The Nix dev shell could not start because access to
  `/nix/var/nix/daemon-socket/socket` was denied; the pinned OpenWatcom
  compiler was run directly.
- M0 rule 1, stack arguments, 2026-11-06: stack inputs now use valid local
  parameter names and are omitted from the register `#pragma aux` list. The
  rerun advanced past the shared-header stack-name errors, but remains 0/287
  (0/66,726 bytes): the next shared-header error is unsupported `float10` at
  generated ABI declaration `0x00028b80`.
- M0 rule 2, `float10`, 2026-11-06: Ghidra `float10` now maps to OpenWatcom
  `long double`, preserving the x87 extended type. The rerun advanced past
  that declaration; the report remains 0/287 (0/66,726 bytes), now blocked by
  the unconverted Ghidra `dword` type at `0x000b3084`.
- M0 rule 3, `dword`, 2026-11-06: Ghidra `dword` now maps to `uint32_t`. The
  rerun advanced past the dword declarations; the report remains 0/287
  (0/66,726 bytes), now blocked by Ghidra's generic function type `code` at
  `0x000ed133`.
- M0 rule 4, generic `code`, 2026-11-06: generated prototypes now use a
  generic no-argument function type for Ghidra code pointers. The rerun
  advanced past `code *` declarations; it remains 0/287 (0/66,726 bytes),
  now blocked by `float10` in a signature-derived prototype at `0x00120a52`.
  That exposed that the type rules currently normalize bodies but not the
  signature table.
- M0 rule 5, signature type normalization, 2026-11-06: apply the same Ghidra
  type rules to the confirmed signature table as to function bodies. The
  shared header now compiles and 104/287 generated functions compile (36.2%,
  16,484/66,726 bytes = 24.7%). Across all 1,022 unrecovered functions this
  is 10.2% by count and 3.0% by bytes (16,484/544,429). There are 183
  compile errors (50,242 bytes) and 735 manual functions (477,703 bytes).
  First-error groups: E1011 undeclared symbols 49, E1151 parameter-count
  conflicts 43, E1029 pointer type required 41, E1009 malformed `r_eax`
  parameters 38, E1030 address-of-rvalue 5, E1034 duplicate `r_eax` 3,
  E1045 subscript errors 2, E1052 void expression 1, E1084 empty case 1.
  Manual causes overlap: `extraout_*` 467, `CONCATn` 322, `_N_M_` fields 334,
  `in_*` registers 310, `unaff_*` 173, warnings/halt markers 380, software
  interrupts 22, and SUB/ZEXT/SEXT 21.
- M0 rule 6, DAT widths, 2026-11-06: translate both `DAT_x` and leading-
  underscore `_DAT_x`, using exact scalar widths/types from `globals.tsv`
  where known and the planned 32-bit fallback otherwise. 126/287 generated
  functions compile (43.9%, 22,659/66,726 bytes = 34.0%). Across all 1,022
  unrecovered functions this is 12.3% by count and 4.2% by bytes. Remaining
  161 compile errors: E1151 parameter-count conflicts 51, E1029 pointer type
  required 41, E1009 `r_eax` syntax 38, E1011 undeclared symbols 16, E1030
  address-of-rvalue 6, E1034 duplicate `r_eax` 3, E1045 subscript 3, E1084
  empty case 2, E1052 void expression 1.
