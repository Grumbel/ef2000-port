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
- M0 rule 17, pointer-slot globals, 2026-11-06: lower Ghidra `PTR_*` names
  as reads and writes of the 32-bit pointer stored at the guest address;
  byte-offset subfields continue to address the slot's bytes. OpenWatcom
  reports 386/1,022 ok functions (37.8%, 113,189/544,429 bytes = 20.8%), up
  from 383/1,022 (37.5%, 111,957/544,429 bytes = 20.6%). Compile errors
  fall from 45 to 42 (20,829 bytes); 594 functions (410,411 bytes) remain
  manual. The pointer-slot interpretation comes from Ghidra naming and use,
  and is not runtime verified.
- M0 rule 18, inferred register pointer types, 2026-11-06: retain Ghidra's
  declared pointee type for inferred `in_EAX`/other live-in registers, while
  keeping the same 32-bit register parameter ABI. This fixes dereferences
  emitted as `*in_EAX` without a cast. OpenWatcom reports 389/1,022 ok
  functions (38.1%, 114,296/544,429 bytes = 21.0%), up from 386/1,022
  (37.8%, 113,189/544,429 bytes = 20.8%). Compile errors fall from 42 to
  39 (19,722 bytes); 594 functions (410,411 bytes) remain manual. These
  register parameter types are Ghidra-derived, not verified against callers.
- M1 replay after M0 rule 17, 2026-11-06: the whole-set linker redirects
  1,978 entries (9,890 original-image bytes) and places 569,028 bytes in
  the guest arena; 556/721 modules link (316 mechanical and 1,661 hand-written
  bodies patched). The current hand-written C modules were rebuilt with
  OpenWatcom `-wx` first; all 335 compile without warnings. Private-Xvfb
  replay still exits at the dataset check (`IP=0x222`, 101 interrupts,
  296 ticks, `AX=0xffff4cff`); only `0xe7835` among the three required startup
  entries executes before exit. The original baseline reaches the 500M
  instruction checkpoint and saves a framebuffer. M1 remains unpassed.
- ABI correction for `0xa7990`, 2026-11-06: disassembly at `0xebcf3`
  shows `push eax; call 0xa7990`, and the callee reads `[ESP+4]` and removes
  that stack argument. Added `#pragma aux guest_fn_a7990 parm []`; an
  OpenWatcom probe emits the required push and assumes callee cleanup. All
  335 hand-written modules compile with `-wx`. The full hybrid still fails
  at the dataset check (`IP=0x222`, 101 interrupts, 296 ticks). A new
  bisector run narrowed no unique culprit (candidate set remains broad in
  the `0xc1bd0`–`0xd13dc` region), so M1 remains unpassed.
- M3 startup bisection, 2026-11-06: delta debugging isolated mechanical
  entry `0xcd4d4` as a minimal cause in one reduced patch set. Its Ghidra
  draft inferred two fastcall parameters and used dword accesses for the
  byte state at `0x15b72e`; original disassembly shows a no-argument entry,
  byte accesses, preserved EBX/ECX/EDX and explicit register setup at calls.
  Replaced it with a C body using the original selector registers and call
  values; OpenWatcom compiles the module with `-wx`. With only this entry and
  the lowercase canary patched, the focused Unicorn replay stops at
  `unsupported_interface` during keyboard I/O, so it cannot establish replay
  equivalence. The full patch still fails earlier at `IP=0x222` (101
  interrupts, 296 ticks); M1 remains unpassed.
- M3 bisector correction, 2026-11-06: the preceding `0xcd4d4` minimal-set
  result was a false attribution. The bisector compared only stop status and
  IP, conflating different progress points (101 and 198 interrupts). Its
  failure signature now includes interrupt, timer-tick and IRQ0 counts.
  Re-running the full set for the original target (`guest_exit`, IP `0x222`,
  101 interrupts, 296 ticks, IRQ0 0) leaves 62 candidates; focused reduction
  leaves 10 (`0xbc36a`–`0xbc471` and `0xde6c4`–`0xdf21b`) and remains
  unresolved. The focused two-entry trial for `0xcd4d4` instead stopped at
  `unsupported_interface`. No culprit is confirmed; the full hybrid still
  fails at the dataset check.
- M3 correction at `0xbc36a`, 2026-11-06: original disassembly pushes EBX,
  zeroes EBX for the `0xbc34f` call and restores EBX. The recovered C omitted
  that temporary value. Added the register sequence around the C call; the
  OpenWatcom output preserves the surrounding ABI. With only `0xbc36a` and
  `0xe7835` patched, replay reaches the 500M-instruction checkpoint and
  matches baseline interrupt/tick/IRQ0 counts, framebuffer hash and selected
  guest-RAM hash. It records 98 hits to `0xbc36a`. The full hybrid still
  exits at `IP=0x222` after 101 interrupts and 296 ticks, so this local fix
  does not resolve the first full-set failure.
- M3 replay horizon/signature update, 2026-11-06: a 100M-instruction
  manifest still observes the full-hybrid dataset exit (`IP=0x222`, 101
  interrupts, 296 ticks) while the original reaches its instruction limit.
  The bisector now buckets progress by 8 interrupts, 16 ticks and 8 IRQ0
  events, retaining stop status/IP while tolerating small timing shifts. The
  full run reduces the 62-entry exact-counter set to 31 candidates; it is
  still unresolved.
- M0 rule 19, terminal empty switch cases, 2026-11-06: when Ghidra emits a
  final `case`/`default` label immediately before a switch close, the
  translator now inserts an empty statement. This keeps terminal cases from
  falling through while making the draft valid C. The pinned OpenWatcom run
  rises from 388/1,021 to 391/1,021 generated exports (38.3%, 114,873/544,304
  bytes = 21.1%) and removes three `E1084: Statement required after 'case'`
  failures. There are now 36 compile errors (19,020 bytes) and 594 manual
  functions (410,411 bytes); `CONCATn`/register-related manual causes still
  overlap. One manifest entry is marked `reused` and has no export file, so
  it is outside this report. `nix develop` could not write its fetcher-cache
  database in this session; the pinned OpenWatcom executable was invoked
  directly. M1 remains unpassed: the latest linked hybrid has 1,979 entry
  redirects (9,895 original bytes), 570,772 arena bytes, and 557/724 modules
  linked; replay exits at `IP=0x222` (`AX=0xffff4cff`, 101 interrupts, 296
  ticks).
