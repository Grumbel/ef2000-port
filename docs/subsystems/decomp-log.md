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
