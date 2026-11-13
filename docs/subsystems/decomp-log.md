<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation log

Append M0 mechanical translation, hybrid milestone, and bulk recovery progress
here. Ghidra-derived function bodies remain only under ignored `local/`.

- Full-hybrid trace comparison, 2026-11-07: instruction traces from the same
  deterministic headless Unicorn setup first diverge after untouched caller
  `0x10e206` calls redirected entry `fill_bytes_b3fb0` (`0xb3fb0`). The caller
  enters with EAX=`0x4df090`, EBX=2, ECX=0xc, ESP=`0x3ffeebc`, and selectors
  CS=0x2f, DS=SS=7, ES=FS=0x17, GS=0. At return site `0x10e20b`, original
  code preserves EAX=`0x4df090`; the hybrid returned EAX=`0x4df091`. ESP,
  EBX, ECX, EDX and selectors matched. EFLAGS were `0x3246` original and
  `0x3202` hybrid. Original code saves/restores EAX around `0xdeb50`; the
  hand-written C wrapper did not. This is the first confirmed ABI divergence
  in the captured parser trace, preceding the later `0x9c558` INT 0 symptom.
  The trace run was headless because private Xvfb could not create its X11
  socket in this environment. M1 remains unpassed. Current full mechanical
  report: 442/1,020 `ok` (138,459/543,917 body bytes), 578 `manual`, zero
  compile errors.
- M0 trace fix and full-startup recheck, 2026-11-07: preserve EAX around
  `guest_fn_deb50_f` in `fill_bytes_b3fb0` and correct the ABI tracker: the
  original saves EAX and ECX, but EDX is volatile and is not saved. OpenWatcom
  1.9 compiles the module with zero warnings/errors; disassembly confirms a
  `push eax` before the helper and `pop eax` after it. In paired Unicorn traces,
  the return at `0x10e20b` now has matching EAX=`0x4df090`, ESP=`0x3ffeec0`,
  EBX, ECX, and selectors; subsequent caller instruction addresses align.
  The volatile EDX and EFLAGS still differ. The current 2,244-entry full
  hybrid (11,220 redirect bytes plus a 608,516-byte arena; 619,736 total
  patched bytes) still does not reach the menu: it stops at unsupported BIOS
  INT 13, IP `0x4dc`, after 13,749,992 counted guest instructions. The same
  original image reaches its 500M instruction budget at IP `0x110de6` after
  26,308,809 counted guest instructions, so the remaining startup divergence
  is not resolved. Private Xvfb remains unavailable (`SDL_Init: No available
  video device`). Mechanical report remains 442/1,020 `ok` (138,459/543,917
  bytes), 578 `manual`, zero compile errors.

- M1 trace diagnosis, 2026-11-07: a paired deterministic headless C++ Unicorn
  trace (guest DOS/VGA clocks, 100,000 instructions per timer tick, separate
  fresh overlays) places the first confirmed preserved-register divergence at
  `apply_options_82cec` (`0x82cec`), called from `0x8b472`. Both images enter
  with ECX=`0xc9c0`; the original body explicitly saves/restores EBX, ECX and
  EDX, as recorded in `signatures.tsv`. At the following caller boundary
  (`0x8b477`, immediately before the call to `0xcd4d4`), the original retains
  ECX=`0xc9c0`, while the hybrid has ECX=0. The OpenWatcom listing for the
  compiled body has a `push edx` but no `push ecx`: the aux pragma describes
  caller-visible effects but does not make this C body save the extra original
  callee-preserved register. This is a systemic arena/C ABI gap, not evidence
  for blind single-function demotion. The subsequent X-32 thunk trace shows
  ECX=`0x2f` at `0x734` in the original and 0 in the hybrid; at `0x4dc` the
  original's `pop ds` restores selector `0x2f`, while the hybrid restores 0,
  before the following `int 0x21` at `0x4e0`. Thus `0x4dc` is the DS-pop
  symptom, not the interrupt instruction itself. A linker-level
  preserved-register trampoline and generated OpenWatcom ABI pragmas are now
  implemented. Relinking the currently available 341-object set redirects
  1,744 functions (8,720 entry bytes) into a 492,036-byte arena, including
  2,828 trampolines (50,256 bytes); 257 modules link. This is link evidence
  only: the local OpenWatcom toolchain could not be invoked because Nix
  attempted to write its read-only fetcher cache, so the changed C sources
  have not been recompiled and the patched image has not been replayed. M1
  remains unpassed.

- Menu-focused hybrid diagnostic, 2026-11-07: rebuilt the 160-entry menu
  patch from 143 hand-written and 17 mechanical functions, then used the
  automatic bisector to isolate `0x8e494` as the single candidate that
  triggers the protected-mode selector free / #GP path (keep-canary
  `0xe7835`). `0x8e494` remains `unknown` and its generated ABI is
  unconfirmed, so it was demoted to original bytes for the follow-up replay.
  The resulting partial patch contains 159 redirects (795 entry bytes) and
  450,484 bytes of new code; total patch payload is 451,279 bytes. Under
  private Xvfb, main menu, quick-fight setup, and simulator-menu key-choice
  checkpoints pass (3/3). Mission accept still fails: the demoted build runs
  to tick 4,663, then makes an unmapped 16-bit write to `0x19655800` at guest
  IP `0x15bbbd`; the prior build
  with the candidate replacement instead reaches the selector restore #GP at
  IP `0x31a`, tick 4,557. Thus menu navigation is usable through setup in
  this partial diagnostic image, but M1's full `compiles` hybrid and M2 are
  not passed. Added opt-in Unicorn entry/interrupt register-and-stack tracing
  to support the next bisector investigation; this trace change was compiled
  and exercised locally.
- Menu accept follow-up, 2026-11-07: the bisector narrowed the unmapped-write
  path to `0x15c0c4` as a minimal failure-inducing candidate (alone it causes
  a selected-RAM mismatch). Its tracker entry is `typed/asm`, but M0 marked
  the mechanical draft `ok` and the focused linker included it. Keeping both
  `0x8e494` and `0x15c0c4` original produced a 158-entry patch (790 redirect
  bytes; 450,484 arena bytes; 451,274 total patch bytes). The accept replay
  now reaches its 6,000-tick instruction limit, matching baseline status and
  tick count. It remains divergent: 32,166 vs 32,175 interrupts, different
  framebuffer hash, and different selected-RAM hash. The first crash was
  removed, but menu acceptance and cockpit entry are not yet behaviorally
  verified.
- Menu regression check, 2026-11-07: the exact 158-entry demoted patch also
  passes all three earlier private-Xvfb checkpoints: main menu, quick-fight
  setup, and simulator-menu key choice (3/3). This confirms the two demotions
  preserve that partial navigation path.
- Demotion recorded, 2026-11-07: marked `0x15c0c4` `compiles-bad` and added
  it to `decomp/symbols/hybrid-exclude.tsv`, so future hybrid links retain its
  original body. A five-round bisect of the remaining no-crash mismatch did
  not converge: 58 candidate entries remain. The current first failure is
  selected RAM/framebuffer divergence (interrupt count differs by nine); M1
  and M2 remain open.

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

- M1 updated full-hybrid bisect, 2026-11-07: with 429 mechanical `ok` bodies
  and the current hand-object set, the linker redirects 2,462 functions
  (12,310 entry bytes, 594,500 arena bytes; 714/777 modules). The
  repeated-baseline main-menu replay faults at IP `0x190`, tick 561, on an
  unmapped read of `0x358be389` (size 4, code
  `093589e38b35302a1300cd2172308935`). The bisector preserves that exact
  fault signature despite tick shifts and reduces it to one replacement:
  `0xc36b4`. Original disassembly shows inputs in EAX/EDX/EBX/ECX, two stack
  arguments, and `ret 8`; Ghidra emits only two ordinary parameters and omits
  the register/stack arguments passed to `0xc3584`. Mark it `compiles-bad` and
  exclude it. This identifies the target failure culprit; M1 still requires
  replaying the hybrid with the original entry restored.
- M0 preserved-register ABI rule and M1 startup follow-up, 2026-11-07:
  confirmed `preserved` registers now become the complement `modify` set in
  generated OpenWatcom pragmas. The full M0 compile remains 429/1,020 `ok`
  (133,085/543,917 body bytes), 591 `manual`, zero compiler errors. A full
  hybrid with 2,461 redirects (12,305 entry bytes; 594,692 arena bytes)
  faults at invalid instruction `0x4df015`, tick 650; the updated bisector
  isolates `0xa9e70`. Demoting its mechanical GUI dispatch body changes the
  first stop to unsupported BIOS INT 13 at guest IP `0x318`, tick 650.
  Disassembly of `0xebcfc` shows that its fallback calls `0xa7ac8` with two
  stack arguments, while the existing declaration let OpenWatcom pass them
  in registers. Add the stack-only pragma and pass flags then amount, matching
  the original pushes; the object compiles with zero warnings. The corrected
  hybrid advances to an unmapped read at IP `0x3ffe6e7`, address `0x3d80c3c0`,
  tick 682. A 106-trial bisector run remains unresolved with 1,070 candidate
  redirects. Current hybrid status is below M1; the main-menu checkpoint is
  still the target.
- M1 replay tracing, 2026-11-07: `replay_suite.py` now preserves and validates
  optional manifest `trace_addresses`, and installs those hooks only for
  patched runs. A focused trace before the tick-682 fault records 0xebcfc
  once, 0xf227f 13 times, and the verified 0xe7835 72 times; traced entries
  0x9cfdc, 0xa7ac8 and 0xa7a64 are not reached. The fault remains the unmapped
  read at IP 0x3ffe6e7, address 0x3d80c3c0. M1 is still unpassed.
- Focused menu launch exploration, 2026-11-07: the saved 154-function menu
  hybrid was replayed again under private Xvfb. Main menu and quick-fight
  setup still match their repeated original baselines in status, timing
  counters, framebuffer and selected guest RAM. From the simulator mission
  selection screen, clicking the rendered “Akzeptieren” button reaches the
  mission loading screen. At 900M instructions, original and patched runs
  match status/counters and selected guest RAM; the framebuffer differs in
  eight pixels during the loading animation. At 1.5B, the original continues
  to IP 0x110de6 (32,175 interrupts / 5,952 IRQ0s), while the patched image
  reaches IP 0 (29,924 interrupts / 4,485 IRQ0s) with different guest RAM.
  The exact 154-entry composition was bisected for this launch case; after
  11 trials, 95 candidates remain in the 0x8bb50..0x944f4 scene/UI cluster.
  This does not change the passing menu-to-setup checkpoint or M1/M2 status;
  simulator launch remains unresolved.
- Focused menu ABI refresh, 2026-11-07: rebuilt seven OpenWatcom modules
  against the current `ef_abi.h`: the five changed implementation modules
  and both callers of `set_scene_angles`. All compile with zero warnings or
  errors. The refreshed hybrid has 160 redirects (143 hand-written, 17
  mechanical; 800 entry bytes, 594,740 arena bytes; 715/777 modules). Its
  repeated-baseline main-menu, quick-fight-setup, and simulator-menu-key-choice
  cases all pass with matching status, timing counters, framebuffer and
  selected guest RAM. A 1.5B-instruction mission-accept replay proceeds beyond
  menu selection; the original reaches its instruction limit at IP `0x110de6`
  (32,175 interrupts / 5,952 IRQ0s), while the refreshed hybrid stops at
  unsupported real-mode BIOS INT 13, IP `0x31a`, tick 4,557 (29,964
  interrupts / 4,504 IRQ0s). This is beyond the passing menu checkpoint; M1
  and cockpit remain unpassed.
- Mission-accept stop classification, 2026-11-07: traced the 160-redirect
  menu patch through the stop at IP `0x31a`. The C++ interrupt transcript
  records vector `0x0d` (decimal 13) at reported IP `0x318`, immediately after
  X-32 INT 21h AX=`0x3502`; the original payload bytes at `0x318` and `0x31a`
  are `pop gs` and `pop fs`. `Machine::interrupt` subtracts two from every
  interrupt IP and routes every vector except `0x21` through the BIOS handler,
  whose decimal diagnostic therefore mislabeled this protected-mode #GP as
  “BIOS INT 13”. The stop is a segment-restore exception, not a disk request;
  exception delivery/selector state is now the next M1 investigation. Menu
  checkpoints remain passing; M1 and cockpit remain unpassed.
- M1 hybrid link refresh, 2026-11-07: rebuilt all 340 current `decomp/src`
  modules with OpenWatcom, zero warnings/errors. Added shared protected-mode
  segment and port-I/O helpers, DOS dispatch bridges that preserve the
  original X-32 interrupt entries, and missing indirect-call adapters. The
  full link redirects 2,551 functions (12,755 entry bytes; 597,188 arena
  bytes; 718/796 modules; 609,943 total bytes changed), including 406 of 429
  mechanical `ok` bodies. The private-Xvfb 500M main-menu replay still fails
  at invalid instruction IP `0x155034`, tick 651, before `0x106bfc`; M1 remains
  unpassed. A 31-trial full-set bisect did not isolate a stable minimal set:
  demotions led to several different early failures and 637 candidates remain.
  The four-register callback adapter's emitted code exposed and fixed an
  incorrect stack offset; the fresh 340-module link and replay still stop at
  the same IP. The bisect must be rerun against that refreshed image.
- Timer-vector entry recovery, 2026-11-07: the M1 bisector's reduced trials
  produced unrelated early exits and did not reach its `0x222` canary, so its
  `0x1e9`/`0x235` candidates are not treated as culprits. Original payload
  disassembly shows `0x235` compares the requested handler and then enters the
  shared update path at `0x23b`; the old C body returned a Boolean and skipped
  the vector update. Replaced it with a C entry that composes the comparison
  with `set_timer_vector_23b`. OpenWatcom compiles the module with zero
  warnings and the emitted entry calls the shared implementation. The rebuilt
  hybrid redirects 3,449 functions (17,245 entry bytes; 601,460 arena bytes;
  740/810 modules). Its 500M main-menu replay still stops at unsupported
  real-mode BIOS INT 0, IP `0x9c55a`, before any required patch entry is hit;
  M1 remains unpassed and this function's runtime behavior is not yet reached
  by the menu replay.
- Replay trace correction, 2026-11-07: the preceding sentence is too broad.
  On a repeat of the same 500M Unicorn run under private Xvfb, the hybrid
  reports 16 patch hits, including `0x235` once and `0xb04` once. The three
  main-menu canaries (`0xe7835`, `0x106bfc`, `0x10d1da`) remain unhit, so M1
  is still unpassed; the trace establishes entry reachability, not behavioral
  equivalence of those patched functions. Execution stops at the original
  `0x9c558` thunk (`INT 0`, next IP `0x9c55a`), reported by the runner as
  unsupported real-mode BIOS INT 0. A temporary register trace at that stop
  showed ESI=`0x21`, but this is after the thunk loaded the interrupt input
  block and does not establish the interrupt number used to select the stub.
- M0 segment-register live-ins, 2026-11-07: translate read-only Ghidra live-ins
  `in_CS`, `in_DS`, `in_ES`, `in_SS`, `in_FS`, and `in_GS` through the existing
  `guest_seg_x` selector helper; writes remain manual. The full OpenWatcom pass
  moves seven functions out of `manual`: 434/1,020 `ok` (135,401/543,917 body
  bytes; 42.5% of functions, 24.9% of bytes), 584 `manual`, and two
  `compile-error`. Five newly generated functions compile. The two remaining
  compiler failures expose Ghidra's undeclared `curbrk` alias for the reviewed
  `_curbrk` global at `0x132a30`; no hybrid replay was run.