- M3 ABI repair at `0x1114c7`, 2026-11-06: trace-enabled reduction isolated
  the replacement as the decisive entry in a five-entry reproducer. Original
  disassembly shows the caller pushes two arguments and cleans them with
  `add esp,8`, while the callee returns with plain `ret`. OpenWatcom had emitted
  `ret 8` for the recovered C function, causing double stack cleanup. Declaring
  `parm caller [eax edx ebx ecx]` keeps the four register inputs and makes the
  generated body leave caller-owned stack arguments untouched. WDIS now shows
  the original `[ESP+4]`/`[ESP+8]` argument accesses and a tail jump to the
  release helper, which returns to the caller without popping those arguments.
  In the five-entry pre-fix reproducer, demoting `0x1114c7` reaches the 100M
  instruction checkpoint; the fixed object also compiles with the corrected
  stack accesses and cleanup. The complete hybrid now patches 1,982 entries
  (9,910 original bytes) and links 559/726
  modules with a 571,524-byte arena. It no longer faults at the startup stack
  write, but M1 still fails at 100M: `IP=0x0` vs original `0x110de6`, 202 vs
  13,766 interrupts, 1 vs 356 IRQ0 events, and a different selected RAM hash.
  A broad trace over all 1,982 entries was manually stopped because of tracing
  overhead; the next culprit remains unresolved. M0 remains 391/1,021 exports
  (38.3%, 114,873/544,304 bytes); M1 is unpassed.
- M3 cdecl table-wrapper and fill-pattern ABI corrections, 2026-11-06:
  original disassembly shows the `0x102144`–`0x102384` trace wrappers and
  callees `0x111365`, `0x1113ac`, `0x1113d4`, `0x11141f`, `0x111473`,
  `0x1114c7`, `0x1114f7`, and `0x111527` use stack arguments with caller
  cleanup. Their recovered prototypes described register arguments instead.
  Changed the C declarations and definitions to `EF_CDECL`; OpenWatcom builds
  all 335 hand-written modules with `-wx`. Separately, caller disassembly at
  `0x2af20` shows `fill_pattern_deb50` receives destination in EAX, pattern in
  EDX, and byte count in ECX (not EBX); its header pragma and ABI row now match.
  With the cdecl corrections but before the ECX correction, the full hybrid
  reached `IP=0x110de6` at 100M but differed in interrupt/IRQ0 counts and RAM.
  With both corrections, replay instead faults in original heap code at
  `0x110487`, trying to write `0x90909008` at tick 228, before canary
  `0xe7835` is hit. `misc_drafts1.obj` is excluded from the hybrid because it
  has an unresolved `guest_callptr1_x_` external, so `heap_alloc_110410` is
  still original. The new bisector reduction retains 371 candidates after two
  resumes and is unresolved. M0 remains 391/1,021 exports (38.3%,
  114,873/544,304 bytes); M1 remains unpassed.
- M3 fill-wrapper call ABI correction, 2026-11-06: original disassembly at
  `0xb3fb0` loads the fill count from EBX into ECX before calling `0xdeb50`,
  whose callers pass the count in ECX. The recovered wrapper called the
  `guest_fn_deb50_f` alias without an OpenWatcom pragma, so WDIS showed the
  count left in EBX. Added the alias pragma for EAX/EDX/ECX; a fresh WCC build
  now emits `mov ecx,ebx` before the call. The full hybrid still exits at
  `0x222` after three ticks with `datfile: not enuff memory` (51 interrupts,
  no IRQ0), so this fixes a verified call ABI but does not resolve M1. The
  latest full link patches 1,984 entries (9,920 bytes), links 559/726 modules,
  and uses a 571,620-byte arena. M0 remains 391/1,021 exports (38.3%,
  114,873/544,304 bytes); M1 is unpassed.
- M3 list-stream wrapper ABI correction, 2026-11-06: rebuilt
  `list_lookup.c` with the pinned OpenWatcom 2.0 compiler (zero warnings or
  errors) and confirmed in WDIS that `0xbc34f` allocates the stream in EAX,
  passes NAME/MODE/FLAGS/STREAM in EAX/EDX/EBX/ECX to `0xbc287`, and returns
  its EAX result. `0xbc36a` saves EBX, passes zero for FLAGS, and restores EBX,
  matching the original disassembly. The fresh whole-image link contains
  1,984 redirects (9,920 original-entry bytes; 571,540 arena bytes; 559/726
  modules). Its replay no longer exits at `0x222` with `datfile: not enuff
  memory`; it now reaches a later invalid stack write at `IP=0x22`,
  `ESP=0x401c791`, tick 172 (zero IRQs), versus the original 100M checkpoint.
  M1 remains unpassed; the next failure is stack/control-flow corruption. M0
  remains 391/1,021 exports (38.3%, 114,873/544,304 bytes).
- Menu-path validator ABI recovery, 2026-11-06: disassembly at `0xf8b14`
  confirms inputs in EAX/EDX/EBX/ECX plus three stack arguments and callee
  cleanup of 12 bytes. Replaced its mechanical draft with explicit C helper
  calls and the matching OpenWatcom `routine` ABI; WCC 2.0 reports zero
  warnings/errors and WDIS confirms the generated `ret 0xc`, register setup,
  and stack argument offsets. The hybrid linker selects this hand-written
  body and resolves the call to original helper `0xf8624`: 1,984 redirects,
  9,920 original-entry bytes, 572,164 arena bytes, 560/727 modules, and
  3,169 exclusions. This variant has not yet been replayed. The previous
  menu replay first faulted at `0xf8c25`; no M1/M2 milestone is passed. A
  private Xvfb replay could not start because this environment's
  `/tmp/.X11-unix` is owned by `nobody`, so Xvfb refuses the socket directory.
  M0 remains 391/1,021 exports (38.3%, 114,873/544,304 bytes).
- Follow-up menu-path evidence, 2026-11-06: the earlier bisector's trial 12
  still redirected `0xf8b14` to its mechanical body (the trial exclusion
  file does not demote it) and failed with an unmapped fetch at
  `0x4e0f8624` while that path calls original helper `0xf8624`. The
  mechanical draft calls this helper without arguments. Hand C supplies
  EAX/EDX/EBX/ECX from the original call setup and keeps `0xf8b14`'s 12-byte
  callee cleanup. This is
  a concrete candidate explanation, not a replay confirmation. Prepared an
  ignored trace manifest for `0xf8600`, `0xf9448`, `0xf8b14`, `0xf8624` and
  the relevant slot helpers; rerun it when private Xvfb is available.
