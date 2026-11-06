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
- M0 rule 7, inferred register live-ins, 2026-11-06: when an unrecovered
  function reads Ghidra's `in_EAX`/`in_AX`/byte-register pseudo-variables,
  translate them to inferred Watcom register parameters, retaining Ghidra's
  other guessed parameters. Remove only the corresponding pseudo-local
  declaration; functions that write an input register remain `manual`. The
  OpenWatcom rerun produced 138/282 compilable generated functions (48.9%,
  26,100/65,950 bytes = 39.6%). Across all 1,022 unrecovered functions this
  is 13.5% by count and 4.8% by bytes. 144 generated functions remain compile
  errors; first-error groups are parameter-count conflicts 66, pointer type
  required 45, `true`/`false` 8, address-of-rvalue 6, `undefined6` 3,
  duplicate `r_eax` 3, subscripting 3, empty `case` 2, and other individual
  errors. 740 are manual (478,479 bytes); register writes account for the
  increase from the previous report. The increase in successful output
  verifies the translation rule, but inferred signatures remain hypotheses
  and are not yet accepted for hybrid linking.
- M0 rule 8, boolean literals, 2026-11-06: normalize Ghidra's lowercase
  `true`/`false` literals to C integer constants. The OpenWatcom rerun compiled
  144/282 generated functions (51.1%, 27,633/65,950 bytes = 41.9%); six more
  functions compiled, and all undeclared-boolean errors disappeared. Across
  all 1,022 unrecovered functions this is 14.1% by count and 5.1% by bytes.
  The remaining 138 compile errors are led by parameter-count conflicts 68
  and pointer-type errors 45; the rest are smaller independent classes.
- M0 rule 9, unspecified unconfirmed call prototypes, 2026-11-06: Ghidra
  sometimes emits different argument counts for calls to the same function.
  Shared declarations for functions without confirmed signatures now use an
  unspecified C argument list; their generated definitions retain the
  decompiler's concrete parameters, and confirmed declarations remain strict.
  The OpenWatcom report rises to 189/282 ok (67.0%, 37,460/65,950 bytes =
  56.8%). Across all unrecovered functions this is 18.5% by count and 6.9% by
  bytes. The remaining 93 compile errors are led by guest-pointer expressions
  45 and parameter-count conflicts 23. This relaxes compile-time call checking;
  it provides no ABI verification and those calls still require hybrid replay.
- M0 rule 10, byte-pair concatenation lowering, 2026-11-06: translate
  `CONCATxy(a, b)` to width-masked unsigned shifts for values up to 64 bits;
  wider forms stay manual. The OpenWatcom report rises to 207/1,022 ok
  (20.3%, 47,712/544,429 bytes = 8.8%), from 190/1,022 (18.6%,
  37,479/544,429 bytes = 6.9%). The remaining export has 107 compile errors
  (34,149 bytes) and 708 manual functions (462,568 bytes). Compilation used
  the pinned OpenWatcom binary directly because `nix develop` could not write
  the Nix fetcher cache; compile success does not establish ABI correctness.
- M0 rule 11, Ghidra byte-offset fields, 2026-11-06: lower `base._N_M_` reads
  and writes to width-specific accesses at the same address and byte offset;
  three-byte fields stay manual. The OpenWatcom report rises to 271/1,022 ok
  (26.5%, 68,495/544,429 bytes = 12.6%), from 207/1,022 (20.3%,
  47,712/544,429 bytes = 8.8%). The remaining export has 157 compile errors
  (65,523 bytes) and 594 manual functions (410,411 bytes). Mechanical lvalue
  lowering is a draft translation; compiler acceptance is not behavioral
  verification.
- M0 rule 12, unspecified generated-call prototypes, 2026-11-06: the shared
  generated header now leaves argument counts unspecified even for functions
  with confirmed register signatures. Each function body still has its
  signature-derived definition and `#pragma aux`; mismatched call arity is
  deferred to replay. OpenWatcom reports 281/1,022 ok (27.5%,
  77,005/544,429 bytes = 14.1%), up from 271/1,022 (26.5%,
  68,495/544,429 bytes = 12.6%). There are 147 compile errors (57,013 bytes)
  and 594 manual functions (410,411 bytes). This removes only ten rejections;
  compiler acceptance still says nothing about live-register correctness.
- M0 rule 13, address-only global aliases, 2026-11-06: translate Ghidra
  `ram0x<address>` names through the global-width table and byte-offset
  `DAT_<address>_<n>` aliases to guest byte accesses. OpenWatcom reports
  290/1,022 ok (28.4%, 83,196/544,429 bytes = 15.3%), up from 281/1,022
  (27.5%, 77,005/544,429 bytes = 14.1%). The remaining export has 138
  compile errors (50,822 bytes) and 594 manual functions (410,411 bytes).
  These aliases address storage but do not prove the inferred widths or
  behavior.