- M0 reviewed global-name aliases, 2026-11-07: resolve a leading-underscore
  source symbol spelling only when its alias is unique in `globals.tsv` and
  is not declared as a function parameter or local. This lowers Ghidra's
  `curbrk` references to the reviewed `_curbrk` location and clears both
  compiler errors from the prior tranche. The full OpenWatcom pass is now
  436/1,020 `ok` functions (135,803/543,917 body bytes; 42.7% of functions,
  25.0% of bytes), 584 `manual`, zero `compile-error`; no hybrid replay was
  run.
- M0 multi-line function headers, 2026-11-07: normalize Ghidra declarations
  through the opening brace before matching prototypes, so signatures split
  across lines are recognized. Confirmed stack-only ABIs whose argument
  count disagrees with the Ghidra prototype stay `manual` rather than
  producing a malformed C declaration. The full OpenWatcom pass is now
  438/1,020 `ok` functions (137,248/543,917 body bytes; 42.9% of functions,
  25.2% of bytes), 582 `manual`, zero `compile-error`; no hybrid replay was
  run.
- M0 unsigned carry flags, 2026-11-07: lower Ghidra `CARRY1`, `CARRY2`, and
  `CARRY4` to widened unsigned sums and extract the carry bit at the
  corresponding 8-, 16-, or 32-bit boundary. The full OpenWatcom pass moves
  five functions out of `manual`; 443/1,020 `ok` functions
  (138,559/543,917 body bytes; 43.4% of functions, 25.5% of bytes), 576
  `manual`, and one `compile-error`. The newly exposed error is an unmapped
  `unkbyte10` local in a sixth carry-only body. No hybrid replay was run.
- M0 x87 stack live-ins, 2026-11-07: classify `in_ST0` through `in_ST7` as
  unsupported register live-ins, rather than emitting locals with no source
  for the guest x87 stack state. This keeps the carry-only body at `0xfa3d4`
  manual and clears its misleading compile error. The refreshed full pass is
  442/1,020 `ok` functions (138,459/543,917 body bytes; 43.3% of functions,
  25.5% of bytes), 578 `manual`, zero `compile-error`; no hybrid replay was
  run.
- M1 dispatcher trace, 2026-11-08: paired the original and hybrid under the
  same instruction-timed C++ Unicorn setup. The failing full-image trace
  reached redirected entry `0x0b04` (original caller `0x0e2a`); its recovered
  path reached `run_dos_int_ex` and the call at arena address `0x03032418`.
  The emitted call targeted `0x9c558`, the `INT 0` stub, although the helper
  intended to call the original dispatcher at `0x9c47a`. `load_abi_aliases()`
  had mistaken the explanatory `0x9c558` in `guest_dos_int_x`'s header comment
  for that helper's entry address. Disassembly confirms the dispatcher scales
  ESI by three before selecting the interrupt stub; the wrong binding entered
  the table base and produced INT 0 while the register block held interrupt
  `0x21`. Alias parsing now accepts only a leading explicit entry annotation;
  the deliberate `emit_command_packet` entry annotation was normalized to
  that form. The two asset-free alias tests pass. A rebuilt image from the
  available object set patches 2,078 functions (10,390 entry bytes; 659,030
  arena bytes; 596/783 modules); its replay advances beyond INT 0, then stops
  at unsupported real-mode INT 13 (`EIP=0x4dc`, `AX=0x2506`, 158 interrupts).
  This rebuild is not coverage-equivalent to the prior 3,449-function image:
  the stricter alias rule exposes unresolved helper modules. M1 remains
  unpassed. Mechanical status remains 442/1,020 `ok` (138,459/543,917 bytes),
  578 `manual`, zero `compile-error`.
- M1 native64 fault trace, 2026-11-08: startup reaches and closes
  `EF2000.CFG`, then faults at guest IP `0x13`. Disassembly of original
  `0xa7990` shows it calls `0xbc421` and resumes at `0xa79c7`; `0xbc421`
  sets `EDX=1` before calling `0xbc450`, and original `0xbc450` forwards
  that register to `0xbc4da` as its second argument. The recovered
  `release_by_key_bc450` prototype had only the EAX input and called
  `guest_fn_bc4da` without EDX, losing the `extra` flag that controls
  `0xfa9a0` handle close. Updated both declarations and forwarded EDX.
  This is a static ABI correction; runtime replay remains pending. M1 is
  still unpassed, and no mechanical-generation numbers changed.
- Native64 hook-slot replay, 2026-11-08: corrected the original `0x348`
  helper ABI to declare its EBP clobber. OpenWatcom had kept the recovered
  `start_timer_734` table offset in EBP across that guest call; the original
  helper returns with EBP pointing at `0x196148`, so the recovered body wrote
  the selector outside the slot. Also corrected the `0x3f8` register inputs
  from the original caller/disassembly. The private-Xvfb decomp launch now
  reaches the menu and gets past the former `DS=0x6118` fault. It later exits
  to startup and faults at `0x9c231` (`AX=0x252b`, `ERROR=0xe834`); M1 remains
  unpassed. This image reports 1,744 recovered functions, 8,720 redirect
  bytes, 493,408 arena bytes and 257/341 modules linked.
- Native64 startup fault trace, 2026-11-08: the `0x9c231` frame's return
  address `0x030720ab` maps exactly to the ABI trampoline for `0x9c23d`.
  Original `0x9c23d` is a stack-preserving tail jump to `0x9c224`; the C
  replacement enters through a trampoline that pushes six preserved registers
  before tail-jumping into the X-32 wrapper. Its stack arguments are therefore
  read as saved EDI/ESI/EBP (`0x67`, `0x03ffe834`, `0x80`), producing the bad
  selector at `LES` in `0x9c231`. Added `0x9c23d` to the hybrid exclusion list;
  rebuilding yields 1,743 redirects, 8,715 redirect bytes, 493,390 arena
  bytes and 257/341 modules linked. The exclusion's runtime effect is still
  unverified: sandboxed Xvfb cannot bind its required X11 socket and `nix run`
  cannot access the Nix daemon. M1 and the interactive menu remain unpassed.

- Fill-helper stack fix, 2026-11-08: tracing the cleanup fault at `0x1104c7`
  back to its source showed `fill_bytes_b3fb0` (`0xb3fb0`) had filled an
  archive context with `0xf0` bytes. The function wrapped a C call to original
  helper `0xdeb50` in inline `push eax`/`pop eax`; OpenWatcom then read the
  fill-value parameter from the shifted stack slot, so a pointer ending in
  `0xf0` became the repeated fill value. Replaced that sequence with a small
  Watcom `#pragma aux` helper that saves ESI/EAX, invokes `0xdeb50` with the
  confirmed EAX/EDX/ECX helper ABI, then restores both registers. OpenWatcom
  2.0 compiles `leaf_helpers1.c` with zero warnings/errors. A differential probe
  with destination `0x03000000`, fill `0x5a`, count 32 passes 1/1 and compares
  all 32 guest RAM bytes. A patch containing only this replacement reaches
  startup, loading, and the main menu in a 20-second headless replay without a
  fault. This isolates and fixes the observed poison-fill crash; the broader
  hybrid still faults at `0x4dc`, so M1 is not passed. UI interaction is not
  reverified: private Xvfb cannot bind its X11 socket in this environment.
  For the next quick-fight interaction replay, the click target is the
  rectangle below “Schnelles Gefecht” at `(165, 110)`, not the label text.
- INIT3D dispatcher failure, 2026-11-08: replaying the current 1,743-function
  hybrid with the fixed DOS clock in headless native64 mode reproduces the
  `ini: expected '='` message and guest exit `0xff` after video mode `0x100`.
  The triggering recovered entry is `run_text_script_1093d8`; leaving that
  entry original removes the parser message and exit, but exposes a separate
  stack-fetch fault at `EIP=0x03ffee9e` while the framebuffer still shows the
  DIC logo. The entry is excluded from redirects and marked `compiles-bad`.
  The paired X-32 vector restore at `0x2507` was handled before the parser
  call and is not the cause. Main menu remains unpassed; private Xvfb could
  not bind because `/tmp/.X11-unix` is owned by `nobody`, so UI input was not
  tested.
- Logo startup stack fix, 2026-11-08: replay with current metadata patches
  1,742 functions (8,710 redirect bytes, 493,356 arena bytes; 257/341 modules)
  and no longer exits with `0xff` in the recovered INIT3D dispatcher. Its
  first remaining fault is a stack fetch at `EIP=0x03ffee9e` while the DIC logo
  is displayed. The original `0xe4188` caller pushes four arguments before
  calling `0xdc21c`; the original callee reads `[EBP+0x10..0x1c]` and ends in
  `ret 0x10`. The replacement body consumes OpenWatcom register arguments and
  ends in plain `ret`, leaving those stack arguments in place. Excluding
  `0xdc21c` restores the original call ABI and advances past the stack fault,
  but the guest then exits `0xff` parsing `supergui/System.gui` (`String "0" not
  recognised as a Variable`). The entry is marked `compiles-bad` pending a
  stack-aware thunk; menu remains unpassed.
- Startup follow-up, 2026-11-08: retaining the original `0xe94ec` text loader
  removes the `System.gui` parser error. With `0xe94ec` original, the next
  fault is an invalid read in original `0xe9a24`; its caller frame is `0xe4f31`
  and supplies a corrupt source pointer. The call path crosses `0xc92e4`, whose
  confirmed ABI has four register inputs, four stack inputs, and `ret 0x10`.
  The hybrid cannot build a stack-aware thunk, so restoring original `0xc92e4`
  advances farther to the next invalid read. Both replacements are excluded:
  `0xe94ec` for the observed parser behavior, `0xc92e4` for its unsupported
  stack ABI. The main menu remains unpassed.
- Main-menu milestone, 2026-11-08: original disassembly for
  `0xeb050`/`0xeb1f0` shows five caller-clean stack arguments; the recovered
  declarations incorrectly add four register parameters. Restoring both
  original entries removes the invalid framebuffer source read. A 20-second
  headless native64 run with the fixed DOS clock renders the German main menu
  at 640x400 without a guest fault. The patch redirects 1,737 functions,
  8,685 entry bytes, and has a 493,338-byte arena across 257/341 modules. The
  menu is reached, but the scripted quick-fight click remains unverified.
- Menu interaction follow-up, 2026-11-08: a fresh native64 run of the
  `84d38cd1` tree reaches video mode `0x100` and remains at the main menu; it
  does not reproduce the `0xff` exit from the older `g7f97d8c` run. On private
  Xvfb, clicking the gray rectangle below “Schnelles Gefecht” (guest mouse
  position `0xa3,0x6b`) opens the Quick Fight setup page. The page remains
  stable, but clicking its “Akzeptieren” control (guest mouse position
  `0x122,0x16f`) has not advanced to the cockpit. The build redirects 1,737
  functions, 8,685 entry bytes, and has a 493,338-byte arena across 257/341
  modules. Cockpit milestone remains unpassed.
- Mouse-call ABI fix, 2026-11-08: original `0xc4b95` reads one stack argument
  and returns with `ret 4`; its caller at `0xc9238` passes the block address
  through `guest_fn_c4b95_x`. OpenWatcom had passed the declared parameter in
  EAX, causing the original routine to read unrelated stack data and fault at
  `0xc4bdd`. Added the Watcom-only `parm []` declaration. The OpenWatcom Nix
  build and its package check phase pass; the private-Xvfb run now reaches the
  main menu and Quick Fight setup without that fault.
- Campaign resource ABI audit, 2026-11-08: original callers and disassembly
  confirm `0xea3ec` receives its descriptor in EAX and lookup text in EDX,
  forwards both to `0xea20c`, and returns the bsearch result's presence in AL.
  Corrected that ABI and replaced the drafted `0xea20c` four-register call
  with the local five-argument bsearch call, which supplies the comparator on
  the stack and returns EAX. OpenWatcom compiles both modules with zero
  warnings; disassembly confirms those argument locations. The exact private
  Xvfb campaign-create replay still faults at `IP=0xffffffff`, opcode
  `0x8603`. The original descriptors `0x1603f4` and `0x16040c` have zero at
  `+0x0c`, so this path skips the cache lookup; the ABI correction does not
  resolve the observed failure. Main menu remains reachable. Next trace the
  redirected file callbacks `0x9cb20` and `0x9cbc4` used by the Norway load.
- Campaign renderer stream ABI fix, 2026-11-08: the fault context points into
  threaded renderer handler `0xc4e9d`. Original disassembly confirms the four
  `0xc5390`/`0xc539c`/`0xc53a8`/`0xc53df` helpers advance implicit EBX by 2 or 4;
  their C replacements instead leave EBX untouched and return the advanced
  pointer in EAX. The `0xc4e3b` entry is also a tail-jump into the handler table,
  which its C replacement cannot model. Excluding only `0xc4e3b` while leaving
  the four stream helpers compiled still faults at `IP=0xffffffff`; restoring all four EBX
  stream helpers prevents the fault and loads campaign and navigation data in
  private Xvfb. The run reaches the Kampagne choice dialog, but the original
  build proceeds to the campaign map after the same input, so the subsequent
  callback transition remains unresolved. Hybrid count: 1,733 functions,
  8,665 redirect bytes, 493,242-byte arena across 257/341 modules. Main-menu
  startup path remains unchanged.
- Campaign renderer stream return registers, 2026-11-08: a disassembly review
  of `0xc4f0d` showed `movzx ecx,ax` immediately after the `0xc53a8` handler,
  so the word helpers must return the swapped word in AX as well as advancing
  EBX. The dword helper similarly returns EAX and advances EBX. The prior C
  replacements modeled EBX but left the return register unspecified. OpenWatcom
  now emits the swapped value into AX/EAX and updates EBX for all four helpers;
  the rebuilt campaign replay reads both campaign and navigation files without
  fault, but remains on the campaign-choice dialog and does not issue the final
  campaign-file read observed in the original. Thus this fixes the helper ABI
  mismatch, but the create-screen transition still needs a separate UI trace.
- Standalone build bootstrap, 2026-11-09: the flake now separates a GCC Linux
  ELF (`ef2000-decomp`) from a 32-bit OpenWatcom/CauseWay DOS executable
  (`ef2000-decomp-watcom`) launched through DOSBox-X. Both link the recovered
  `hiscore_qualifies` source to a minimal common `main` with synthetic data,
  while the existing original-byte hybrid is retained under `ef2000-hybrid`.
  GCC successfully compiled 338/341 recovered C modules to native objects;
  three Watcom-specific/nonportable modules remain excluded, and all 338
  objects are archived for future linking. The GCC bootstrap compiles and
  passes its recovered-C self-test. The DOS link is defined using OpenWatcom's
  documented `system causeway`, but is not locally verified because Nix and
  OpenWatcom are unavailable. **Neither build plays the game**: recovered
  startup, global definitions, engine bindings and platform services are
  prerequisites. This does not complete plan M1, §7 or §8.