- M0 `undefined6` carrier rule, 2026-11-06: mapped Ghidra's `undefined6`
  and `uint6` value forms to a 64-bit C carrier. The four affected generated
  bodies consume the returned value as EDX:EAX, explicitly selecting the low
  16 bits of EDX where needed. The pinned OpenWatcom compile pass increased
  from 390 to 393 `ok` functions (115,359/543,917 bytes, 21.2% of exported
  body bytes); 594 remain `manual` and 33 remain `compile-error`. This is
  compile acceptance only, not behavioral verification. The menu validator
  replacement still awaits private-Xvfb replay; M1/M2 remain unpassed.
- Hybrid link callback bridge, 2026-11-06: original disassembly at
  `0x104471` pushes the callback argument, calls the pointer at `[ESI+0x24]`,
  then removes four bytes, confirming the cdecl one-stack-argument contract
  declared for `guest_callptr1_x`. Added OpenWatcom C bridges for that call
  and `ui_report_missing_record` (through the guest callback at `0x132a74`).
  Both compile with zero warnings; WDIS shows `push EDX; call EAX; add ESP,4`
  and the UI wrapper's pointer load and tail call. The fresh hybrid now
  redirects 2,160 functions (10,800 original-entry bytes; 572,932 arena
  bytes), linking 581/731 modules with 2,823 exclusions. This resolves the
  `guest_callptr1_x_` and `ui_report_missing_record_` module failures and
  adds 176 patched functions over the previous link. No compliant replay has
  run on this artifact: Xvfb's private Unix-socket bind is denied with
  `EPERM`, so M1/M2 remain unpassed and the current artifact's first guest
  failure is unknown. The earlier `0xf8c25` fault came from a different
  hybrid with `0xf8b14` demoted.
- Two-argument callback bridge, 2026-11-06: original disassembly at
  `0x956cc` pushes the format and name arguments, calls the callback at
  `0x132a74`, and removes eight bytes. Added `guest_callptr2_x` with the
  cdecl right-to-left stack layout and caller cleanup; WCC reports zero
  warnings and WDIS confirms `push EBX; push EDX; call EAX; add ESP,8`.
  Relinking resolves three more modules and raises coverage to 2,174
  redirected functions (10,870 original-entry bytes; 572,932 arena bytes),
  584/731 modules linked and 2,795 exclusions. This hybrid has not been
  replayed; M1/M2 remain unpassed, with no current-artifact guest failure
  yet available because the private Xvfb socket bind is denied.
- Menu-path compile/link audit, 2026-11-06: rebuilt `menu_runtime.c` with
  the pinned OpenWatcom v2 compiler (zero warnings/errors); its object is
  byte-identical to the object used by `menu-path-callbridge2`. WDIS confirms
  the validator's callee-clean `ret 0xc` and the EAX/EDX/EBX/ECX setup before
  its call to original helper `0xf8624`. The hybrid redirects `0xf8b14`,
  `0xf8600` and the `0xe7835` replay canary; `0xf9448` and `0xf8624` remain
  original. A fresh private-Xvfb startup check fails because this environment
  has `/tmp/.X11-unix` owned by `nobody` while Xvfb requires it root-owned;
  user-scope `PrivateTmp` is unavailable, so the current hybrid has no new
  replay result. M0 remains 393/1,020 functions and 115,359/543,917 bytes
  (21.2%); M1/M2 remain unpassed.
- Menu dispatch ABI inspection, 2026-11-06: original disassembly confirms
  `0xf8600` pushes four copies of its stack inputs before calling `0xf9448`;
  `0xf9448` reads three more words from the inherited outer stack, calls
  `0xf8b14` with four registers and three stack arguments, then calls
  `0xf8e44` with four registers and ten stack arguments. The epilogues are
  `ret 0x10` at `0xf9448` and `ret 0x28` at `0xf8e44`. This inherited-stack
  behavior is not represented by Ghidra's six-parameter pseudo-C prototype;
  leave `0xf9448` and `0xf8e44` original until their stack layout is modeled.
- M0 recovered-alias rule, 2026-11-06: mechanically declare recovered
  callees with no register inputs (including no-argument and stack-only
  signatures) so generated callers and function-table initializers resolve.
  Where no return is confirmed but Ghidra consumes EAX, the generated alias
  uses the generic `uint32_t` carrier; this is compile-enabling evidence, not
  a behavioral ABI claim. The OpenWatcom pass increased from 393 to 394 `ok`
  functions and from 115,359 to 116,954 ok body bytes (21.5% of exported
  bytes). Relinking added `0x1a580`, bringing the hybrid to 2,175 redirects
  (10,875 entry bytes; 574,596 arena bytes), 585/732 modules, with 321
  mechanical functions and 1,854 hand-written functions patched. No replay
  ran because private Xvfb cannot bind in this environment; M1/M2 remain
  unpassed and the current hybrid's first guest failure is unknown.
- M0 three-byte/subfield rule, 2026-11-06: translate Ghidra `uint3` values
  through a 32-bit carrier and recognize `_N_M_` subfields on array
  elements, preserving the byte offset in the containing local. The full
  OpenWatcom pass now reports 395/1,020 `ok` functions and 117,666/543,917
  ok body bytes (21.6%); three asset-free translator tests pass. Relinking
  keeps 2,175 redirects (10,875 entry bytes; 575,284 arena bytes), 585/733
  modules, 321 mechanical functions and 1,854 hand-written functions
  patched. The new `0x5d1c4` body compiles but its module is excluded for
  unresolved OpenWatcom helper `__U8LS`; total exclusions rise to 2,797.
  No replay ran because Xvfb cannot bind; M1/M2 remain unpassed.