- M1 structural assessment, 2026-11-06: all 334 `decomp/src/*.c` modules
  compile with OpenWatcom `-wx` and zero warnings into ignored
  `local/hybrid/obj/` (432,877 CODE bytes plus 4,388 DATA bytes). The OMF
  parser accepts all modules; their 9,217 external and 685 segment fixups are
  32-bit offsets framed against FLAT. No hybrid image has been produced and
  no functions/bytes are patched. The current `omf_splice.py` planner only
  accepts one CODE segment with one function public at offset zero;
  `efpatch.py` and its C++ reader reject redirects/arena writes, cap patches
  at 512 in-image writes, and the runner treats the initialized payload as
  the whole patchable image. A whole-set linker therefore needs a distinct
  multi-object symbol/relocation pass and a guest-arena loading path. The
  arena must be above the startup-cleared BSS end `0x4de2f0`, below the
  64-MiB guest limit, and reserved from the simulated heap; placing code in
  BSS would erase it during startup. There are 41 eligible entry slots under
  five bytes, which cannot take a direct `jmp rel32` and need explicit
  exclusion or a proved trampoline. This is a tooling/segment-layout blocker,
  not an OpenWatcom or unsupported-OMF-fixup failure; M1 remains unpassed.
- M1 hybrid-link prototype, 2026-11-06: added a whole-set OMF linker and
  guest-arena patch path. The initial link redirects 1,835 entries (9,175
  original-image bytes) and loads 475,316 bytes of compiled code/data in the
  arena; 411/524 hand/mechanical modules link, with 3,061 function exclusions.
  An isolated OpenWatcom `to_lower_ascii` object reaches the main-menu and
  quick-fight-setup checkpoints with identical framebuffer and selected guest
  RAM hashes. Cockpit framebuffer matches, but the 64-KiB guest-RAM hash does
  not, so this isolated run is not a full replay pass. The complete hybrid
  stops before startup; a redirect-set probe implicated mechanical entry
  `0x0009be38` (the unclassified 427-byte allocator). This is a suspect, not
  a conclusive bisect: the probe removed entry redirects but retained the
  linked arena and its cross-module calls. Demoting that entry advances
  execution but still does not reach the menu (`IP=0x0301afc5`, 7 interrupts
  at the 500M instruction checkpoint), so M1 remains unpassed and the next
  failure is not yet isolated. The runner now
  preserves the BSS initialization pattern around the arena and only blocks
  heap/stack growth if it would overlap the arena; this keeps startup memory
  reporting unchanged. Unit checks pass for 33 Python patch/OMF/replay tests,
  the C++ EFPATCH test, and the OpenWatcom module compilation.
- M0 rule 14, pointer-valued DAT globals, 2026-11-06: detect unknown
  `DAT_<address>` globals used as pointer dereference, indexing, or indirect
  call operands. Reads load the stored 32-bit guest pointer; writes to the
  global itself remain guest-memory lvalues. A second compile pass corrected
  an ambiguity between unary dereference and multiplication. OpenWatcom now
  reports 328/1,022 ok functions (32.1%, 89,263/544,429 bytes = 16.4%), up
  from 290/1,022 (28.4%, 83,196/544,429 bytes = 15.3%). Compile errors fall
  from 138 to 100 (44,755 bytes); 594 functions (410,411 bytes) remain
  manual. This is expression-context inference and does not validate the
  recovered global's real type or runtime behavior.
- M1 replay after M0 rule 14, 2026-11-06: the whole-set linker redirects
  1,934 entries (9,670 original-image bytes) and places 541,108 bytes in the
  guest arena; 512/663 modules link, including 273 mechanical and 1,661
  hand-written entry bodies. The private-Xvfb main-menu replay still exits
  at the original dataset check (`IP=0x222`, 101 interrupts, 296 ticks,
  `AX=0xffff4cff`); only `0xe7835` among the three required startup entries
  executes before exit. M1 remains unpassed. The original baseline reaches
  its 500M-instruction checkpoint and saves a framebuffer.
- M0 rule 15, generic indirect-call declarations, 2026-11-06: generated
  `code *` expressions now use an unspecified argument list and integer
  return type, allowing the varying argument counts and common EAX returns
  present in the export. OpenWatcom reports 367/1,022 ok functions (35.9%,
  101,974/544,429 bytes = 18.7%), up from 328/1,022 (32.1%,
  89,263/544,429 bytes = 16.4%). Compile errors fall from 100 to 61
  (32,044 bytes); 594 functions (410,411 bytes) remain manual. This is a
  loose compile declaration, not a verified callback ABI.
- M0 rule 16, address-of guest globals, 2026-11-06: lower address-taking of
  `EF_U*`/`EF_S*` lvalues to the guest address and `&PTR_*` expressions to
  a pointer to the guest pointer slot. OpenWatcom reports 383/1,022 ok
  functions (37.5%, 111,957/544,429 bytes = 20.6%), up from 367/1,022
  (35.9%, 101,974/544,429 bytes = 18.7%). Compile errors fall from 61 to
  45 (22,061 bytes); 594 functions (410,411 bytes) remain manual. The
  pointer types are inferred from Ghidra expression context and remain
  unverified.