- Native GCC image-mapping foundation, 2026-11-09: added an explicit 64 MiB
  guest-address translation for `EF_DECOMP_NATIVE` instead of dereferencing
  32-bit offsets as Linux pointers. The Linux bootstrap accepts `--image`,
  SHA-256-verifies the supported `_EF2000.EXE` without distributing it,
  extracts the initialized X-32 application payload from its documented
  two-stage envelope and allocates zeroed BSS. Synthetic envelope/bounds tests
  pass, as do recovered stream-reader and 8x6-glyph routines operating on
  synthetic guest RAM. All 338 previously portable C modules compile with
  `EF_DECOMP_NATIVE` (warnings remain for guest callback offsets cast to host
  function pointers). No game instructions execute and no first-screen output
  is presented. Still required: initialization of original game globals,
  recovery of the main entry at `0x8b274`, native binding of callbacks and
  enough recovered video/platform services to display a real frame.
- Standalone GCC first startup call, 2026-11-09: linked the actual recovered
  `init_arena_e29a0` (0xe29a0) and `reset_buffer_e2a1c` (0xe2a1c) through
  native `guest_fn_c3f70`, `guest_fn_9c3cf`, `guest_fn_e2a1c` and bounded
  `guest_fn_b3fb0` bindings, with separate-function linker sections to discard
  unrelated/unresolved guest paths. The new optional Linux `--image ...
  --probe-arena` path executes just this first game-owned startup step after
  SHA-pinned image loading and reports its arena pointer/size and cleanup
  callback. Synthetic data tests validate allocation, reset, globals, hook,
  refusal on repeated or uninitialized calls, and memory-span bounds. This is
  not a guest differential, does not promote either function to `verified`,
  and still stops before TACTCOM init (`0x8b39c`) or video setup.
- Native GCC game-main prefix, 2026-11-09: the supplied German game archive
  contains the expected 1,684,561-byte `_EF2000.EXE` (SHA-256 matches
  `docs/executable.md`). The existing native arena probe passes directly on
  the real initialized payload: 3,000,000 bytes at 0x4df000 and cleanup
  address 0xe2a50. Disassembled and reconstructed the 100-byte game entry
  `0x8b274`, plus its ASCII case-insensitive comparator at `0x9d524`.
  `--image ... --probe-main` executes the C main prefix on the real executable,
  initializes the recovered arena, and halts explicitly at the first missing
  TACTCOM call `0x8b39c`; the synthetic alternate argc=2 `playvideo` branch
  stops at unrecovered reset `0x8b2d8` without allocating an arena. Neither
  function is Watcom/guest-differential verified. The first screen remains
  blocked by TACTCOM's many library and platform dependencies.
- GCC/Linux TACTCOM prefix, 2026-11-09: translated only the 0x8b39c..0x8b43c
  memory/diagnostic part of the 402-byte TACTCOM initializer, leaving the
  remaining initializer explicitly unrecovered. The `0x880` DOS 350Bh memory
  wrapper, recovered 0x8b328 comma formatter, 0xea4a7 stdout flush wrapper,
  guest string copy and native printf adapter are now wired into the bounded
  game-main probe. The X-32 free-memory response is deliberately *synthetic*:
  unused 64 MiB guest RAM after the recovered arena, not a claim of matching
  original extender allocation state. With the supplied SHA-pinned executable,
  `--probe-main` now prints TACTCOM's original memory diagnostic (57,618K in
  that synthetic memory model), installs logging callback offsets 0xea50c and
  0x18a838, and stops before configuration path initialization at `0x9c980`.
  Synthetic high-memory and low-memory tests exercise the two boundaries;
  there is still no game screen. Next: reconstruct 0x9c980's directory/path
  dependency, the file-existence callback and the remaining TACTCOM setup
  calls. The original executable remains external and read-only.
  Validation: 410 asset-free Python tests pass (81 skipped), including GCC
  synthetic normal/low-memory TACTCOM branches, and the opt-in SHA-pinned
  real executable `--probe-main` run. Full original-asset suite still has
  three unrelated save-state `legacy` keyword failures in the Python harness;
  Nix/OpenWatcom are unavailable in this workspace.
- GCC/Linux first TACTCOM resource pathname, 2026-11-09: extended the bounded
  0x8b39c prefix through 0x8b459. It executes already-recovered
  `build_default_path_9c980`, `getenv_9c858` (environment table),
  `lowercase_string` 0xeb478, `strcat_9ca57` and callback 0x132ab4 to
  `find_in_archives_9cb20`, rather than stopping before the path builder.
  The SHA-pinned original image has no DOS environment table yet because the
  native C-runtime setup is incomplete; it therefore generates the relative
  first resource pathname `\\check.cd` and stops before the original
  `read_file_payload_size_b3b90` disk reader. A synthetic DOS `DID` env
  directory validates case folding, concatenation and the callback dispatch.
  The host checks the original 0x132ab4 callback offset and refuses archive
  access before initialization; there is no file I/O in this probe. The
  remainder of 0x8b39c and the first screen are still unimplemented.
- GCC/Linux first TACTCOM disk-file reader, 2026-11-09: recovered the 114-byte
  `0xb3b90` file-size function from the original disassembly and connected it
  to existing `0xa7900` open, `0xa79d4` read, `0xa7ae0` seek-end and `0xa7990`
  close recovered-C helpers. It reads eight header bytes into guest address
  `0x41f360`; JR returns length at +4 plus 16, RA returns length at +4, other
  headers use the actual file length, and an open failure returns zero.
  Added Linux-only, read-only `bc36a` / `f1d90` / `a7a64` / `bc421` adapters
  backed by five host stream slots. Paths are rooted at the game executable's
  containing directory or `--data-dir`, with DOS case-insensitive lookup and
  explicit rejection of parent traversal and symlinks. The real callback at
  `0x132ab4` now returns to the recovered TACTCOM prefix, which takes the
  original `0x8b460..0x8b472` missing-versus-present branch. The probe stops
  at diagnostic callback `0xea50c` if `\check.cd` is absent, or before options
  call `0x82cec` if present; neither the diagnostic itself nor option/display
  initialization is linked. The native adapter never writes original data.
  Synthetic GCC tests cover JR, RA, ordinary files, missing files, mixed-case
  DOS paths, blocked `..` paths, both TACTCOM branches and file-slot cleanup;
  opt-in testing against the SHA-pinned original executable also passes.
  Reached farther into the normal game-owned startup; no screen rendered yet.
- GCC/Linux missing-CD diagnostics and partial options, 2026-11-09: advanced
  the bounded TACTCOM prefix through the `0x8b472` options call. The missing
  `check.cd` path handles this specific no-argument `0xea50c` format using
  recovered `copy_string_9d749` and `enter_handler_eb448`, preserves the guest
  nesting/message globals (`0x160638`, `0x17d404`, `0x160a24`), converts DOS
  CP437 to UTF-8 for a read-only Linux stderr diagnostic, and stops at the
  unimplemented `0xeb25a` page switch. The presence path runs recovered
  `apply_options_82cec`, `select_handlers_9cfdc`, `strcat_9ca57` and
  `run_steps_d6694`. Synthetic tests confirm the `german` language handler
  callbacks, constructed language subdirectory and copied option flag. This
  branch uses a detached-gameport probe adapter for `0xd66c2`: repeated DOS
  `IN AL,DX` samples on port `0x201` would all return `0xff`, clear the four
  button latches and return zero after the original's 20-pass stability loop.
  The next stop is `0xe94ec`, the still-unrecovered GUI/text file loader;
  other later unimplemented options calls are also explicit stop boundaries.
  The probe does not implement the full variadic `0xea50c` formatter, guest
  log-file writing, `0xeb25a` screen switch or connected-joystick I/O. Synthetic and
  opt-in SHA-pinned real-executable GCC tests pass; no first screen yet.
- GCC/Linux initial options text loader, 2026-11-09: removed the `0xe94ec`
  stop and executed the recovered `load_text_file_e94ec` via native bound
  callbacks. Recovered the three previously untracked original routines
  `0x9cf2c` (72 bytes), `0x9cf74` (48 bytes), and `0x9cfa4` (56 bytes),
  using bounded guest scratch in place of Watcom-local stack buffers. They
  construct the language-prefix pathname, size the localized candidate via
  recovered `0x9cb20`, fall back to the original pathname, and read using
  `try_handlers_9cbc4`. The Linux host reuses its existing secure read-only
  DOS path resolution; plain files are copied byte-for-byte to guest arena
  memory and compressed JR/RA files stop at `0xb3c04` instead of being
  misinterpreted as text. Important original behavior: the localized path
  builder does not insert an additional slash for the bare `EF2000.CFG`
  name, so the checked synthetic `german/EF2000.CFG` fixture is not chosen;
  the ordinary file is read. `check_arena` now verifies arena accounting
  after the legitimate C `0xe2a94` text allocation. On successful config
  load the next boundary is `0xe93dc`, the options parser; on a missing
  config it is `0x30214`. Synthetic and SHA-pinned real-image GCC runs
  exercise these paths. The loader remains `compiles-bad` for the hybrid
  pending original-guest differential/parser verification. No graphics yet.
- GCC/Linux outer configuration parser, 2026-11-09: connected recovered
  `parse_script_text_e93dc` (original `0xe93dc`, 272 bytes) to the actual
  game-owned startup path after loading `EF2000.CFG`. Its cursor, line counter,
  source table, and parser flags remain guest-RAM state. At `0xe882c` the
  source-recovered comment-only branch clears its three scratch globals and
  returns success for a semicolon line shorter than the original 0x84-byte
  line limit. Any real setting assignment still stops at the exact `0xe882c`
  entry, rather than guessing the missing 2,990-byte table interpreter.
  Synthetic and SHA-pinned real-image tests now cover this boundary as well
  as a comment/whitespace-only file: that complete parse reaches recovered
  `release_buffer_e9574`, resets the arena allocation, and stops at the next
  unknown `0x30214` (4,119 bytes). No graphics or actual option decoding yet.
  The original `0xe882c` disassembly identifies a 16-entry settings descriptor
  table at `0x12b370` with an 18-byte stride: key pointer at +0, type at +4,
  destination at +5, scalar width at +9 and callback pointer at +14.
  `FULLSCREEN`, `CONTROL`, `DETAIL`, `MODEL`, `STICK`, `HUDCOL`, `NETMODE`,
  `REZ`, `JOY_FACTOR` use type 3; the remaining seven keys use type 5 with
  subhandlers. Type 3 dispatches through the original numeric conversion
  formatter at `0xe8d7c`, then writes 1, 2 or 4 bytes by +9. This describes
  the next recovery target; the probe deliberately does not yet perform
  these assignments without validating numeric conversion/error behavior.
- GCC/Linux scalar configuration assignment, 2026-11-09: recovered a bounded
  path through original `0xe882c` for simple type-3 signed decimal values:
  lex key in the original 0x84-byte line budget, match its 18-byte descriptor
  in guest RAM, scan the `%ld` numeric format from `0x190ed8`, update parser
  scratch/cursor, and write 1-, 2-, or 4-byte destination values. Only the
  supported full-token decimal subset is accepted (including 32-bit minimum
  and maximum); original permissive conversion/error paths are left as a
  boundary. Recovered the original 16-byte `CONTROL` descriptor callback
  `0x82710` independently: it copies byte `0x1a5c0b` to `0x1fcb30` and
  `0x420960`. Synthetic tests assert each width, signed wrap/truncation,
  callback mirrors, case folding, scratch fields and unsupported cases.
  Opt-in SHA-pinned executable integration confirms `CONTROL=1` and other
  scalar-only configs now pass arena release to unknown `0x30214`; a shipped
  settings prefix (`DETAIL=2`, `REZ=0`, `CONTROL=1`) reaches the next
  unimplemented complex `MUSIC=1,127` at `0xe882c`. This is a partial
  route inside the 2,990-byte token interpreter, not a complete recovery;
  no video first screen yet.
- GCC/Linux MUSIC/SOUNDFX list settings, 2026-11-09: recovered the
  text-copy case of original `0xe882c` (jump at `0xe8e1d`, string copy at
  `0xe935b`, scratch length at `0xe939d`) for two type-5 descriptors. Their
  source descriptor fields are width 0x50, destination `0x1fcb50`, and
  per-setting callbacks. Recovered `MUSIC` callback `0x8289c` and
  `SOUNDFX` callback `0x82814`: both scan `%hd,%hd` from `0x188b94`, clamp
  signed 16-bit volume to 0..127, and update runtime enable/volume fields.
  `SOUNDFX` preserves the original overlapping high-byte test for its
  quality flag `0x15b7e5`. Only complete, valid numeric pairs are accepted;
  malformed/incomplete `sscanf` and other type-5 callbacks remain boundaries.
  Synthetic tests verify scratch text/length, callbacks, volume limits and
  unsupported cases; opt-in SHA-pinned startup advances the shipped prefix
  through MUSIC and STICK to the unimplemented JOYSTICK callback at `0xe882c`.
  A sound-only config completes the parser and reaches unknown `0x30214`.
  No first screen or full interpreter recovery yet.
- GCC/Linux remaining shipped config callbacks, 2026-11-09: recovered the
  423-byte JOYSTICK list callback `0x828f8` for the existing unplugged
  gameport model (ten `%d` values and a final `%hd` into `0x198432`). Its
  guest calibration globals, clamped scale/threshold words, axis extrema,
  overlapping 32-/16-bit fields and signed callback `0x1c548` are now updated
  in the source-recovered order. Indirect `[0x15bab8]` resolves to `0xd6960`
  in the pinned executable; the native model follows only its no-device
  `0xd66c2` path, clearing button latches and clamping axes, with connected
  sampling explicitly unmodeled. Recovered `0x826f0` SPEECH (nonzero string
  becomes two enable bytes) and `0x82c34` VRHELMET (ten-way mode/submode
  mapping) from original disassembly. Synthetic RAM tests cover calibration,
  signed extrema, the sound callbacks, speech, VR modes and incomplete input.
  Opt-in GCC/Linux startup test now runs the complete 146-byte shipped
  `EF2000.CFG` in original option order and reaches `0x30214` after freeing
  its buffer, rather than stopping at `0xe882c`. The next milestone is
  recovering `0x30214` (4,119 bytes) to progress toward the first screen;
  no original-guest differential or Watcom splice for these parser paths.
- GCC/Linux generic-sequence loader prefix, 2026-11-09: identified original
  `0x30214` (4,119 bytes) as `ReadGenSeqs()` in `Views.c`, corroborated by
  its embedded error string after the resource path. The initial instructions
  `0x30214..0x3024d` copy five bytes from `0x182708` and the default word
  `0x182710` into all forty 0xa1-byte slots at `0x19cbb0` (names at +0x40).
  `0x3024e..0x30271` initializes a private 0x94-byte path frame with the
  original recovered default-path helper `0x9c980`, zeroes the word at
  frame +0x90, and appends the original `0x182794` DOS resource name
  (`BG\\views\\views2.dat`) with `0x9ca57`. It next calls `0x9ccb8` to open
  the resource. A new derived-source prefix under `decomp/src/views_sequences.c`
  reproduces these writes without running any original machine code. Synthetic
  RAM tests check all forty records and untouched bytes; the SHA-pinned
  startup test confirms the path and the next `0x9ccb8` boundary. No archive
  opener, file parser, original-guest replay, Watcom splice or first screen
  is claimed; the 4,119-byte function remains `named`, not `compiles`.