- Hybrid OpenWatcom shift bridge, 2026-11-06: added a small 32-bit OMF
  runtime helper for `__U8LS` (64-bit EDX:EAX value, EBX count modulo 64),
  assembled with the pinned OpenWatcom `wasm` and confirmed in WDIS. The
  fresh link resolves that helper class and grows from 2,175 to 2,202
  redirected functions (11,010 entry bytes; 575,300 arena bytes), with
  348 mechanical and 1,854 hand-written functions patched across 613/734
  modules; exclusions fall from 2,797 to 2,743. The startup/menu redirects
  at `0xf8600`, `0xf8b14` and `0xe7835` remain present. No replay ran because
  private Xvfb cannot bind; M1/M2 remain unpassed. The first remaining link
  failure class is `__CHP` (19 modules); first guest failure is unknown.
- Hybrid OpenWatcom right-shift bridge, 2026-11-06: added the matching
  `__U8RS` OMF helper with logical EDX:EAX shift semantics and EBX count
  modulo 64; WDIS confirms the emitted register sequence. Relinking raises
  coverage to 2,228 redirects (11,140 entry bytes; 575,316 arena bytes),
  with 358 mechanical and 1,870 hand-written functions patched across
  626/735 modules; exclusions fall to 2,691. The current leading unresolved
  class is `__CHP` (19 modules). No replay ran because Xvfb cannot bind, so
  M1/M2 remain unpassed and the first guest failure is unknown.
- Hybrid OpenWatcom signed multiply bridge, 2026-11-06: added `__I8M` for
  64-bit operands EDX:EAX and ECX:EBX, returning EDX:EAX. Its OMF body uses
  32-bit partial products; WDIS confirms the helper and the generated caller
  register setup. Relinking reaches 2,235 redirects (11,175 entry bytes;
  575,364 arena bytes), 630/736 modules, with 360 mechanical and 1,875
  hand-written functions patched; exclusions fall to 2,677. The first
  remaining link failure class is `__CHP` (19 modules). No replay ran because
  private Xvfb cannot bind; M1/M2 remain unpassed and the first guest failure
  is unknown.
- Hybrid OpenWatcom x87 chop bridge, 2026-11-06: added `__CHP`, which rounds
  ST(0) toward zero with FRNDINT while restoring the original x87 control
  word, EAX, EFLAGS, and stack. WDIS confirms the 32-bit OMF body; relinking
  resolves the `__CHP` failures and grows to 2,315 redirects (11,575 entry
  bytes; 575,396 arena bytes), 644/737 modules, with 367 mechanical and 1,948
  hand-written functions patched; exclusions fall to 2,517. This is link
  evidence, not a successful menu replay. Both local Unix and TCP Xvfb
  listeners fail in the sandbox, so M1/M2 remain unpassed and the first guest
  failure is unknown.
- M0 / hybrid `ROUND`, 2026-11-06: declared Ghidra's `ROUND` as a `double`
  input/output operation in the generated ABI header, avoiding Watcom's
  implicit int-returning call. The regenerated `0x1cd8` object now calls
  `ROUND_`, then `__CHP` and `fistp`, matching the separate floating-point
  rounding and integer-truncation operations. Added an OMF `ROUND_` bridge
  implementing nearest-integral rounding with halfway values away from zero
  while preserving the x87 control word. Existing translator tests pass; the
  full compiler pass remains 395/1,020 ok (117,666/543,917 bytes, 21.6%). The
  hybrid grows to 2,325 redirects (11,625 entry bytes; 575,860 arena bytes),
  655/738 modules, with 377 mechanical and 1,948 hand-written functions
  patched; exclusions fall to 2,497. This is compile/link evidence only; no
  private Xvfb listener can start in the sandbox, so M1/M2 remain unpassed
  and the first guest failure is unknown.
- Hybrid OpenWatcom unsigned-64-to-double bridge, 2026-11-06: added
  `__U8FD7` for input EDX:EAX and double result EDX:EAX. The implementation
  forms both unsigned 32-bit halves in x87 extended precision, restores the
  caller's x87 control word, and stores the final double once. WDIS confirms
  the OMF body and data relocations; relinking resolves ten dependent modules
  and grows to 2,334 redirects (11,670 entry bytes; 575,972 arena bytes),
  665/739 modules, 386 mechanical and 1,948 hand-written functions patched;
  exclusions fall to 2,479. Runtime replay remains unavailable because Xvfb
  cannot bind, so M1/M2 remain unpassed and the first guest failure is
  unknown.
- Hybrid OpenWatcom signed-64 divide/modulo bridge, 2026-11-06: added
  `__I8D` with dividend EDX:EAX, divisor ECX:EBX, quotient EDX:EAX and
  remainder ECX:EBX. The restoring unsigned divide core and sign handling
  passed a helper-level Unicorn differential over 1,013 deterministic edge
  and random vectors, including ESI/EDI/EBP and stack preservation; WDIS
  confirms a compiled caller's register setup. The same harness passed 1,007
  `__U8FD7` unsigned-to-double vectors and confirmed x87 control-word
  restoration. Relinking resolves the `__I8D` class and grows to 2,417
  redirects (12,085 entry bytes; 576,132 arena bytes), 673/740 modules, with
  388 mechanical and 2,029 hand-written functions patched; exclusions fall
  to 2,313. These helper checks do not substitute for game replay: Xvfb still
  cannot bind, M1/M2 remain unpassed, and the first guest failure is unknown.
- Hybrid OpenWatcom signed-64 arithmetic shift bridge, 2026-11-06: added
  `__I8RS` for EDX:EAX shifted by EBX modulo 64. Its arithmetic SAR/RCR loop
  assembles cleanly and passed 128,896 helper-level Unicorn vectors across
  signed edge/random values and counts 0 through 127, including callee
  register and stack preservation. Relinking resolves both `__I8RS` modules
  and reaches 2,419 redirects (12,095 entry bytes; 576,148 arena bytes),
  675/741 modules, 388 mechanical and 2,031 hand-written functions patched;
  exclusions fall to 2,309. M1/M2 remain unpassed because private Xvfb cannot
  bind, and the first guest failure is unknown.
- Hybrid OpenWatcom double-square-root bridge, 2026-11-06: added
  `IF@DSQRT` as an x87 ST(0)-in/ST(0)-out `FSQRT` helper. Original callsite
  disassembly shows ST(0) is a sum of squared coordinate differences in all
  three dependent geometry modules; one caller also guards nonpositive
  values. A helper-level Unicorn test passed 1,013 nonnegative 80-bit ST(0)
  inputs, preserving the x87 control word and stack. The bridge does not
  reproduce OpenWatcom's math-error reporting for negative inputs, which are
  outside these observed caller paths. Relinking adds 12 redirects and reaches
  2,431 functions (12,155 entry bytes; 576,148 arena bytes), 679/742 modules,
  388 mechanical and 2,043 hand-written functions patched; exclusions fall
  to 2,285. M1/M2 remain unpassed because private Xvfb cannot bind, and the
  first guest failure is unknown.