- GCC/Linux Views resource opener and stream context, 2026-11-09: checked the
  original `0x9ccb8..0x9cde6` disassembly and reused recovered game-handle
  primitives `0xa7900`, `0xa79d4`, `0xa7ac8`, and `0xa7990` through the
  existing read-only DOS-path adapter. The native probe handles absent
  resources by returning `-1` to the recovered `ReadGenSeqs` two-argument
  diagnostic call (`0x1827a8`, pathname), stopping at its original callback
  `0xea50c`. A synthetic, uncompressed resource exercises the eight-byte
  magic check and rewind, then allocates a bounded 0x56c-byte guest stream
  record through recovered `0x9d060`/`0x9d114`, sets its leading byte to
  `0x63`, and stops at original line parser `0x9d1e8`. JR/RA resources stop
  at the packed branch `0x9cd53`; configured archive contexts stop before
  `0xebb58`. The separate 0x56c guest allocation is a tightly constrained
  host probe, not a general X-32 heap. Header reads shorter than eight bytes
  remain unsupported because the original tests uninitialized stack bytes.
  SHA-pinned original executable and synthetic filesystem fixtures validate
  these boundaries without adding proprietary assets to the repository.
  There is still no Views file parser, archive decoder, Watcom splice,
  original-guest differential, or playable first screen; `ReadGenSeqs`
  remains `named`, not `compiles`.