- Hybrid OpenWatcom double-to-unsigned-64 bridge, 2026-11-06: added
  `__FDU87` for a binary64 input in EDX:EAX and uint64 output in EDX:EAX.
  The conversion truncates the magnitude in range, returns zero below 1,
  saturates non-finite values and magnitudes at least 2^64 to UINT64_MAX, and
  negates in-range negative magnitudes as the OpenWatcom helper does. WDIS
  confirms the caller setup; a helper-level Unicorn check passed 5,014 input
  bit patterns and verified preserved non-result registers, control word, and
  stack. Both dependent guest modules now link. The hybrid reaches 2,433
  redirects (12,165 entry bytes; 576,292 arena bytes), 682/743 modules, with
  390 mechanical and 2,043 hand-written functions patched; exclusions fall
  to 2,281. This is runtime-helper ABI/model evidence, not differential proof
  against the original guest. M1/M2 remain unpassed because private Xvfb
  cannot bind; the first guest failure is unknown.
- Menu-path ABI corrections, 2026-11-07: original callers and disassembly
  show `intdos_2b0` receives its register block in both EAX (input) and EDX
  (output); the recovered wrappers had supplied only EAX. The C ABI and all
  recovered wrapper calls now pass both pointers. The original `0x9c220` and
  `0x9c23d` helpers read three caller-stack arguments, so their declarations
  now explicitly use `EF_CDECL`; WDIS confirms the rebuilt `0x628` caller
  pushes those arguments. Four affected OpenWatcom modules compile with zero
  warnings. A focused hybrid with only `init_far_stub_b04` and
  `to_lower_ascii` redirected passes repeated-baseline main-menu and
  quick-fight-setup replays: 2 functions, 10 entry bytes, 576,340 arena bytes
  (576,350 total bytes written). The broad 2,433-entry hybrid still fails
  before the menu: after these ABI fixes it faults at tick 604 in linked
  mechanical `original_memcpy` (`0x030856e2`); demoting that unverified entry
  moves the fault to guest IP `0x4` at tick 469. Formal M1/M2 remain unpassed.
- M0 confirmed return-register type rule, 2026-11-07: when a recovered
  signature names one output register, mechanical C now uses that output's
  confirmed type as its return carrier. This resolves OpenWatcom's invalid
  return-register errors for narrow results (for example `0x23800`, whose
  callers observe `AL`; WDIS confirms the compiled body writes the low byte
  before return). Recompiled with the cached OpenWatcom v2 `wcc386`: `ok`
  rose from 395/1,020 (117,666 bytes) to 402/1,020 (121,560 bytes), and
  compile errors fell from 31 to 24. Current rate is 39.4% of functions and
  22.4% of available export body bytes; 594 remain `manual`. This changes
  M0 only; no new hybrid replay was run, and the latest M1/M2 evidence above
  is unchanged.
- M0 Ghidra string-label rule, 2026-11-07: string symbols can contain `>`
  in their display-derived names before the final guest-address suffix.
  The generator now recognizes that spelling and translates it to a guest
  memory pointer. The OpenWatcom v2 compile pass accepts three additional
  functions (`0x9ea0`, `0xdae40`, `0xdb21c`): `ok` is now 405/1,020
  (123,186 bytes), up from 402/1,020 (121,560 bytes), and compile errors
  fall from 24 to 21. Current M0 rate is 39.7% of functions and 22.7% of
  available export body bytes; 594 remain `manual`. This is compile evidence,
  not replay validation; M1/M2 status is unchanged.
- Hybrid link with current M0 report, 2026-11-07: linking the 405 current
  mechanical `ok` objects with the existing hand-written object set emits
  2,443 redirects: 2,043 hand-written and 400 mechanical, 12,215 original
  entry bytes, a 582,532-byte code arena, and 594,747 total patch bytes.
  692/753 object modules link; 2,281 functions are listed as excluded. This
  artifact has not been replayed. The prior 2,433-redirect image still has
  the known IP `0x4` fault at tick 469 after excluding `original_memcpy`, but
  that is not evidence about this new link. The required private Xvfb still
  cannot start: `/tmp/.X11-unix` is owned by `nobody`, and Xvfb refuses to
  create its socket. The focused two-function main-menu/quick-fight-setup
  replay remains the highest replayed hybrid slice; formal M1/M2 are not
  passed.
- Xvfb isolation probe, 2026-11-07: a user+mount namespace can overlay a
  root-owned private tmpfs at `/tmp/.X11-unix` without changing the host
  directory, but Xvfb still fails to bind its `local` and `unix` listeners
  inside that namespace. `strace` is also denied ptrace by the environment,
  so the bind errno is unavailable. No replay of the new 2,443-entry image
  was attempted without a functioning private display.
- M0 typed `cRam`/`dRam` globals, 2026-11-07: the generated `cRam` access
  at `0xc41c4` is an 8-bit self-modified state byte, and `dRam` at `0xdcb7e`
  and `0xdcb86` stores the adjacent binary64 constants `+709.7827128933839`
  and `-709.7827128933839`. The generator now maps those labels to guest
  byte/double accesses; it deliberately leaves the all-ones `cRam` sentinel
  untouched. OpenWatcom `ok` rose from 405 to 407/1,020 (121,560 to 123,359
  body bytes), and compile errors fell from 21 to 19. The new `cRam` and
  `dRam` translations compile in `0xc4167` and `0xdcb96`; this is compiler
  evidence, not guest behavior verification.
- Relink with 407 mechanical `ok` objects, 2026-11-07: the latest hybrid
  emits 2,444 redirects (2,043 hand-written, 401 mechanical), 12,220 entry
  bytes, and a 582,996-byte arena (595,216 total patch bytes). 693/755
  modules link, with 2,283 function exclusions. No replay was run: the
  private Xvfb socket bind still fails in the isolated namespace. M1/M2
  remain unpassed; the first failure for this image is unknown.