- GCC/Linux Views line reader and first sequence-record defaults, 2026-11-09:
  disassembled the original `0x9d1e8..0x9d274` non-comment text-line loop
  and added its recovered C body in `runtime_helpers31.c` (the original
  function's assigned module). Its `0x400`-byte guard, leading whitespace,
  CR/LF and semicolon behavior, physical line counter, and fatal callback
  are preserved. The original `0x9cde8` plain-handle read is supplied by a
  read-only probe adapter, allowing the already recovered `0x9d170`
  256-byte buffered character reader and `0x9d278` token reader to execute
  unchanged. `ReadGenSeqs` now compares the first token against original
  `0x18274c` ("#") through recovered `0x9d524`, executes the exact
  `0x302e1..0x303f8` field writes for the first 0xa1-byte sequence record,
  and stops at `0x303f9` before reading the record name. Non-`#` tokens
  stop at the original `0x31201` skip branch rather than inventing its loop
  and cleanup. Synthetic GCC tests cover the record's individual default
  fields and next-slot isolation, comment/blank-line skipping, CRLF across
  a 0x100-byte refill, unrecognized input, and overlong-line callback;
  the SHA-pinned opt-in executable uses only a synthetic Views resource.
  Packed/archive input, sequence options and subsequent records remain
  unsupported. No original guest differential or Watcom splice; the 4,119-
  byte `ReadGenSeqs` remains `named`, not `compiles`.
- GCC/Linux Views record name and camera mode, 2026-11-09: inspected the
  original `0x303f9..0x304fb` instructions after the first '#' record
  defaults. The 0x9d278 token reader supplies the remaining header token
  as the record name, copied through the existing 0x9d749 implementation
  into record offset +0x00. The 0x9d1e8 line reader advances to the next
  physical line; the original then discards its first token and compares
  the second token case-insensitively against six embedded labels at
  0x1827d8/0x1827e0/0x1827f0/0x1827fc/0x182810/0x18281c. The matching
  value 0..5 is stored at record +0x3c, or that byte is untouched if no
  label matches. The bounded native probe stops at original `0x304fc`
  before the remaining option records. Because the original 0x30407
  `strcpy` does not check its destination, the host probe explicitly stops
  at that call for names of 30 or more bytes; shorter strings preserve
  original behavior. Synthetic GCC tests exercise all six labels (including
  lowercase), unknown token retention, name copying, a long-name guard,
  and the multi-line 256-byte refill; the pinned original executable also
  runs with a synthetic Views resource. No packed/archive decoding, later
  options, multi-record loop, guest differential, Watcom splice or first
  screen is claimed; `ReadGenSeqs` stays `named`, not `compiles`.
- GCC/Linux Views text options, 2026-11-09: extended the bounded first-record
  parser through original `0x304fc..0x305e0`. The recovered line/token loop
  compares embedded `Name` (0x182828), `Key` (0x182830), and `Location`
  (0x182834) labels case-insensitively, copies Name at record +0x1e and Key
  at +0x40 through the existing 0x9d749 helper, and writes record +0x3d as
  zero for embedded `INSIDE` (0x182840), one for any other location. Each
  recognized option loops to the next physical line; the first unsupported
  field stops at original numeric-dispatch address `0x305e1`, before the
  `PosDamping` x87/parser path. The original string copies at `0x3053d` and
  `0x30577` are unchecked, so GCC host-only guards stop at those addresses
  for tokens that would overrun the 30- and 15-byte destination regions.
  Synthetic tests cover quoted names, Key, INSIDE/OUTSIDE, case-folded option
  spelling, multiple lines crossing the 256-byte refill, and both overlong
  writes; a SHA-pinned real-image opt-in GCC probe uses only invented Views
  text. No numeric options, subsequent records, guest differential, packed
  resources, Watcom splice or playable screen is claimed; `ReadGenSeqs`
  remains `named`, not `compiles`.
- GCC/Linux Views scalar and first vector options, 2026-11-09: inspected
  original `0x305e1..0x30aae` and extended the first-record option loop with
  `PosDamping` (record +0x3e), `AngDamping` (+0x3f), `MinAlt` (+0x4f),
  `RollLock` (+0x53), `OffSet` (+0x60/+0x64/+0x68), `RandomOffset`
  (+0x54/+0x58/+0x5c), and `InitialOffSet` (+0x6c/+0x70/+0x74, then
  +0x78 += 3). All labels are compared at their original guest-image
  addresses; `RollLock` matches the six values `NONE`, `HORIZON`,
  `BANKING_HORIZON`, `DAMPED`, `RPLANE`, `INSIDE`, retaining the default
  for unknown labels. The three scalar options use the original 100/32
  float constants and truncation pattern (100 minus damping, negative
  altitude multiplied by 32); the vectors scale each component by 32.
  The 462-byte decimal converter `0x9d55d` is still not reconstructed:
  a narrow host-only GCC adapter accepts ordinary finite decimals via
  `strtod` and stops at `0x9d55d` on malformed/special forms rather than
  claiming the original's error, overlong-mantissa, or x87 edge semantics.
  Synthetic cases cover negative/exponent decimals, truncation, all six
  RollLock values, unknown-value retention, vector field mapping and the
  increment, plus invalid decimal input; SHA-pinned opt-in startup runs
  with invented resource text. The boundary is now `0x30aaf` before
  `LookAtObj`; later options, packed/archive resources, following records,
  guest-state differential and Watcom splice remain pending. The whole
  `ReadGenSeqs` remains `named`, not `compiles`.
- GCC/Linux Views look-at, clamp and HUD options, 2026-11-09: followed
  original `ReadGenSeqs()` disassembly from `0x30aaf` through `0x30f58`,
  extending the first-record line-option dispatch. `LookAtObj` compares its
  argument to the embedded string **`SOURCE`** at `0x1828d0` (a substring of
  stored `RESOURCE`) and stores record +0x7c as zero for SOURCE, one for any
  other value. `LookAt` reads three 16-bit components at +0x7d/+0x7f/+0x81;
  `Dist` reads four at +0x87/+0x89/+0x8b/+0x8d. Each value uses the original
  guest-image float 32 and integer truncation, retaining the low word.
  `XClamp` enables word +0x8f and reads two 182-scaled signed words at
  +0x91/+0x93; `YClamp` enables +0x95 and reads two at +0x97/+0x99.
  `HUDCenter` and `HUDHeight` read unscaled truncated words at +0x9c/+0x9e.
  The 182.0f scale at `0x182a05`, 32.0f at `0x182a09`, embedded option
  labels and record offsets all come from the SHA-pinned original image.
  Synthetic GCC cases cover selector case-folding, nonmatching selectors,
  16-bit signed values, both clamp flags, x87-like truncation and case-folded
  option names. The opt-in GCC probe runs an invented text resource with
  the real original image; no proprietary resource is committed. The next
  explicit boundary is `AInsts` at `0x30f59`. The source guest decimal
  converter `0x9d55d`, later options, repeat records, archive/packed
  resources, guest-state differential and Watcom splice remain unimplemented;
  do not promote `ReadGenSeqs()` beyond `named`.
- GCC/Linux Views instrument and cockpit selectors, 2026-11-09: examined
  original `0x30f59..0x310c7`. The `Insts` option at `0x182910` was called
  `AInsts` in the previous boundary notes, but its actual pointer begins at
  `I` within that adjacent text; the accepted spelling is `Insts`. It reads
  one token and compares six independent, case-insensitive embedded strings,
  writing record byte +0x9b as 5 for `FrontInternalCock` (0x182918), 2 for
  `BottomInfoBar` (0x18292c), 3 for `FullViewHUD` (0x18293c), 4 for
  `DisplayFullTarget` (0x182948), 1 for `InternalFullHUD` (0x18295c), and
  0 for `None` (0x18296c). An unrecognized value retains the existing byte;
  the middle two pointers start within neighboring stored literals. The
  `Cockpit` option label at 0x182974 checks value `ON` at 0x182750, setting
  record byte +0xa0 to 1 for ON and 0 for any other token. Synthetic GCC
  cases check every layout, case-folding, repeated enum fields, unknown
  values and both cockpit states; opt-in GCC startup also reads invented
  resource text with the SHA-pinned original image. The explicit boundary
  advances to `0x310c8`, which branches on `END` and performs record
  finalization, an error callback and conditional derived geometry. These
  remain unrecovered, as do later records, original guest decimal semantics,
  packed/archive sources, guest-state differential and Watcom linkage.
  `ReadGenSeqs()` stays `named`, not `compiles`.
- GCC/Linux Views END and successive records, 2026-11-09: traced original
  `0x310c8..0x3120b`. The option dispatcher recognizes the embedded `END`
  label at `0x182754`, while an unknown option branches to the original
  three-argument diagnostic call at `0x310d8` (host probe stops beforehand).
  At `0x310f7`, record dword +0x78 must be 0 or 3, otherwise the original
  count-error callback at `0x31116` is an explicit stop. For camera byte
  +0x3c equal to 2, the original calls `direction_angles_dc91c` at
  `0x3119b` and follows with derived direction/distance work, which remains
  behind a hard geometry boundary. All other camera modes advance the source
  stack-frame record index word +0x90 after END. The `0x31201` branch skips
  non-`#` physical lines and resumes scanning; the EOF `0x3120b` file/stream
  cleanup is another explicit boundary rather than a claimed guest call.
  The EOF refill resets stream +0x558 to zero and advances +0x55c to one,
  matching recovered `read_stream_char_9d170` behavior. Synthetic GCC tests
  exercise two records separated by non-record text, the 0/3 count rule,
  unknown-option rejection, mode-two geometry stop, stream EOF state and
  original slot stride. Opt-in GCC startup again uses a SHA-pinned original
  image with invented Views text; no original resource is committed. The
  original numeric converter, mode-two geometry, stream/file cleanup,
  packed/archive lookup, guest-state differential and Watcom splice remain
  unrecovered. Keep `ReadGenSeqs()` at `named`, not `compiles`.
- GCC/Linux Views geometry and EOF resource release, 2026-11-09: original
  `0x31147..0x311f9` loads the three signed words at record +0x7d/+0x7f/+0x81
  as dwords, pairs them against the three OffSet dwords at +0x60/+0x64/+0x68,
  and calls `direction_angles_dc91c` at `0x3119b`. It copies the two angle
  words to +0x83/+0x85. Three `abs_a28f2` calls at `0x311c6`, `0x311d7`,
  `0x311e8` feed `length_3d_decc4` at `0x311ed`; its low word overwrites +0x87
  (the first Dist word) at `0x311f2`. The recovered `0xdc91c` previously
  handed native stack addresses to guest-memory helpers on GCC; it now shifts
  its three local differences without losing the original arithmetic-right-
  shift behavior, and calls `angle_of_fitted_pair_dc8d8` like the disassembly.
  That pair helper also shifts local values; `angle_of_b3b1c` dispatches
  directly to the already recovered ratio lookup at `0xb3ae0`. GCC startup
  links these derived helpers and tests axis-aligned pitch/heading values,
  a 45-degree lookup using a synthetic ratio-table entry,
  negative offsets, a 64000-unit distance requiring halving, and LookAt's
  signed-word wrap at 65536.
  On EOF, original `0x3120b` calls the stream free tail at `0x9d088` ->
  `0xc3f80` and dispatches the unflagged DOS handle through `0x9ce84` ->
  `0xa7990` at `0x31219`. A narrowly scoped host adapter releases its one
  stream allocation and invokes the existing read-only file-slot close;
  synthetic tests confirm handle slot +0x1371a4 clears. The probe stops at
  the original return instruction `0x3122a`, not inside the outer caller.
  The source-image SHA-256 guard remains mandatory and original data is not
  committed. No guest-state differential, exceptional-decimal equivalence,
  packed/archive resource handling, later TACTCOM continuation or Watcom
  hybrid execution is claimed; `ReadGenSeqs()` remains `named`.
- GCC/Linux second Views resource and caller continuation, 2026-11-09:
  original `apply_options_82cec` calls `ReadGenSeqs` at `0x30214` and then
  the separate 720-byte reader at `0x2ff44`, followed by `0xf80`. The first
  plain-file return now releases its one `0x56c` stream and closes the DOS
  slot without terminating the probe at `0x3122a`. The next reader first
  initializes twenty `0x9c`-stride slots at `0x19bf80`, copying the five
  default name bytes to offset +0x3c and the word from `0x182710` to +0.
  Its filename comes from the distinct `0x182714` source suffix
  `\\views\\seq2.dat` (not `BG\\views\\views2.dat`). It uses the same
  read-only plain-file opener and `0x9d114` token stream. After a `#` marker,
  it copies three name fields (+0/+0x1e/+0x3c), sets the ON selector at
  +0x4b, and resolves each following camera name against the forty generic
  camera names at `0x19cbb0` (stride `0xa1`). It records the matching word
  index at +0x4c, a decimal scaled by the original float 50 at +0x60, and a
  decimal scaled by the original float 32 at +0x74; END writes a `0xffff`
  index sentinel, increments the parsed-list index and resumes outer input.
  An unknown camera stops at the original error callback `0x300e6`; host
  guards stop before oversized strings, lists exceeding eight camera entries,
  malformed decimals and missing END loops. At EOF, original `0x301b8..0x30213`
  releases the stream, closes the file slot and initializes bytes at
  `0x19e5e0`, `0x12db9e`, `0x19e555`, `0x19e5e3`, `0x19e5df`, `0x1a53dc`,
  words at `0x19e543`, `0x19e5e5` (the last copied from `0x1f71ca`).
  Synthetic GCC tests cover a referenced camera, scaled values, END sentinel,
  default remaining slots and missing-camera boundary; the opt-in SHA-pinned
  original-image probe with invented plain files reaches the subsequent
  `0xf80` stop. No packed resource decoding, general x87 conversion, original
  guest-state differential, or Watcom splice is claimed. Both Views readers
  stay `named` in the tracker.
- GCC/Linux Aid.c table initialization and first two mappings, 2026-11-09:
  after the two Views readers, `apply_options_82cec` calls `0xf80`.
  Embedded DOS strings at `0x17f3d4` and `0x17f3c4` identify the function
  as `ReadAidData()` and its third, independent `\\views\\aid2.dat` resource.
  Original disassembly `0xf80..0x1119` resets 27 byte entries at `0x196fca`,
  eight at `0x196390`, fifty `0x3e`-stride records at `0x196398` (copying the
  five bytes of `NONE` from `0x17f3bc`), eight `0x33`-stride grids at
  `0x1961f8` and two `0x34`-stride grids at `0x196190`; each grid has five
  ten-byte rows initially `0xff`. Its subsequent resource opener is the
  already recovered plain-file `0x9ccb8` with callback `0xea50c` for missing
  files. At `0x1158..0x1202`, the source skips the heading of each of two
  mapping blocks and parses the second token of the next 27 and eight lines
  with guest `parse_int_9d4cd`, narrowing to byte. A synthetic fixture sets
  the guest digit-classification table and checks first/last results, all
  initialized records, open handle and bounded stop at `0x1202`. The
  opt-in original EXE test supplies invented plain aid2.dat, not original
  game data. Deeper event/association decoding, EOF cleanup, packed archives,
  guest differential and Watcom hybrid remain outstanding; tracker stays
  `named`.
- GCC/Linux Aid.c first nested association, 2026-11-09: source
  `0x1202..0x15cb` consumes two association headings, the first group and
  option headings, then decodes one `0x3e`-stride object into `0x196398`.
  The two relationship enums at offsets `+0x32/+0x33` compare the embedded
  `PARENT/CHILD/TARGET/GTARGET/THREAT/MTHREAT/WINGMAN` strings (values
  1/2/3/4/6/7/5); the second also accepts `NONE=0`. Two dword values at
  `+0x36/+0x3a` are decimals multiplied by the source's 32.0 float at
  `0x17f5b9` and truncated by x87 `0x9d72c`, with `~` denoting zero for the
  second. The word at `+0x34` instead uses the 50.0 float at `0x17f5bd`.
  The object's second-token name is copied at offset zero, and its index
  (zero) fills the first cell of the original group-1 `0x33`-stride grid
  at `0x19622c`. Synthetic tests cover signed values, both second-enum
  cases, byte/word fields and untouched surrounding grid cells. The probe
  stops **before** the source's next line read at `0x15cb`, preserving the
  open stream; mapping-only data now stop at the EOF guard `0x120e`. This
  is one record, not a complete nested loop, resource cleanup or guest
  differential. No Watcom replacement or playable hybrid is claimed.
- GCC/Linux Aid.c association loops and cleanup, 2026-11-09: completed
  the normal input control flow of `0x1202..0x1ad7` from original
  disassembly. Seven ordinary groups (indices 1..7) use `0x33`-stride
  grids; the original `Seqs` terminator advances a column, `Option` closes
  that row and begins the next, and any other token advances a group.
  Every accepted association advances the shared 50-entry object index;
  the source's five rows and ten columns per grid are bounded in the GCC
  host before they can overwrite adjacent tables. The next two `0x34`-
  stride special grids resolve a name against 27 source strings in the
  `0x1e`-stride table at `0x120d31`. Unusually, the second special name
  is read from the remaining tokens of the first group's terminating
  line. The object decoder and source numeric scales are shared between
  both copies in the original, but their exceptional callback sites are
  kept distinct. At EOF after both special grids, source `0x1ab6..0x1ad7`
  releases the stream, closes the read-only handle, calls the recovered
  `reset_selection_state_1ad8`, and returns to its caller. The synthetic
  GCC fixture traverses two `Seqs` entries, two `Option` rows, seven group
  transitions and two special names, checking object payloads, grid
  references/counts and cleanup. An opt-in test with the SHA-pinned
  original EXE and synthetic plain `aid2.dat` also reaches the subsequent
  TACTCOM boundary `0xcd4d4`; malformed special names stop at the source
  fatal callback `0x1687`. No packed archive support, real asset replay,
  guest-state differential, Watcom splice or general DOS execution is
  claimed; the single function remains `named` in the tracker.
- GCC/Linux TACTCOM keyboard initialization, 2026-11-09: disassembly of
  `0x8b477..0x8b486` calls recovered `initialize_input_services_cd4d4`
  (`0xcd4d4`), whose cold branch checks byte `0x15b72e`, programs keyboard
  LEDs via `0xcd414`, clears 128 key entries and maps default scan codes via
  recovered `0xcd430`, and writes a selector via `0xfe610`. The native probe
  now binds a strictly sequential two-byte virtual controller exchange
  (`0xed`, `0x00`, both acknowledged `0xfa`), and verifies the cold path's
  key table, selector and untouched request pool. It stops *before* the
  first `0x628` allocation because the original pre-main `0xe20` routine
  constructs 16 request slots and eight timer slots and registers X-32
  handlers, none of which the Linux bootstrap has executed. Faking those
  vector hooks would not preserve guest state or hardware effects. A
  synthetic already-initialized byte `0x15b72e=1` takes the original return
  branch and reaches the subsequent `0xce08c` call. The SHA-pinned EXE plus
  invented `aid2.dat` integration and asset-free startup tests cover both
  branches. Real IRQ/timer services, DOS setup, first screen, OpenWatcom
  splice and guest differential are still outstanding; `0x8b39c` stays
  `named` and `0xcd4d4` stays `compiles`.
- X-32 pre-main startup reconstruction, 2026-11-09: original bytes
  `0xe20..0xf43` establish the X-32 request service before game entry.
  Recovered `initialize_x32_services_e20` first sets byte `0x132a98`, calls
  existing recovered `init_far_stub_b04`, initializes indices 0..15 at
  `0x195fb0` with count 16 at `0x196144` and indices 0..7 at `0x196134`
  with count eight at `0x196140`, then makes nine `start_request_628`
  registrations. The last request uses DS as its selector and the address
  `0x120cd4` read by the original from the immutable CS:0x226 displacement
  in the verified `0x222` handler; others use CS. The first request's length
  is `0xe20-0x2b0`, and the setup ends by conditionally invoking the trap
  initializer based on environment variable `0x17f3a8`. The isolated GCC
  model checks both environment branches, registration order/arguments,
  free-list contents, seven remaining slots, and execution order of the
  far-stub prerequisite. It does not model original `0xb04` allocator,
  vector installation, protected-mode selectors or DOS/IRQ behavior;
  therefore TACTCOM's native cold path still stops before `0x628`, and no
  standalone DOS program, full game or guest differential is claimed.
- X-32 startup and keyboard request-pool composition, 2026-11-09: added a
  second host-memory test that links the actual recovered source bodies of
  `0xe20`, far-stub setup `0xb04`, request allocator `0x628`, keyboard
  initializer `0xcd4d4` and timer allocator `0x734`. Test doubles are used
  only for the external DOS/X-32 calls, selectors, keyboard subroutines and
  timer hardware. The test inspects the far-stub opcode bytes `0x9a/0xcf`,
  all eleven vector registrations and their ten-byte guest records, seven
  remaining timer free slots, the two allocated input requests (indices 6
  and 5), timer index 7 and its fourteen-byte guest record, both teardown
  registrations, and warm-init idempotence. It runs with and without the
  environment variable that suppresses trap installation. This validates
  composition of recovered C, **not** an actual extender or a DOS interrupt;
  no native startup wiring, guest differential or Watcom splice is claimed.
  UBSan also caught misaligned dword accesses in the original ten- and
  fourteen-byte packed pool structures; `0x628`, `0x6b8` and `0x734` now
  read and write these exact little-endian bytes without misaligned C lvalues.
  The test exercises `0x6b8` release, idempotent release of a cleared entry,
  and reuse of request slot 6 with the original free-list order. The composed
  test now passes with UBSan alignment checks enabled.
- GCC/Linux TACTCOM post-keyboard dispatch, 2026-11-09: extended the
  `0x8b39c` source prefix through the original `0x8b47c..0x8b4c4` control
  flow. After `0xcd4d4`, the caller discards the value read by recovered
  `get_global_ce08c`, invokes page handlers `0x7feb0` and `0x81110` only
  when their respective `0x15bac6`/`0x15bac7` flags are set, and inspects
  device byte `0x420a72`. Nonzero device IDs invoke `0x97f90` and
  `0x98000`; ID two first invokes `0x982a0`. A new independent host-memory
  test links the actual recovered source functions and verifies eight
  flag/device combinations, exact call order, and preserved guest bytes.
  The synthetic and opt-in SHA-pinned executable startup tests still pass.
  The native probe stops at the *first* unmodeled conditional handler,
  or at the original timer-service entry `0xe28e8` if all flags are clear;
  it never assumes that missing page/input/timer calls have no side effects.
  Cold X-32 request registration remains bounded at `0x628`, and startup
  after the timer call (`0x8b4c9..0x8b52d`), guest differential, and
  OpenWatcom integration remain unimplemented.
- GCC/Linux completed bounded TACTCOM warm path, 2026-11-09: reconstructed
  the original 0x8b4c4..0x8b52d tail from raw pinned-image disassembly.
  Timer function `0xe28e8` has a one-byte initialization guard at `0x15f751`:
  an unset guard still stops before hardware, while a set guard takes its
  immediate-return branch without writing memory or relinking the timer
  system. Mode-probe function `0xe9ac0` is side-effect-free for mode zero or
  >3; for modes 1, 2, 3 the bounded runtime stops at the actual nested probe
  calls `0xd9b30`, `0xd9c30`, `0x10d9e0`. The `0x8b38c` CS jump table then
  converts input modes 0/1/2/3 to word 0/0x30/0x46/0x47, with invalid
  modes reset to zero. The signed high word of `0x464470` passed to the
  existing recovered table selector `0x1d3c4` is exactly the updated mode
  word at `0x464472` (overlapping address), so the source invokes the real
  selector and the real scaler `0x1c60c`. The latter calls the already
  available deterministic *disconnected* `0xd6694` gameport model. A warm
  mode-zero path can now return from the whole 402-byte TACTCOM initializer
  and stop at original return `0x8b52d`, before the next main call. The
  expanded source-linked host test checks eight old conditional-device
  cases and six mode/table cases; GCC standalone and SHA-pinned synthetic
  original-image tests cover both the new guarded path and all hardware
  probe boundaries. Cold X-32 vector registration, first-time timer setup,
  connected input, graphical game startup, guest differential and Watcom
  splice remain outstanding; `tactcom_init` stays `named`, not `verified`.
- GCC/Linux native cold X-32 boundary, 2026-11-09: split the recovered
  `0x628`/`0x6b8`/`0x734` pool bodies into `decomp/src/x32_pool.c` so
  they can link independently of parser helpers. The new
  `--image _EF2000.EXE --probe-x32` mode invokes the actual `0xe20`
  pre-main initializer, `0xb04` far-stub setup, and `0x628` request
  allocator with a tightly bounded selector/far-allocation host model.
  It validates both free-list setups (sixteen request indices and
  eight timer indices), cleanup registration `0xad8`, far-call opcode
  bytes `9a ... cf`, and the first 10-byte vector record at slot 15:
  address `0x2b0`, synthetic CS `0x1234`, length `0xb70`, free count 15.
  The source then reaches extender `0x9c220`, whose original 29-byte
  wrapper uses DOS INT 21h AX=252Bh, EBX=501h. The host checks the packed
  record and stops there; it never returns a fake success or installs a
  vector. Normal `--probe-main` still stops at `0x628` **before** pool
  mutation, because it has not run pre-main CRT startup. A fresh memory
  load isolates the two modes in the synthetic test. This is evidence for
  actual game-owned memory writes under GCC, **not** IRQ compatibility,
  DOS/Watcom cold startup, complete vector setup or guest differential.
- GCC/Linux virtual X-32 request registration, 2026-11-09: the explicitly
  opt-in `--probe-x32-vectors` mode runs original C `0xe20`, `0xb04`
  and all nine calls through `0x628`, acknowledging the `0x9c220`
  services only in a bounded, inert host record table. For each request,
  the host checks the pinned original sequence and packed 10-byte guest
  record, with the free-list index descending from 15 to 7. The ninth
  request holds offset `0x120cd4`, synthetic DS `0x5678`, length 2,
  leaving seven free request slots and eight free timer slots. The recovered
  `0x9c858` environment check then runs and, on the absent-variable branch,
  the host halts **before** `0x9c8b4` would save a trap context and call
  DOS INT 21h AX=2533h eight times. No vectors, IRQs, or trap handlers
  are actually installed. Both the strict first-request `--probe-x32`
  boundary and the normal TACTCOM `--probe-main` boundary remain intact.
  These results test the recovered game's memory writes and request order;
  they do not validate real extender return codes or guest hardware state.
- GCC/Linux virtual X-32 trap capture, 2026-11-09: pinned 0x9c8b4
  disassembly confirms the stack callback at `[ESP+4]`, the entry ESP at
  `0x132aa0`, SS at `0x132aa4`, DS at `0x132aa6`, the temporary DS=CS,
  and eight INT 21h AX=2533h registrations: vector 3 at the `0x9c908`
  IRET handler, 6/9/10/11/12/13/14 at `0x9c909`. Added a third,
  explicitly opt-in `--probe-x32-traps` mode: recovered `0xe20` executes
  all nine original request-pool calls then its source-C `0x9c8b4`
  trap routine, while host-only hooks supply a named synthetic ESP/SS/DS
  context and collect/validate all eight trap descriptions in an inert
  table. Guest-RAM saved context is checked and execution stops at the
  original `0xe20` return at `0xf43`. There are **no actual DOS INT 21h
  calls, real interrupt handlers or verified CPU-register snapshots**.
  Earlier `--probe-x32`/`--probe-x32-vectors` boundaries are preserved.
- GCC/Linux cold X-32 keyboard prefix, 2026-11-09: `--probe-x32-input`
  resumes from the virtual-only `0xe20` return into recovered
  `initialize_input_services_cd4d4` (`0xcd4d4`), with bounded keyboard
  `0x60`/`0x64` port acknowledgements. Original recovered C resets the
  0x80-key tables, programs LED bytes 0xed and 0x00, captures synthetic
  DS `0x5678` at `0xfe624`, and uses `0x628` to allocate two further
  X-32 request records (source `0xfe61f`, CS `0x1234`, length `0xd5`,
  slot 6; source `0x41f833`, DS `0x5678`, length `0x118`, slot 5).
  `0x734` decrements the timer free count from 8 to 7, stages source 9
  in the packed 14-byte timer slot 7 and enters `0x324`, where the host
  stops **before** the original INT 21h AX=2503h timer query. The
  `0xcd554` cleanup callback and input initialized flag are not yet set.
  Synthetic memory and SHA-pinned original-image tests verify the state;
  no real hardware port, DOS timer result, IRQ or keyboard handler is
  installed. Original DOS startup, Watcom splice and replay still pending.
- GCC/Linux virtual X-32 timer completion, 2026-11-09: disassembly of
  `0x324`, `0x348` and `0x484` confirms the AX=2503h, AX=2502h and
  AX=2506h DOS calls around the original `0x734` 14-byte timer record.
  An additional opt-in `--probe-x32-timer` supplies explicit test values
  `0x2468` and `0xabcd:12345678`, checks the staged timer source/result
  fields and accepts exactly one *inert* callback registration. The real
  recovered `0xcd4d4` source then writes handle 7 at `0x41f824`, registers
  its `0xcd554` cleanup callback and sets the byte at `0x15b72e` to 1,
  reaching the original return `0xcd550`. Previous strict stops at
  `0x9c220`, `0x9c8b4`, `0xf43`, `0x324` are unchanged. Synthetic and
  original-image tests cover the new mode; no real extender response,
  interrupt/timer/IRQ, differential guest validation or Watcom splice.
- GCC/Linux X-32 to TACTCOM handoff, 2026-11-09: an additional opt-in
  `--probe-x32-tactcom` keeps the exact same recovered `0xe20`, `0xcd4d4`
  and virtual-only X-32 service path, but allows the completed input
  initializer to return before invoking the recovered TACTCOM post-input
  body `0x8b47c..0x8b52d`. With the original zero page/device flags,
  this reads the `0xce08c` global and reaches the uninitialized first-time
  timer/sound entry at `0xe28e8` instead of taking the previously tested
  warm timer-guard shortcut. Strict and timer-only probes still halt at
  their previous addresses; original-image and synthetic GCC tests cover
  the handoff and verify that no first-time timer state was set. The
  preceding TACTCOM resource/options phase is deliberately not included;
  no DOS interrupt, sound timer, real input device or full game execution.
- GCC/Linux recovered cold sound timer entry, 2026-11-09: disassembly
  confirms `0xe28e8` first tests byte `0x15f751`, writes one, and calls
  `0xd6dc4`. The latter tests `0x15bac5`, calls `0x101c54`, and only then
  marks the sound module initialized before locking/registering its
  cleanup handler. The opt-in `--probe-x32-tactcom-timer` extends the cold
  X-32/keyboard/TACTCOM probe using existing recovered source-C for both
  functions, and stops at the real `0x101c54` call before assuming sound
  initialization succeeded. Synthetic guest-memory checks verify the
  one-time guard is now one, module-ready is still zero, and the earlier
  input/timer-pool state is unchanged. Original SHA-pinned Linux GCC
  startup passes too. Other audio/lock externals are hard-fail link stubs;
  no device initialized, IRQ installed, guest differential or Watcom splice.
- GCC/Linux cold AIL trace/defaults entry, 2026-11-09: original instructions
  at `0x101c54..0x101c86` call `0x101a90`, zero trace globals
  `0x4dd624/0x4dd628`, inspect `AIL_DEBUG` at `0x1937b0`, and branch to
  recovered `reset_settings_104580` when no debug filename is present.
  Added this exact no-debug prefix as derived source-C; the alternative
  logging path remains a strict host stop at `0x101c87`. In the separately
  opt-in `--probe-x32-tactcom-ail`, both one-time lock initializers now run
  from recovered C: the narrow test adapter checks nine trace and seven
  default-setting locks with their exact start/end pointers and merely
  records them, without locking host pages. The probe confirms both init
  flags (`0x17d2cc` and `0x17d2d4`) and the cleared trace flags. The
  nineteen default values pass through the already verified recovered
  `exchange_slot_1041e4`; tests check the entire settings table at
  `0x4dd7cc`, the cleared 16 dwords at `0x4dd670` and byte `0x4dd6b0`.
  Execution stops at `0x110fd4` before its original interrupt-disable
  (`CLI`) and hardware initialization. Synthetic nonzero-to-zero sentinel checks and the pinned
  original executable pass in GCC. This is not an AIL driver or timer,
  not the debug-file branch, and not an original guest differential test.
- GCC/Linux cold AIL IRQ table prefix, 2026-11-09: pinned-image disassembly
  of `0x110fd4..0x1110c4` identifies the original saved registers/flags,
  `CLI`, saved `DS` word at `0x17d7be`, and call to the already recovered
  `register_code_ranges_111575`. A new separately opt-in
  `--probe-x32-tactcom-ail-irq` composes this prefix into the existing
  source-C cold trace/settings startup. The host adapter records the
  interrupt-disabled region but **never executes `CLI`**, and checks two
  additional precisely bounded, inert memory-lock requests (18 total).
  Source-C then sets `0x17d7b6/0x17d7ba` to zero,
  `0x17d7b2/0x17d7c0` to `0xffffffff`, and zeroes the four
  16-dword tables at `0x17d664`, `0x17d6a4`, `0x17d6e4`, `0x17d724`.
  Independent nonzero sentinels in synthetic guest RAM prove these writes
  are executed. The new strict boundary is `0x11105b`, the *first*
  `INT 21h AX=2502h` IRQ 8 vector query; no DOS service, interrupt
  handler, real memory lock, PIT or audio driver is installed. The prior
  `0x110fd4` stop and all lower-boundary opt-ins remain unchanged.
  Recovered C beyond `0x11105b`, guest differential verification and
  Watcom/x86 equivalence remain open.
- GCC/Linux AIL IRQ vector state, 2026-11-09: follow-on disassembly of
  `0x11105b..0x1110ab` identifies the original two DOS vector queries:
  `AX=2502h` returns protected-mode `ES:EBX`, `AX=2503h` returns real-mode
  `EBX`. The code saves these at `0x17d7a4` (32-bit protected offset),
  `0x17d7a8` (16-bit selector) and `0x17d7aa` (32-bit real-mode vector),
  then invokes `AX=2506h` for IRQ 8 with `CS:0x110de6`. The following
  write marks `0x17d6a0 = 2` before the `0x1021f4` callback registration
  for offset `0x3c`, function `0xd68d`. A new strict opt-in
  `--probe-x32-tactcom-ail-vectors` uses synthetic DOS read results
  (`0xabcd:0x12345678`, real-mode `0x2468`) and a synthetic `CS=0x1234`;
  it records but **does not execute** the installation request. Synthetic
  guest RAM sentinels prove all three saved-vector stores and the state
  transition from a nonzero initial marker. `0x17d6a0` aliases entry 15
  of the first 16-dword table at `0x17d664`, so this write replaces the
  cleared value with `2` while the other 63 entries remain zero. The
  strict stop moves to `0x1110ab`, before the timer
  callback is registered. The two previous strict IRQ boundaries remain
  available; no actual interrupt, PIT hardware, callback or guest x86
  differential has been exercised.
- GCC/Linux full cold AIL IRQ initializer return, 2026-11-09: the
  `--probe-x32-tactcom-ail-callback` opt-in continues from `0x1110ab`
  through the original `0x110fd4` return at `0x1110c4`. Its wrapper
  `0x1021f4` delegates to previously recovered source-C `0x1114c7`,
  the nested critical-section tails `0x101e70`/`0x101e78`, the smallest
  active-period selector `0x110f6f` and PIT scaler `0x110f3c`/`0x110f0c`.
  The single active slot (index 15, first state table offset `0x3c`)
  keeps state 2 at `0x17d6a0`, receives callback period `0xd68d` at
  `0x17d720`, and updates the minimum cached period at `0x17d7b2`.
  The scale routine maps exactly `0xd68d` to divisor 0, stored at
  `0x17d7ae`. The restricted adapter records the three original
  PIT requests `(0x43,0x36)`, `(0x40,0)` and `(0x40,0)` without sending
  any host port IO. Both nested software critical-depth increments are
  balanced by decrements, leaving `0x17d7ba=0`; the outer original
  `pushf`/`CLI` region is exited **virtually**, with no host flag change.
  The original caller then marks sound module state `0x15bac5=1` before
  stopping at the still-unimplemented real sound lock `0xd6df0`.
  Nonzero sentinels in all four timer tables and the divisor/critical
  fields prove their exact overwrites; prior opt-in boundaries remain
  unchanged. This is not a guest x86 instruction-differential run, a
  running timer callback, or a replacement for X-32/DOS hardware services.
- GCC/Linux cold AIL module locking, 2026-11-09: next opt-in
  `--probe-x32-tactcom-ail-module` continues *existing* recovered source-C
  `register_stack_ranges_d6df0` (49 bytes at `0xd6df0`), which calls
  `stack_range_104908` for exactly `[0xd7108,0xd7150)` and
  `[0x1048b4,0x104cb4)`. The original `init_module_d6dc4` then uses
  recovered `push_handler_9c3cf(0xd6da0)`, writing the first cleanup
  stack entry at `0x40d030`, count 1 at `0x40d0b0` and its original
  book-keeping pointer `0x160788=0x9c398`. Back in recovered
  `start_timer_service_e28e8`, two further `stack_range_104908` calls
  request `[0xe28c4,0xe28dc)` and `[0x15f73c,0x15f750)`. The first
  interval intentionally uses length `0xe28d8-0xe28c0` (24 bytes), as in
  the original, not `0xe28d8-0xe28c4`. A strict host adapter checks all
  four requests and does not lock physical or virtual host memory.
  The no-debug `trace_call_102144` wrapper then stops at its first
  unmodeled `0x111365` slot-allocation call, with `0x15f74c` unchanged.
  A sentinel test checks the original cleanup-stack writes and timer
  handle untouched; the previous strict stop at `0xd6df0` remains.
  This test model does not run a timer, allocate an AIL callback slot or
  execute a guest differential.
- GCC/Linux cold AIL timer slot claim, 2026-11-09: the opt-in
  `--probe-x32-tactcom-ail-slot` extends the previous sound-module probe by
  executing recovered C `claim_table_slot_111365` via the existing no-debug
  wrapper `trace_call_102144`. This call takes and releases the original
  software critical-depth field at `0x17d7ba` after the *virtual* IRQ region
  has exited. Cold slot offset zero is free: `0x17d664` changes 0 to 1,
  `0x17d624` receives `0xe28c4`, and the caller stores offset 0 at
  `0x15f74c`. Slot 15 remains state 2 with period `0xd68d`, the four
  memory locks are inert, and the three earlier PIT requests are recorded
  but never executed. Source-C wrapper `trace_call_102228` reaches the new
  strict stop `0x1114f7` with rate argument `0x32`, **before** live interval
  programming. The prior module probe still stops at `0x111365` and keeps
  its timer-handle sentinel. Synthetic-state and SHA-pinned original tests
  pass. Guest instruction differential, audio driver, live timer and Watcom
  splice remain pending.
- GCC/Linux cold AIL rate programming, 2026-11-09: opt-in
  `--probe-x32-tactcom-ail-rate` runs the already recovered C
  `set_rate_1114f7(0,0x32)` and its wrapper chain. The rate is converted
  to period 20000 (`1000000 / 50`), stored in the first slot at `0x17d6e4`;
  the cached minimum at `0x17d7b2` changes from `0xd68d` to 20000 while
  the existing slot-15 period `0xd68d` is preserved. Original nested
  software critical-section depth reaches 3 and returns to 0. Recovered
  `scale_110f3c` computes a 23866 (`0x5d3a`) PIT reload using its integer
  arithmetic. The virtual-only adapter checks three further port requests
  `(0x43,0x36)`, `(0x40,0x3a)`, `(0x40,0x5d)` with no OUT instruction
  executed (six inert requests in total). The original caller writes
  handler `0xe28e0` to `0x15b620` and reaches the next strict boundary
  `0x11141f`, just before timer slot state 1 becomes 2. Prior stop sites
  remain available. SHA-pinned original and synthetic tests run the same
  recovered host C; there is no guest x86 differential, running PIT timer,
  real IRQ/vector, audio playback or Watcom splice.
- GCC/Linux cold AIL timer-slot activation, 2026-11-09: the independent
  `--probe-x32-tactcom-ail-start` mode executes recovered source C
  `table_slot_1_to_2_11141f(0)` after the complete rate-programming chain.
  Its software critical-section bracket sees state 1 at `0x17d664`, writes
  state 2 and exits with depth zero at `0x17d7ba`. The first TACTCOM
  `start_timer_service_e28e8` call now returns to its original caller,
  which is checked and stopped at `0x8b4c9` before the later hardware
  dispatch. Guest-visible callbacks `0xe28c4`/`0xe28e0`, period 20000,
  cached reload `0x5d3a`, the earlier slot-15 state 2, inert module locks
  and cleanup registration, and all six virtual-only PIT requests persist.
  This exercises **no real PIT OUT, IRQ delivery, timer tick, or audio**;
  the original guest x86 differential and DOS/Watcom integration remain
  untested. The pre-start `--probe-x32-tactcom-ail-rate` boundary remains.
- GCC/Linux cold TACTCOM mode-zero return, 2026-11-09: new opt-in
  `--probe-x32-tactcom-ail-finish` runs the same recovered cold X-32 input,
  AIL initialization, six inert PIT requests, slot allocation, rate update,
  and slot activation before proceeding beyond TACTCOM `0x8b4c9`.
  For original hardware mode zero (no device selected), the bounded
  `0xe9ac0` branch is inert as recovered from the game. The TACTCOM
  mode remapping, recovered `select_tables_1d3c4(0)` and
  `scale_words_1c60c` (synthetic disconnected gameport) execute, then
  stop at the actual TACTCOM return `0x8b52d`. Both selector pointers
  `0x19840c`/`0x198410` are cleared; the mode word remains zero, and
  timer slot zero remains state 2 with period 20000 and reload `0x5d3a`.
  The probe still refuses active hardware modes 1..3 at their original
  call targets, and does not simulate any port OUT or IRQ delivery. As in
  the prior cold continuation, this starts after the options/input phase;
  it does not make the full game startup or an audio driver playable.
  SHA-pinned original/synthetic tests cover the recovered source-C path;
  guest x86 differential and Watcom splice remain pending.
- GCC/Linux synthetic warm game-main exit-handler entry, 2026-11-09:
  `--probe-main-exit` reuses the full source-C `candidate_game_main(1,0)`
  sequence with read-only test-only DOS resources, preinitialized input/timer
  flags and disconnected gameport mode zero. The recovered TACTCOM function
  returns to its actual caller for this opt-in mode; the next original main
  call invokes source-C `install_exit_handler_ea470(1)`. Its first channel
  switch `guest_fn_10dc7f(2,0x10dc40)` remains a strict stop at `0x10dc7f`.
  Sentinel tests assert no mode-change global (`0x17e5e0`) or channel-2
  slot (`0x17d3d4`) mutation, and the selected-hardware mode-1 route cannot
  reach this stop. Ordinary `--probe-main` retains its earlier boundary.
  This is a synthetic warm-state integration test, not a real cold-to-main
  run: no channel state transition, exit registration, following clump
  resource loads, guest instruction differential or Watcom splice is claimed.
- GCC/Linux synthetic warm channel-mode prefix, 2026-11-09:
  New `--probe-main-channel` advances the full read-only, synthetic-warm
  `candidate_game_main` path through already recovered source-C
  `set_channel_mode_10dc7f(2,0x10dc40)` from module `game_helpers75`.
  The mode-change global `0x17e5e0` becomes `0x10dc41`, and channel 2
  takes its original branch into the nested `0x11d24e` tail, where a strict
  boundary protects the not-yet-linked runtime hook. The previous
  `--probe-main-exit` still stops at `0x10dc7f` before that global changes.
  The channel-2 slot at `0x17d3d4` remains untouched, with a seeded
  sentinel; active hardware input mode 1 cannot enter this continuation.
  Original channel-2 hook, register-level guest differential, Watcom splice,
  exit registration and following resource loads remain unverified.
- GCC/Linux flake startup-link correction, 2026-11-09: the flake previously
  linked standalone `runtime/decomp_startup.c` with the full recovered-source
  `libef2000-decomp.a`. Object extraction pulled `guest_callptr1.o` and
  `guest_io.o`, whose Watcom/DOS thunk definitions collided with the host
  probe's guarded versions, producing multiple-definition link errors.
  `runtime/decomp_host_sources.txt` now records the 50 recovered C modules
  used by the existing successful GCC standalone test. Both Nix's native
  executable link and the test consume that same list; the full archive
  remains available separately for inspection. Nix's direct startup C test
  also now includes `game_helpers75.c`, required for the channel-mode probe.
  Local GCC compiled the selected modules as separate objects, linked them
  with the host adapters, and ran `--self-test` successfully. Nix is absent
  from this environment, so the actual `nix build .#ef2000-decomp` remains
  unverified here. Warnings elsewhere in the archive still indicate missing
  64-bit callback and Watcom-only function-thunk ports; they were not hidden.
- Stream B tranche 1, 2026-11-10: recovered one unknown leaf, `0xdc5a9` (158 bytes, `pair_word_dc5a9`, module `unk_leaves1`), to `compiles`. Caller-stack ABI (`ret 0xc`, arguments X, Y, DST at entry `[esp+4..12]`), confirmed from three direct callers (`3bf44`, `d86e0`, `d8fd8`) and the wdis listing of the compiled OMF. The `#pragma aux ... parm routine []` line is a draft, not yet a hybrid splice. Skipped in this tranche: `0x10d63b` (LSL selector intrinsic, one caller), `0x407c4` (branch at +0x1f leaves the body), `0x32c04` (tail jump into a block Ghidra removed as unreachable), `0xc5e02` (indirect jump), `0xf023f` and `0xefdae` (soft-float 80-bit helpers), and most other unknown leaves with one or zero direct callers, which cannot satisfy the two-caller `confirmed` rule. `ef2000-wcc` needed `NIXPKGS_ALLOW_UNFREE=1 nix develop --impure` in this environment.
- Stream A, 2026-11-11: hybrid M2 reached. Bisects 009-054 demoted ~110 single-redirect culprits and a policy exclusion removed 313 unreviewed mechanical bodies; a static comparison of each recovered function's `ret` immediate with the original then found 62 functions with wrong stack cleanup (59 caller-cleanup wrappers compiled as `ret N`; `select_page_e3c6c`, `reset_clip_bounds_c58cc`, `nop_9d774` with arguments in registers instead of on the stack). Fixed with `#pragma aux ... parm caller [eax] [edx] [ebx] [ecx]` / `parm routine []` in `ef_abi.h`. Main-menu and quick-fight-setup replays pass bit-exactly with 1,707 redirects; the cockpit replay renders the cockpit but differs from the original in game time (instruction-counted timing). Still unlinked: mixed register/stack functions `0x2d894`, `0x106b0`, `0x55b44` and the ST0-argument math helpers (`cos`, `sin`, `tan`, `atan`, `atan2`, `truncate`), whose C versions take stack doubles.
- Coverage of the main-menu replay, 2026-11-11 (hybrid with 1,708 redirects): the replay reaches the main menu with a byte-identical framebuffer and guest-hash to the original at 500 M instructions. Hooking every non-redirected tracker entry (`--trace-patch-entry`, incremental rounds: hook, drop what was seen, double the budget; hooks make the run ~300x slower, so all hooks at once never finishes) shows 137 original functions still execute up to ~20 M instructions and none new up to 40 M: x386/Watcom startup, DOS/X-32 bridges, C runtime (fopen/scanf/heap/handles/sscanf), timers, AIL init, parser/loader (`0xe882c`, `0xea270`). The cause for most (~110) is not missing C: `hybrid_link` drops whole modules on one unresolved external (`guest_ail_trace_debug_path_101c87_` 50 functions, Watcom 64-bit helpers `__I8M`/`__U8M`/`__U8LS` 22, `guest_fatal_arg_` 9, `guest_report_parser_enum_error_` 6, `in_`, `ROUND_`, `SegmentLimit_`), 12 have no hand-written public definition, 5 are mechanical objects without a public definition. Resolving those externals (aliases to original addresses or C definitions) is the next step toward "no replacements"; the DOS-extender/startup/C-runtime part belongs to the standalone native build (Stream E). `tools/asm_match.py` (constants, offsets, calls, ret per function) is a heuristic fingerprint check: ~50 % of functions differ, mostly compiler idioms, so it is only a triage aid.

## 2026-11-11 AIL trace initializer and tool fix

- `initialize_ail_trace_101c54` now contains the AIL_DEBUG branch in C (stream
  setup, header, 100 Hz timer slot, "AIL_startup()"); `call_f9dcb` (0xf9ed2)
  returns the formatted-time pointer. This removes the unresolved
  `guest_ail_trace_debug_path_101c87` external that blocked `misc_drafts1`.
- `abi_check.py`/`asm_match.py` matched only 4-digit listing offsets, so every
  function beyond 64 KB into its module was silently skipped. With 4-8 digit
  offsets `abi_check.py` reports 39 `ret N` mismatches that were hidden before
  (listed by running the tool); they must be fixed or excluded before those
  modules are linked.

## 2026-11-11 Watcom runtime helpers, ABI pragmas, mechanical exclusions

- `abi_check.py` (now covering the whole module) found 39 `ret N` mismatches; 25
  are fixed with `#pragma aux ... parm caller [eax] [edx] [ebx] [ecx]` (stack
  arguments caller-cleaned in the original), 14 stack/x87 conventions that
  need a proper declaration are listed in `hybrid-exclude.tsv`.
- New `decomp/src/watcom_rt_helpers.c`: `__U8LS/__U8RS/__I8RS/__U8M/__I8M/
  __U8D/__I8D/__CHP`, the OpenWatcom 1.9 runtime calls that the original
  (older compiler) inlined. This lets 81 more modules link
  (601 -> 682 of 794).
- With those modules linked, 100 unreviewed mechanical `ok` bodies became
  eligible; two of them (0x120746, 0xf9ee0) broke the main menu. All mechanical
  entries are now excluded until hand-verified. Result: 1,872 hand-written
  redirects, main-menu and quick-fight-setup replays pass frame-identical.

## 2026-11-11 (later): cockpit ABI bugs, ABI gates, state of the hybrid

- Cockpit replay (`--match frame`) culprits found and fixed by comparing against
  the original asm, not by guessing: `guest_fn_a4470`/`guest_fn_b45fa` take
  their double in ST0 (the C pushed it on the stack; `#pragma aux ... parm
  [8087] value [8087]`), the fixed-point matrix products take ESI/EBX/EDI (not
  EAX/EDX/EBX), and `dot3_fixed` must keep bits 24..55 of the 64-bit sum
  (original `shld edx,eax,8`), not `>> 8`.
- `tools/abi_check.py` gained `--stack-clobbers` (original preserves registers
  of a stack-cleaning function that the C clobbers; Watcom cannot save EAX) and
  `--inputs` (C and original read different entry registers; 59 functions, now
  excluded until their declaration is fixed).
- Open: the cockpit frame still differs in ~20 % of pixels with 1,850
  redirects and no guest fault; unresolved, likely mostly timing (the
  instruction-counted clock shifts by ~2 s), but not proven. The excluded
  x87 helpers of `float_helpers5` (0xb415c-0xb4364) are unmatched: the C stores
  double intermediates, the original keeps extended precision.
- Main-menu frame replays at 300M, 400M and 500M guest instructions pass
  pixel-identical with 1,850 redirects (100M has no VGA frame; at 200M the
  hybrid is a few frames off a video-mode transition, a timing artefact).
- The `asm_match.py` fingerprint diffs are mostly noise (constant folding,
  helper factoring such as `fade_step`); of 52 diffs on functions the main
  menu executes, 6 sampled were equivalent. Not exhaustively triaged.

## 2026-11-11 (evening): unlinked modules, static-first verification

- Root cause of many "never exercised" functions: `misc_drafts1` was left out
  of the hybrid link because `initialize_ail_irq_tables_110fd4` calls
  host-boundary externals (`guest_ail_irq_*`) that the hybrid linker cannot
  resolve. The function now lives in `ail_irq_init.c`; the link went from
  1,850 to ~2,850 redirected functions (718 of 794 modules). Earlier replay
  PASS results did not cover those functions.
- New Watcom runtime helpers in `watcom_rt_helpers.c`: `__I8LS`, `memset`,
  `memcpy`, inline-8087 stubs (`IF@DSQRT`, sin/cos/log/tan/atan/fmod/atan2),
  `ceil/floor/trunc`, and register-to-original-convention thunks for the
  guest fatal/parser-error/callback entries. `guest_read_candidate_record_`
  (7 modules) and `guest_invoke_runtime_slot_` (3 modules) stay unlinked until
  their conventions are verified against the original.
- Direction from the owner: verify by static comparison of the recovered C
  against the original asm over all functions at once, not by replay
  bisecting. Tools: `abi_check.py` (`--inputs`, `--clobbers`,
  `--stack-clobbers`, `--x87`), `asm_match.py`, and `func_differential.py`
  run over every redirected function (it now accepts arena-based hybrid
  patches). Over 2,902 candidates: 1,164 pass, 310 real diffs (226 are only
  non-result registers, kept), 1,428 harness errors (missing stack-argument
  specs, probe faults); the 84 with EAX/RAM diffs are excluded in
  `hybrid-exclude.tsv` pending triage, as are 16 static-gate failures and 12
  replay-bisect culprits (e.g. variadic `sscanf_df26f`).
- First triage result: `step_generator_a45b3` returned void, but callers
  (`half_word_9c960`, `random_modulo`, ...) use the new low word in AX; it now
  returns it. Remaining triage list: the other EAX diffs, then RAM diffs.
- Standalone host build: it was already failing at HEAD (misc_drafts1 pulled
  in functions whose callees are guest code); `decomp_startup.c` now defines
  fatal-on-call stubs for those nine symbols. `nix build .#ef2000-hybrid
  --override-input ef2000 path:<EF2000 dir>` builds again with all 411 tests.

## 2026-11-12: stale-object fix, randomised differential fuzzing

- The 84 "EAX/RAM" differential failures were an artefact of linking stale
  objects (`local/hybrid/obj`, Nov 6) instead of the current Watcom objects;
  with fresh objects only `any_device_e1260` differed (it tested the full
  EAX of `find_text_in_buffer_e0b10` instead of AL; fixed). The 84
  exclusions were removed.
- New `tools/func_fuzz.py`: seeded random register/stack/memory scenarios,
  original vs patched, for functions with no hand-written spec. Over the
  1,428 former harness errors: 130 PASS, 91 soft (preserved-register only),
  879 weak (original faults on every random input: DOS/interrupt/x87-style
  code), 307 FAIL.
- `tools/hybrid_link.py --only-address` links a single redirect so a failure
  can be attributed to that function and not to a patched callee. In
  isolation 253+ functions still differ; they are excluded in
  `hybrid-exclude.tsv` ("isolated run") pending per-function triage. Weak
  functions remain unverified.

### 2026-11-12 (later): isolated fuzz rerun correction

The first isolated rerun after the exclusions were committed was vacuous:
`hybrid_link.py --only-address X` still honours the exclude list, and X itself
was now excluded, so zero functions were patched and everything passed.
`isofuzz.py` must use an exclude list without the `func_fuzz.py isolated`
entries. With that fixed, and with `func_fuzz.py` ignoring ESP-cleanup
differences where original and patched agree (a wrong signature, not a C
defect), the 264 isolated failures split into 215 FAIL, 35 SOFT, 12 PASS and
2 weak. The 12 PASS functions were re-included. `enter_mode_1a358` was fixed
(flag stored before the call, register arguments passed through) and still
needs a recompile and fuzz run.

## 2026-11-12 — external-call `modify` clauses

`#pragma aux` externals without `modify` made Watcom assume the guest callee
preserves ECX/EDX/EBX etc. (e.g. `draw_prop_text_15a14` kept its loop index in
ECX across `cf7c4` and looped forever). All 33 guest-external pragmas lacking
`modify` now declare `modify [eax ebx ecx edx esi edi ebp]`; 27 pure-stack
`guest_fn_*_x` prototypes also gained `parm routine/caller []`. Isolated fuzz
of the 214 prior failures: 4 PASS (0x3bcc0, 0x5d48c, 0xa3424, 0xc1e3c,
re-included), 18 SOFT, 192 FAIL (mostly patched faults / RAM diffs, other
causes; still excluded).

25 signatures whose original ends in `ret N` (double/stack args) had
`stack_bytes` 0, so the register-saving trampoline wrapped them and the
isolated fuzz returned to address 0. `stack_bytes` now matches the original
`ret N`; the trampoline is skipped for these. 11 now PASS and are re-included
(0x4fdf4, 0x52eb0, 0x72f68, 0xa56b0, 0xe86f7, 0x11b540, 0x11b588, 0x11b8d8,
0x11b950, 0x11bf00, 0x11bfc8); 14 stay excluded with their remaining fuzz
diffs (e.g. EAX returning a stack address at 0x11b040/0x11b3c8/0x11bb80).

## 2026-11-13 — x87 ST0 arguments and results in the fuzz harness

`guest_fn_{fa362,dcb96,e7df4,deb1a}_x` (ST0 -> ST0) had no pragma, so Watcom
pushed the double on the stack; they now use `parm [8087] value [8087]`
(fixes 0x88ff8/0xe9dcc callers). `func_differential.py` treats a `:double`
signature output as an x87 ST0 result and compares it (stored through a
`fstp qword [esp-8]` stub at the sentinel) instead of an incidental EAX.
Isolated fuzz of the 181 remaining failures: 6 PASS re-included (0xe9dcc,
0xe9f84, 0x11b644, 0x11bbdc, 0x11bf78, 0x120a60), 169 FAIL, 4 SOFT, 2 weak.

Pragma sweep: 35 prototypes whose original ends in `ret 4*nargs` had no
`#pragma aux` (stack arguments passed in registers by Watcom): 21 external
guest calls got `parm routine [] modify [...]`, 14 locally declared ones
`parm routine []` (e.g. `set_view_origin_dc21c`, which broke 0x76620).
Isolated fuzz: 0x76620, 0xc9ecc, 0xc9f28 now PASS and are re-included;
165 FAIL, 5 SOFT, 2 weak remain.

Return widths: 119 external `guest_*` prototypes documented as returning in
AL/AX now return `uint8_t`/`uint16_t` instead of `uint32_t`, so Watcom no
longer tests stale upper EAX bits (0x90c0 now PASS and re-included).
`guest_fn_2dba4` stays `uint32_t` because 0x17474 returns EAX with the
callee's upper bits; `update_unit_90c0` casts explicitly. A full-link fuzz
against a baseline build showed no other regressions (123 pre-existing
FAILs unchanged).

## 2026-11-13 — Unknown functions: self-contained integer leaves

Triage of the 591 `unknown` game functions: 172 draft mechanically and
compile (kept excluded as unreviewed), 290 have only recovered callees and no
indirect calls, and 119 of those are self-contained integer code (no x87, no
segment registers, no branches leaving the body). The rest are fragments that
jump into other functions, x87/soft-float helpers, DOS bridges and rasteriser
inner loops that are hand-written assembly.

`0xb3168` (`dump_lists_to_file_b3168`, 9 arguments, `ret 0x14`) is recovered by
hand from the disassembly. `report_message_b3374` returned nothing although the
original returns the duplicated message buffer in EAX; it now returns it. The
fuzz harness cannot exercise either function (file I/O: 0 completed cases), so
the check is static: the compiled body matches the original's call sequence,
stack offsets, push order and `ret 0x14`.

`0xd1afc` (`write_planar_image_d1afc`, new module `image_dump1`) is recovered
by hand: it takes its two row buffers from the stack with the original's bare
`sub esp, eax` (an inline-asm `stack_take` pragma, because Watcom's `alloca`
would add `stackavail`/`__GRO` externals that the hybrid link cannot resolve).
The fuzz harness completes 0 cases (log-file I/O); the compiled body matches
the original's frame, checks and call sequence.

Goal (user, 2026-11-13): every function is either decompiled or skipped as too
difficult. Functions skipped as too hard are marked `blocked` in the tracker
with the reason in the notes column.

`0xbfb88` (`position_in_box_bfb88`, EAX/EDX/EBX in, 0/1 out) recovered; EBX is
an unpushed hidden input forwarded to `0xbfb2c`. Fuzz with 1,500 cases: PASS
(6 completed; random pointers make most scenarios fault in the original, use
`--cases 1000+` for pointer-heavy functions).

Skip policy: 28 unknown functions whose branches leave their own body (shared
tails / Ghidra boundary splits) are marked `blocked` with that reason. Their
neighbours must be recovered together; revisit after the plain functions.
`0x10d94f` (`align_block_size_10d94f`, new module `memory_blocks1`) recovered;
it calls the segment-limit helper `0x10d63b` as an external guest function
(`guest_fn_10d63b`, value in EAX). Fuzz: PASS, 456 completed cases.

- 2026-11-13: recovered `loop_objects_c1034` (0xc1034, `game_helpers130.c`): loop over `0xbc5d0` count calling `0xcc51c` (kind 0x19) and `0xc1210` (4 register + 6 stack args, ret 0x18, declared as `guest_fn_c1210`). Compiled code matches the original instruction for instruction in structure. Isolated fuzz: 76 completed, 2 FAIL where the patched run faults inside `0xbc5d0` (reached via `0xc1210`); not yet attributed, but `0xcc51c` is already known to differ in the EDX residue (SOFT), which may be the cause. Recorded as `compiles`, not verified.

- 2026-11-13: per the owner's goal (decompile or skip as too difficult), 368 remaining `unknown` game functions classified by objdump of the payload were marked `blocked`: segment/DOS/port code (149 or so), x87-only code (167, deferred rather than impossible), and indirect-call dispatchers (52). Plain integer functions (about 190) remain.
- 2026-11-13: blocked remaining unknown functions at >=0xed000 (runtime/soft-float/startup region).
- 2026-11-13: blocked 42 unknown functions with implicit register inputs or carry-flag results.
- 2026-11-13: blocked 53 more unknown functions that branch on callee flags, use carry-chain arithmetic, indirect jumps or segment registers.
- 2026-11-13: recovered `draw_segment_records_418c4` (0x418c4); isolated fuzz PASS 800 cases.
- 2026-11-13: recovered `scan_nodes_bfa58` (0xbfa58, calls 0xbe59c with 11 stack args and 0xbdbf8 as guest functions). Compiled pushes match the original. Isolated fuzz: 52 completed, 3 patched-only faults; its callees 0xfc0ec/0xb16c0 were never verified (weak), so the divergence is probably theirs but is not attributed. State `compiles`.
- 2026-11-13: recovered `dump_state_b3084` (0xb3084); call sequence and ret 0x1c match the original; isolated fuzz weak (file I/O, 0 completed cases).
- 2026-11-13: recovered `parse_angle_line_db32c` (0xdb32c); builds; isolated fuzz weak (reads the global text cursor, the original faults on random pointers), no instruction-level comparison done.
- 2026-11-13: recovered `pick_target_at_4e204` (0x4e204); isolated fuzz PASS 1000 cases.
- 2026-11-13: recovered `ray_hits_plane_cfde4` (0xcfde4); isolated fuzz PASS 380 completed cases. 0xc3ae8 blocked (VGA port output, indirect calls).
- 2026-11-13: recovered `init_nodes_from_grids_caa28` (0xcaa28); isolated fuzz PASS 60 completed cases (940 skipped).
- 2026-11-13: recovered `select_next_target_65a88` (0x65a88); isolated fuzz PASS 1000 cases.

- 0x14d58 `draw_framed_box_14d58`: framed box from 19-byte record table (fuzz PASS 1000).

- 0xeb520 `lookup_name_entry_eb520`: name normalisation + hash bsearch (fuzz PASS 15 completed).

- 0xb9c68 `build_object_records_b9c68`: object record builder; fuzz weak (0 completed, global pointers), logic decoded from disassembly.

- 0x9a6fc `classify_target_flags_9a6fc`: flag-group counters over target list (fuzz PASS 769).

- 0x6c1c `update_damage_flags_6c1c`: damage-bit randomiser (fuzz PASS 1000).

- 0x31b40 `update_view_angles_31b40`: view-angle results from camera state (fuzz PASS 1000).

- 0x324b0 `aim_angles_to_point_324b0`: direction angles with clamp windows (fuzz PASS 381).

- 0x3287c `apply_camera_keys_3287c`: key-driven camera zoom/angle steps with limit windows (fuzz PASS 1000).

- 0xa3dc `dump_selected_group_a3dc`: text report of selected group; fuzz weak (stream/list globals), logic decoded from disassembly.

- 0x82e40 `rewrite_status_file_82e40`: status text file rewrite; fuzz weak (file I/O), call sequence decoded from disassembly.

- 0xb3648 `dump_node_list_b3648`: node list text dump; fuzz weak (stream printf), argument order decoded from the push sequence.

- 0x78f3c `draw_unit_info_78f3c`: unit info panel text (fuzz PASS 1000).

- 0x275e0 `draw_stat_row_275e0`: statistics table row draw (fuzz PASS 696).

### 2026-11-13 update_scene_camera_9457c (0x9457c)

Recovered the scene camera update (derive view angles from the navigation component, zoom keys, camera offset, scene metric fallback, angle/pair/input-triplet pushes). Fuzz reports only guest RAM 0x159064 differing (original 0x80, patched 0x6c): that word is the renderer's diagnostic saved ESP, written by a callee, so it reflects this function's stack frame depth (20 bytes difference), not behavior. All other RAM and registers matched in the completed cases.

### 2026-11-13 collect_list_entries_c193c (0xc193c)

Recovered from disassembly: allocates the id/record tables (COLLECTION+0x78/+0x69), walks the node list and claims nodes matching the two keys, copying owner-block fields into 22-byte records; returns the count in EAX/EBX. Fuzz is weak (0 completed cases: random list pointers always fault the original), so the result is verified by disassembly only.

### 2026-11-13 find_nearest_object_33b4 (0x33b4)

Recovered: picks a target object for group record AX (0xca-byte records at 0x19bb58), publishing 0x197061/0x19705f/0x197083; first unflagged member, then nearest to the reference block at +0xb5, then (type 2) nearest to its own target. Quirk kept: when the first scan runs out, the last looked-up object stays selected. Fuzz PASS 612 completed cases.

### 2026-11-13 draw_button_11e7c (0x11e7c)

Recovered: draws button AX (29-byte records at 0x121fb4) as a masked frame blit plus a centred label (colour 0xe/2/1 by state and label flag; label is the shared string 0x12288c or the record's pointer). Fuzz PASS 1000 completed cases.

### 2026-11-13 draw_text_box_1f994 (0x1f994)

Recovered the scrolling text box step (one character per call, wrapping, tab/`>` stops, scroll-up when the cursor passes the bottom). Quirk: when the record has no text slot the original draws into the caller's EDI, so the function takes EDI as a second register input (`#pragma aux ... parm [eax] [edi]`; a `modify exact` list made the build fault).

### 2026-11-13 record_track_sample_2e494 (0x2e494)

Recovered the flight-track sampler: appends the tracked group's scaled position to the 10-byte sample table at 0x1a3650 unless it is within one unit of the previous sample (then only the trailing words are refreshed). Fuzz PASS 1000 completed cases. 0x21654 (1967 bytes) was blocked in the same pass: it is x87 floating-point code.

### 2026-11-13 draw_debug_overlay_58714 (0x58714)

Recovered from disassembly: debug overlay printing the selected object's header lines and one line per non-empty row of the 80x8-word table at 0x1a5df0. Fuzz is weak (0 completed cases: the original faults on every random scenario), so this is verified by disassembly only.

- 2026-11-13: recovered `odometer_text_5e21c` (0x5e21c). Fuzz PASS (19 completed cases of 1000). The original's packed stack locals (stale mode dword overwritten by x in the minus branch) are mirrored by semantic locals.

- 2026-11-13: recovered `update_screen_palette_76370` (0x76370). Fuzz weak (0 of 1000 completed; callees fault on random state), so verified from the disassembly only.

- 2026-11-13: recovered `select_target_object_92668` (0x92668). Fuzz PASS (1000/1000).

- 2026-11-13: recovered `update_scene_camera_node_92984` (0x92984, fuzz PASS 1000), `hit_test_pointer_acbe4` (0xacbe4, fuzz PASS on 766 completed cases), `draw_progress_panel_98658` (0x98658, fuzz weak: 0 completed, checked against the disassembly only) and `dump_record_b1f80` (0xb1f80, fuzz weak: 0 completed, checked against the disassembly only).

- 2026-11-13: recovered `reserve_requirements_bd92c` (0xbd92c). Fuzz PASS on 245 completed cases.

- 2026-11-13 `collect_group_nodes_be7e4` (0xbe7e4): group/node collection helper recovered; isolated fuzz PASS (387 completed of 1000).

- 2026-11-13 `build_group_request_ccdbc` (0xccdbc): request builder recovered from disassembly; isolated fuzz weak (0 completed of 1000, random inputs fault in the original).

- 2026-11-13 `nearest_segment_hit_cff80` (0xcff80): segment-vs-plane nearest hit recovered; isolated fuzz PASS (148 completed of 1000).

- 2026-11-13 `wrap_text_lines_e1e6c` (0xe1e6c): text word-wrap into line nodes recovered with goto-structured C; isolated fuzz PASS 1000 (many cases likely exit early on the font/enable checks, so deep paths are covered mainly by disassembly review).

- 2026-11-13 `clip_against_windows_e52a4` (0xe52a4): window overlap / dirty-rect clipping recovered; isolated fuzz PASS 1000, but the initial-image rectangle is empty so nearly all cases take the early "return 2" exit; the overlap and border paths are verified from disassembly only.

- 2026-11-13 `select_view_angles_91afc` (0x91afc): reopened from the "unrecovered callees" block (all callees already had ABI wrappers; added `guest_fn_d86e0_x`, `guest_fn_dc519_x`). Isolated fuzz: only difference is guest RAM 0x159064, the renderer's saved-ESP slot written by the 0xc4105/c4167 callees (stack depth of the compiled frame differs); no other RAM/register difference in 3 completed cases (weak sample).

- 2026-11-13: recovered `centre_view_sample_39744` (0x39744) from disassembly; differential fuzz weak (0 of 1000 cases complete, original faults on random state), so verified by disassembly review only.

- 2026-11-13: recovered `replay_elapsed_steps_c630` (0xc630) from disassembly; added `guest_fn_d68c_x` wrapper. Fuzz weak (0 of 1000 cases complete); disassembly-checked only.

- 2026-11-13: recovered `draw_outline_segments_3d7b0` (0x3d7b0); fuzz PASS 222 completed. Clip results live in a local buffer mirroring the original unaligned stack frame.

- 2026-11-13: recovered `draw_group_markers_43df4` (0x43df4). Fuzz: 98 of 100 completed cases agree; the 2 FAILs (cases 754, 822) are a harness artifact: the random list pointer 0x300132a lands inside the patched image's hand-object arena, whose bytes differ from the unpatched image's zeros (traced: next-node read hits code bytes, then faults). Callee conventions from disassembly (cc22c/cc1ec/cc1ac, ca620, 440f4).

- 2026-11-13: recovered `draw_group_markers2_43f48` (0x43f48); fuzz PASS 96 completed (marker-drawing paths mostly unreached; disassembly-checked).

- 2026-11-13: recovered `draw_group_markers3_441fc` (0x441fc). 0xca620/0x440f4 clobber ECX and the count-icon call at 0x4d24c consumes that leftover, so they are called through ECX-returning wrappers `guest_fn_ca620_e`/`guest_fn_440f4_e` (pragma `value [ecx]`). Fuzz: 85 of 86 completed cases agree; the single FAIL (case 32) is the harness artifact where the random list pointer 0x3006c42 lies in the patched image's hand-object arena.

- 2026-11-13: recovered `process_pending_nodes_bf81c` (0xbf81c, 9 params, ret 0x14, be59c pushes mapped from disassembly). Fuzz: 86 of 88 completed cases agree; FAILs (329, 539) are the arena artifact: a list node at 0x300000c, inside the patched image's hand-object arena (zeros in the original image), traced.

- 2026-11-13: recovered `assign_slot_flags_b0df4` (0xb0df4; slot pick via the 0x9c960 RNG, state flags of matching list entries; new `guest_fn_cb3c4_x` wrapper). Fuzz: 91 of 93 completed cases agree; FAILs (129, 188) are the arena artifact: the list walk in 0xbc5e8 passes through 0x300132a, inside the patched image's appended region (zeros in the original image). Traced: identical calls and RNG outputs up to that read.

- 2026-11-13: recovered `draw_group_markers4_43b14` (0x43b14; type-0x16 variant of the group-marker drawers). Fuzz: 86 completed cases, one FAIL (747) = the arena artifact: a node pointer lands at 0x300132e in the patched image's appended region (zeros in the original), traced by logging reads in that range before the fault.

- 2026-11-13: recovered `process_group_nodes_c0624` (0xc0624, 6 stack args, ret 0x18, list is the third). Callees 0xfbad8, 0xbdd64 (its `jmp *%cs:table` switch is a flat-memory read, not a real segment dependency) and 0xbe59c are called through wrappers (`guest_fn_fbad8_x`, `guest_fn_bdd64_x`); the 0xfbad8 selector byte is an untouched stack slot for types other than 0xe/0x16 and is modelled as 0. Fuzz PASS 53 completed cases (deep path with a hit not necessarily reached).

- 2026-11-13: recovered `release_object_74584` (0x74584; teardown of an indexed object, 0x10750 called through the existing `guest_fn_10750_x` wrapper, which preserves ECX as the original relies on). Fuzz PASS 1000 completed cases, but random object tables mostly end at the first lookup, so the teardown body is checked by disassembly rather than by the fuzz.

- 2026-11-13: recovered `draw_tile_window_7ac74` (0x7ac74; EAX/EDX/ECX + one stack argument, so the C prototype carries an unused EBX slot to get the Watcom default register assignment right). The random fuzz never completes (window loops exhaust the budget), so it was checked with a controlled differential: 40 scenarios with a real tile-map layout, 0x9e0c4 stubbed to `ret` and its register arguments logged in both images: all 40 agree, 435 draw calls compared (scratchpad script, not committed).