- M0 width from `CONCAT22`, 2026-11-07: `uRam0019721e` is the high 16-bit
  operand of `CONCAT22`, so the generator now reads it as `EF_U16` at its
  guest address. This avoids assigning a blanket width to other `uRam`
  labels, many of which are self-modified code immediates. OpenWatcom accepts
  `0x59adc`; `ok` rises from 407 to 408/1,020 (123,359 to 124,581 body
  bytes), and compile errors fall from 19 to 18. Current rate is 40.0% of
  functions and 22.9% of available export bytes; 594 remain `manual`.
- Relink with 408 mechanical `ok` objects, 2026-11-07: 2,445 functions
  redirect (2,043 hand-written, 402 mechanical), patching 12,225 original
  entry bytes and placing 584,292 bytes in the code arena (596,517 total
  patch bytes). 694/756 object modules link; 2,283 functions are excluded.
  This image was not replayed because Xvfb still cannot bind a private
  listener. M1/M2 remain unpassed and this image's first runtime failure is
  unknown.
- M0 confirmed register-parameter lowering, 2026-11-07: for confirmed
  register inputs, the generator now removes Ghidra's duplicate `in_REG`
  local declaration and uses its recovered pointer type for the C parameter.
  When a no-stack signature also has a referenced generic `param_N`, that
  name is mapped to the corresponding confirmed register input. This fixes
  three compile errors (`0xb00d0`, `0xb1b8b`, `0x11c0b4`); OpenWatcom `ok`
  rises from 408 to 411/1,020 (124,581 to 125,051 body bytes), and compile
  errors fall from 18 to 15. WDIS confirms the resulting `0xb1b8b` body reads
  through EAX and `0x11c0b4` reads EDX and writes through EAX. Current rate
  is 40.3% of functions and 23.0% of available export bytes; 594 remain
  `manual`. This does not verify runtime behavior.
- Relink with 411 mechanical `ok` objects, 2026-11-07: 2,448 functions
  redirect (2,043 hand-written, 405 mechanical), patching 12,240 original
  entry bytes and placing 584,852 bytes in the code arena (597,092 total
  patch bytes). 697/759 modules link; 2,283 functions are excluded. The
  private Xvfb listener failure prevents replay; M1/M2 remain unpassed and
  this image's first runtime failure is unknown.
- Menu replay with the current local M0 report, 2026-11-07: OpenWatcom accepts
  416/1,020 mechanical functions (125,300 body bytes; 589 manual, 15 compile
  errors). Original disassembly confirms `0xe9a24` has three stack arguments,
  callee cleanup via `ret 0xc`, and EAX/EDX pointer advances; its mechanical
  C lowering faults at arena IP `0x03087882` on tick 695. Added a persistent
  hybrid exclusion for this unresolved ABI. With it, the hybrid links 2,452
  entries (2,043 hand-written and 409 mechanical), 12,260 entry bytes and a
  585,684-byte code arena (597,944 patch bytes; 702/764 modules). The 500M
  main-menu replay then faults at guest IP `0x4` on tick 560. M1/M2 remain
  unpassed; menu navigation is not yet reached by this image.
- M0 confirmed register lowering, 2026-11-07: OpenWatcom `parm` inputs now
  name the containing register for narrow inputs (`DL` uses `EDX`, `BL` uses
  `EBX`), while narrow output carriers retain `value [al]`/`[ax]` as confirmed.
  WDIS for generated `0xb0208` reads the expected `DL` and `BL` bytes without
  stack arguments; the `0x15c071` listing uses EBX/ECX/ESI/EDI as confirmed.
  Generic fastcall formals are matched to confirmed live-in registers only
  when their use and width agree, and exact `unaff_REG` live-ins are lowered
  to those inputs. The full OpenWatcom report now has 416/1,020 `ok`
  functions (125,300 bytes), 589 `manual`, and 15 compile errors: 40.8% of
  functions and 23.0% of report body bytes. This is compile and ABI evidence;
  it does not pass the menu replay.
- Menu-focused hybrid checkpoint, 2026-11-07: an event-free 200M-instruction
  `main-menu` replay passes with a reduced 1,227-entry hybrid (6,135 redirected
  entry bytes; 591,819 total patch-write bytes). The replay reaches the same
  `instruction_limit` state as the original at IP `0x110de6`, with 14,547
  interrupts, 800 ticks, 756 IRQ0s, matching framebuffer output, and matching
  guest RAM hash at `0x195fb0:65536`; canary `0xe7835` is hit. This proves the
  reduced hybrid preserves the event-free main-menu replay through its budget,
  but does not yet prove interactive menu navigation. The full 2,452-entry
  hybrid still faults at guest IP `0x4`, tick 560. Four bisection rounds leave
  1,531 candidate entries and do not isolate the full-image failure. M1/M2
  remain unpassed; next checkpoint is a private-Xvfb replay with menu input
  against the passing reduced image.
- Quick-fight menu-input checkpoint, 2026-11-07: the 1,227-entry partial image
  exits at guest IP `0x222`, tick 904, when the replay's main-menu key events
  enter quick-fight. A four-round bisect finds a 154-entry composition that
  passes the 700M `quick-fight-setup` replay. A repeated-baseline strict replay
  confirms matching status/counters, framebuffer hash, and 64 KiB guest hash;
  this composition contains 142 hand-written `compiles`, 11 mechanical `ok`,
  and one previously verified replacement. Further bisection isolates
  `InitPickMiss` (`0x84b74`) as sufficient for the guest exit: its original
  five-byte body is a `JMP` to `0x41028`, while the mechanical C body does not
  preserve the thunk's register-forwarding ABI. Added it to the persistent
  hybrid exclusion list. Demoting only this entry from the 1,227-entry patch
  avoids the guest exit but still diverges in interrupts, IRQ0, framebuffer,
  and guest RAM, so the fix is necessary for that path but not sufficient for
  the larger subset. Cockpit evidence is recorded below.
- Cockpit checkpoint, 2026-11-07: the same 154-entry composition passes the
  strict repeated-baseline 700M quick-fight setup replay, then runs the 1.5B
  cockpit input replay to the instruction limit in 320x200 mode. The original
  frame shows the cockpit; the patched frame is black. Patched counters are
  39,060 interrupts / 5,386 IRQ0 versus 39,982 / 5,952 in the repeatable
  original, and the 64 KiB guest hash differs (patched IP `0x0`, original
  `0x110de6`). The cockpit bisector ran seven trials over two resumed passes,
  reducing 154 suspects to 38 but not isolating a single cause; remaining
  candidates are concentrated in scene state/nodes, camera, and scene helpers.
  A 20-entry trial from the retained cockpit candidates renders a framebuffer
  byte-identical to the original cockpit (100 redirected entry bytes; 585,784
  total patch-write bytes). Its status, IP, ticks, IRQ0 count, and final
  framebuffer match; interrupts are 39,973 versus 39,982 and the selected
  64 KiB guest hash differs. This subset contains 19 hand-written `compiles`
  replacements plus verified `to_lower_ascii`; it is close but not a strict
  M2 pass. The current checkpoint is quick-fight setup with strict equivalence
  and cockpit with matching pixels but a small control/state delta. The full
  hybrid's earlier main-menu failure at IP `0x4`, tick 560 remains unchanged.
- Menu navigation focused replay, 2026-11-07: replayed the retained
  154-function patch (770 redirected original entry bytes) against both the
  event-free main menu and the existing quick-fight key sequence into setup,
  with repeated original baselines under private Xvfb. Both cases match
  status, interrupt/tick/IRQ0 counters, framebuffer SHA-256, and the selected
  64 KiB guest hash; `to_lower_ascii` is hit 460 times at the menu checkpoint
  and 686 times in setup. This verifies the focused menu-to-setup slice, not
  the full M1 hybrid: the 2,452-entry image still fails during startup at IP
  `0x4`, tick 560.
- M0 rule 17, self-modifying function-symbol aliases, 2026-11-07: when a
  non-call `FUN_<address>` use overlaps a reviewed global beginning at
  `address+1`, lower it as a guest-memory value; lower `iRam`/`uRam` dwords
  only when they fall within a reviewed global range. Original disassembly
  shows callers `0xb69c` and `0x63d5c` writing the 32-bit immediate at
  `0xc4756`, immediately after the `sub eax, imm32` opcode at `0xc4755`;
  their other camera-origin values are dwords at `0xc475c` and `0xc4762`.
  Both generated bodies compile. OpenWatcom now reports 418/1,020 `ok`
  (127,079/543,917 body bytes; 41.0% of functions, 23.4% of bytes), up from
  416 (125,300 bytes); compile errors fall from 15 to 13 and 589 remain
  `manual`. This is compile evidence only. Relinking patches 2,453 functions
  (2,043 hand-written, 410 mechanical), 12,265 entry bytes and 587,348 arena
  bytes (599,613 total patch bytes; 704/766 modules). The repeated-baseline
  500M main-menu replay still faults at guest IP `0x190`, tick 561, on an
  unmapped read at `0x358be389`; 48 calls to `to_lower_ascii` occurred before
  the fault. M1/M2 remain unpassed; the focused 154-function menu/setup image
  remains the highest passing replay checkpoint.
- M1 hand-only control and fault-signature detail, 2026-11-07: linked a
  hand-written-only control image with 2,043 redirected functions (10,215
  entry bytes, 441,492 arena bytes). Its repeated-baseline main-menu replay
  stops at tick 559 on unsupported real-mode BIOS INT 13, while the full
  2,453-function hybrid faults at tick 561. Neither reaches the menu. Replay
  fault parsing and bisector signatures now retain Unicorn fault type, address,
  size, and instruction bytes. Parsing the prior full-hybrid bisect artifacts
  shows the same unmapped read (`0x358be389`, size 4, code
  `093589e38b35302a1300cd2172308935`) across trials, so that detail does not
  separate the present failure; the bisector's prior 50 trials remain
  unresolved and `to_lower_ascii` remains independently verified.
- Focused simulator-menu navigation, 2026-11-07: the retained 154-function
  hybrid (143 hand-written, 11 mechanical; 770 redirected entry bytes) passes
  the repeated-baseline `simulator-menu-key-choice` replay. The scripted Q
  and Enter input reaches the simulator difficulty and mission-selection
  screen. Original and patched runs match at the 700M instruction limit
  (33,348 interrupts, 2,800 ticks, 2,752 IRQ0s, IP `0x110de6`), with identical
  framebuffer SHA-256
  (`e19e27fcf72127a56a6b413c8fe7140282b9572a07c148af6c5a83daa2c95768`)
  and selected 64 KiB guest hash
  (`7d8931aaf9b6c03514c14258260489d68da529a53770edf20bb83a62a8306817`); the
  compiled `to_lower_ascii` entry is hit 686 times. This verifies a focused
  main-menu-to-simulator-selection path, not M1: the full hybrid still faults
  during startup before the menu.
- M0 external code-label addresses, 2026-11-07: for `&LAB_<address>` with no
  matching label definition in the current pseudo-C body, emit a guest-memory
  code pointer; local `LAB_` labels and `goto` targets remain C labels. This
  fixes three OpenWatcom declaration errors (`0xc4db4`, `0xdc5a9`, `0x101c54`)
  after checking the referenced addresses as code targets. Using the pinned
  OpenWatcom v2 wrapper directly (the Nix dev-shell invocation could not
  connect to the sandbox-denied daemon socket), the M0 report improves from
  418/1,020 `ok` (127,079/543,917 body bytes) to 421/1,020 (127,764 bytes;
  41.3% of functions and 23.5% of bytes); compile errors fall from 13 to 10
  and 589 remain `manual`. Relinking gives 2,455 redirects (12,275 entry
  bytes, 588,388 arena bytes; 600,663 total patch bytes; 706/769 modules).
  The repeated-baseline 500M main-menu replay still faults at IP `0x190`,
  tick 561, on the same unmapped read `0x358be389` (size 4, code
  `093589e38b35302a1300cd2172308935`); this is M0 compile evidence, not an M1
  pass.
- M0 address-only stack placeholders, 2026-11-07: synthesize a 32-bit local
  only when an undeclared `stack0...` symbol is used exclusively as an
  address-taken call argument. The two affected original callers (`0xfd164`
  and `0x15c0c4`) push EBP around register-only helper calls; the separate
  `0x1177f4` stack-frame-base use stays a compile error rather than receiving
  a guessed local layout. The pinned OpenWatcom rerun advances from 421 to
  423/1,020 `ok` (128,137/543,917 body bytes, 41.5% of functions and 23.6% of
  bytes); compile errors fall from 10 to 8 and 589 remain `manual`. The full
  relink has 2,457 redirects (12,285 entry bytes, 588,852 arena bytes;
  708/771 modules), but its 500M main-menu replay still faults at IP `0x190`,
  tick 561, on read `0x358be389`. Re-linking the retained menu subset with the
  current M0 objects yields 160 redirects (143 hand-written, 17 mechanical;
  800 entry bytes; 589,652 total patch bytes). Its repeated-baseline 700M
  simulator-selection replay passes at 33,348 interrupts, 2,800 ticks, 2,752
  IRQ0s and IP `0x110de6`, with matching framebuffer SHA-256
  (`e19e27fcf72127a56a6b413c8fe7140282b9572a07c148af6c5a83daa2c95768`) and
  selected guest hash
  (`7d8931aaf9b6c03514c14258260489d68da529a53770edf20bb83a62a8306817`).
  This confirms the focused menu path remains working; full M1 is unpassed.
- M0 recovered callback references and relative output paths, 2026-11-07:
  generated callers now cast references to already hand-recovered functions
  through their semantic declarations, while unresolved/manual callees get
  generic declarations for compilation and remain original in the hybrid.
  This allows `0x105e38` to compile when assigning recovered callback
  `destroy_sound_object_105da0` and passing still-original
  `0x105648`. The OpenWatcom driver also resolves `--out` before compiling;
  this fixes duplicated relative paths caused by invoking the compiler with
  the output directory as its working directory. The pinned compiler run
  improves M0 from 423/1,020 to 424/1,020 `ok` (129,717/543,917 body bytes;
  41.6% of functions and 23.8% of bytes); 589 remain `manual` and 7 have
  compiler errors. A full link from the available hand-object directory and
  current mechanical batch redirects 1,714 functions (8,570 entry bytes,
  591,060 arena bytes; 636/772 modules). Its repeated-baseline
  `simulator-menu-key-choice` replay still faults at IP `0x190`, tick 560,
  on the established unmapped read `0x358be389`; M1 remains unpassed. The
  previously recorded 160-function focused menu build remains the last passing
  menu-navigation composition.
- Focused menu-to-quick-fight replay and cockpit bisect, 2026-11-07: using
  the canonical `decomp/include/ef_abi.h`, current hand objects (including
  two pending ABI edits compiled with zero OpenWatcom warnings) and the
  424-function mechanical report links a 161-function menu subset (805 entry
  bytes, 591,060 arena bytes, 709/772 modules). The replay narrowed this to
  160 redirects after demoting `0xc4db4`; repeated-baseline main-menu and
  quick-fight-setup both pass with matching status/counters, framebuffer and
  guest hash (33,351 interrupts, 2,800 ticks, 2,754 IRQ0s and IP `0x110de6`
  for quick-fight setup). The 161-function cockpit run faults with an
  unmapped fetch at IP `0x15918c05`, tick 3,188. The automatic bisector
  isolates `renderer_normalize_scene_c4db4` as necessary for that failure;
  retaining its original bytes removes the fetch, but the cockpit run still
  reaches the instruction limit with divergent counters (39,060 interrupts,
  5,386 IRQ0s), framebuffer and guest RAM. Two residual bisector stages leave
  `0x8fe58` and `0x9195c` as a joint candidate set; demoting all three causes
  an `unsupported_interface` stop, so those are not yet validated fixes.
  Mark `0xc4db4` `compiles-bad` and exclude it from future hybrids pending an
  ABI/behavior correction. Main menu and quick-fight setup pass; M2 cockpit
  remains unpassed.
- M0 full-link count correction, 2026-11-07: the preceding M0 link entry used
  `ef_gen_abi.h` as the hybrid linker's ABI alias source. Re-linking with its
  canonical `decomp/include/ef_abi.h` produces 2,457 redirects (12,285 entry
  bytes, 591,060 arena bytes; 709/772 modules; 603,345 total patch bytes).
  The repeated-baseline 500M main-menu replay still faults at guest IP `0x190`,
  tick 561, on the unmapped read `0x358be389` (size 4, code
  `093589e38b35302a1300cd2172308935`). This supersedes the preceding
  1,714-function link count; M1 remains unpassed.
- M0 rule 16, generic EAX declarations for manual callees, 2026-11-07:
  generated callers now see an integer return carrier when Ghidra marks an
  unconfirmed callee `manual` for extra live-out registers. The excluded
  callee body remains original, and the declaration is only enough to compile
  callers that consume EAX. The pinned OpenWatcom v2 compile rises from
  424/1,020 to 425/1,020 `ok` (129,882/543,917 body bytes, 41.7% of functions
  and 23.9% of bytes); 589 remain `manual`, and six have compiler errors.
  `0x41fc` now compiles. The compiler was invoked directly from the Nix store
  because `nix develop` could not connect to the Nix daemon socket. Compiler
  acceptance does not establish the callee's ABI or behavior.
- M0 rule 17, 32-bit guest-address comparisons, 2026-11-07: make pointer
  comparisons explicit when a guest `EF_U32` value is compared with a known
  pointer local, a null pointer constant, or a generated callback address.
  The four targeted type-mismatch bodies compile, and the full pinned
  OpenWatcom pass rises to 429/1,020 `ok` (133,085/543,917 bytes, 42.1% of
  functions and 24.5% of bytes); 589 remain `manual` and two have compiler
  errors. Those remaining errors are an unclassified `uRam` label at `0xc41b0`
  and a stack-frame placeholder at `0x1177f4`; the latter's pointer arithmetic
  is not treated as a safe generic local. These pointer-width conversions
  lower the generated type model but do not verify the address globals.
- M0 rule 18, reject untyped RAM and caller-frame placeholders, 2026-11-07:
  leave unresolved `uRam`/`iRam` symbols and non-address-only `stack0x...`
  references as `manual`. Disassembly confirms `0xc41b0` writes into its own
  instruction stream; `0x1177f4` forms a pointer into the caller's stack
  frame. The complete OpenWatcom pass now reports 429/1,020 `ok` (133,085 /
  543,917 body bytes; 42.1% by function and 24.5% by bytes), 591 `manual`,
  and zero compiler errors. The 467 `extra return register` and 165
  `unpreserved register` manual cases remain the largest classes. The new
  manual classifications prevent unsupported placeholders from being
  mistaken for mechanical C; they do not increase the ok-rate.
