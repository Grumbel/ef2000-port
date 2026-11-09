<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: LicenseRef-EF2000-Derived -->

# Recovered EF2000 source

This directory holds game source code recovered by reverse engineering
EF2000, as described in [the decompilation plan](../docs/decompilation-plan.md).
It is **not** GPL. Everything here, including this README, is under the
[EF2000-Derived Source Notice](../LICENSES/LicenseRef-EF2000-Derived.txt):
the recovered logic is derived from the original program, whose rights
remain with its holders. Contributors' own additions may be reused under
that notice's terms.

The GPL host code (`runtime/`, `tools/`, and later the platform layer)
carries a GPL section 7 linking exception
([LICENSES/GPL-3.0-linking-exception.txt](../LICENSES/GPL-3.0-linking-exception.txt)),
so builds combining both parts can be distributed.

Rules:

- Never add original game data, executable bytes, or Ghidra pseudo-C here.
- Every file needs a `LicenseRef-EF2000-Derived` SPDX header (or is covered
  by the `decomp/**` annotation in `REUSE.toml`).
- Do not move recovered game logic out of this directory into GPL code.

Contents so far:

- `symbols/functions.tsv`: every original function (address, size, name,
  module, recovery state). Maintain with `tools/decomp_status.py`
  (`summary`, `next`, `check`, `seed`).
- `symbols/globals.tsv`: named global data with evidence.
- `symbols/signatures.tsv`: calling conventions confirmed from original code
  and at least two distinct callers (which does not imply C verification).

These files hold facts about the original program, never its code bytes.

Layout:

- `include/`: fixed-width guest types, ABI declarations and recovered layouts.
- `src/`: recovered C, grouped by documented original module when known.
- `compat/`: reproductions of original library behavior where required.
- `tests/`: per-function differential test specifications.
- `build/`: ignored OpenWatcom object files and other generated outputs.

There is no standalone **playable game** build yet. The flake now produces
two executable bootstraps from `standalone/main.c` and recovered
`src/hiscore.c`: `.#ef2000-decomp` (native GCC/Linux) and
`.#ef2000-decomp-watcom` (OpenWatcom/CauseWay DOS, launched via DOSBox-X).
The Linux package also compiles 342 modules into a GCC archive;
`game_helpers125.c`, `runtime_helpers23.c`, and `runtime_helpers42.c` still
contain Watcom-specific or non-portable C. The DOS package retains all OMF
objects for hybrid splicing. The bootstrap uses synthetic high-score data;
it cannot enter a playable game. The game entry's initial arena call has now
been recovered, but TACTCOM, external function bindings, global initialization
and platform services are not yet reconstructed for standalone use.
`nix run .#ef2000-decomp -- --image /path/to/_EF2000.EXE` now validates the
known original executable and maps its initialized X-32 payload and zeroed BSS
into host-owned, bounded 64 MiB guest memory. No original instructions run;
the first-screen startup call graph still needs recovery. The standalone Linux
tests prove that recovered C stream and 8x6 glyph functions can use the
translated guest offsets with synthetic data. This does not make the whole
archive safe to invoke: remaining 32-bit callback pointers and guest function
bindings must be adapted for a 64-bit Linux process.
The optional `--probe-arena` attempts the first recovered game-owned startup
call, `init_arena_e29a0`, and its `reset_buffer_e2a1c` callee, with a bounded
host allocator and the minimum required Watcom/X-32 service bindings. A
synthetic execution test verifies the game globals, 0xe2a50 cleanup hook and
zeroed arena. This is a startup **probe**, not a verified reproduction of the
original executable's entire C-runtime startup; its execution against real
game data passed with the SHA-pinned German executable on 2026-11-09.
`--probe-main` runs the recovered 100-byte game entry at `0x8b274`
(including the native case-insensitive `0x9d524` comparison), invokes the
recovered arena setup, and enters the **partial** TACTCOM `0x8b39c` prefix.
It follows the recovered `0x880` DOS service wrapper via a native, synthetic
X-32 350Bh available-memory response, calls the recovered comma formatter,
prints the memory diagnostic from the original image and flushes the output
stream. The prefix then sets two logging callback globals, uses the recovered
`0x9c980` default-directory path builder and `0x9c858` DOS environment
lookup, appends the game's first resource pathname and dispatches through
recovered `0x9cb20` to the recovered `0xb3b90` resource-size reader. On Linux,
`--probe-main` now maps DOS paths into a **read-only** data directory, by
default the original executable's directory or the explicit `--data-dir DIR`.
The reader recognizes JR and RA header lengths and falls back to seeking to
the actual file end. The original file-open/read/seek/close paths are bound
to five host-owned handles; no data is written or added to the repository.
For the SHA-pinned original executable without a `check.cd`, the normal branch
now copies the `0xea50c` missing-resource message, emits it to host stderr as
UTF-8 (instead of writing the original log file), executes the recovered
`0xeb448` dispatcher, and stops before the `0xeb25a` page switch. With a
synthetic `check.cd` it executes recovered `0x82cec` options initialization,
including the language handler/path selection. A synthetic disconnected
DOS gameport returns `0xff` from port `0x201` during `0xd66c2`, as the
original's no-device branch would. The probe now executes recovered
`load_text_file_e94ec` for `EF2000.CFG`, using source-recovered `0x9cf2c`,
`0x9cf74` and `0x9cfa4` language-path callbacks. The host reads uncompressed
  text only, under the same read-only DOS path rules as the earlier size reader;
  compressed files stop at `0xb3c04` pending decompression. If the config is
  present, it is allocated in guest arena RAM and processed by recovered
  `parse_script_text_e93dc`, including line counting and the source-recovered
  semicolon branch of `0xe882c`. A narrow type-3 scalar route decodes signed
  decimal options with the original descriptor widths (1/2/4 bytes) and
  performs the original `CONTROL` callback (`0x82710`). The type-5 scratch
  text route supports `MUSIC`, `SOUNDFX`, `JOYSTICK`, `SPEECH` and `VRHELMET`
  through their original callbacks. The joystick handling follows the
  probe's disconnected-gameport branch (`0xd6960`, `0xd66c2`); connected
  joystick sampling, malformed values and diagnostic branches remain
  bounded. Missing, comment-only or entirely supported
  configs (including the unchanged shipped `EF2000.CFG`) run through
  original `0xe9574` arena release and into the recovered prefix of
  `ReadGenSeqs()` at `0x30214` (Views.c). The bounded source version parses
  ordinary `BG\\views\\views2.dat` text records, including camera modes,
  derived geometry, and resource cleanup, then continues through the next
  original call at `0x2ff44`. That second Views reader loads the separate
  `\\views\\seq2.dat` list of named cameras and scaled values, resolves
  references against the preceding forty records, and initializes the
  corresponding twenty-slot list table. With both synthetic plain files
  present, it next enters `ReadAidData()` at `0xf80` (Aid.c). That loader
  resets the 27+8 byte mapping arrays, 50 packed `0x3e`-byte objects, eight
  `0x33`-byte grids and two `0x34`-byte grids. It opens the independent
  `\\views\\aid2.dat` and processes 27+8 two-token integer mappings through
  the original guest integer parser. The bounded GCC reader now follows
  all seven ordinary association grids and both special-name grids, using
  the source's `Seqs`/`Option` transitions, 50-object record table and
  27-name lookup. Successful plain-text input releases the stream, closes
  the file and calls `reset_selection_state_1ad8()`, returning through the
  recovered options caller and then executes the already recovered keyboard
  initializer at `0xcd4d4`. The GCC probe supplies a bounded 0xed/zero
  keyboard-LED acknowledgement model, runs the original key-table reset
  (`0xcd430`) and selector write (`0xfe610`), then stops at `0x628`, **before**
  the first X-32 request-pool mutation. The original pre-main initializer
  at `0xe20` is now recovered separately as source in `x32_startup.c`,
  including its nine vector registrations, request/timer free-list setup,
  selector-dependent final vector and optional trap-handler registration.
  It has a synthetic host-memory test plus an isolated composed test that
  executes recovered C for `0xe20`, `0xb04`, `0x628`, `0xcd4d4`, and `0x734`
  with test-only DOS/X-32 call doubles. That test verifies eleven requests,
  one timer, the far-stub bytes, and two cleanup callbacks after the cold
  input path, plus request release/reuse through recovered `0x6b8`. Packed
  request and timer dwords use explicit little-endian byte accesses so the
  same guest layout is valid under the native C alignment rules. It is
  **wired into a separate opt-in native cold-start probe**:
  `--image _EF2000.EXE --probe-x32` executes the recovered pre-main
  `0xe20`, far-stub `0xb04` and request allocator `0x628` in order.
  The host supplies explicitly synthetic selectors and one six-byte
  allocation, validates the initialized free lists and first packed vector
  record, and stops at real extender call `0x9c220` *after* its game-owned
  writes, **without installing any vector**. An explicitly virtual
  `--probe-x32-vectors` mode acknowledges these requests in a private
  host-only registry and executes all nine recovered `0x628` calls,
  checking their order and packed records, then stops at `0x9c8b4`
  before DOS trap installation. The final request in slot 7 holds address
  `0x120cd4`, synthetic DS selector `0x5678`, and length 2; the free
  count is seven. An additional opt-in `--probe-x32-traps` mode executes
  recovered `0x9c8b4` with an explicitly synthetic ESP/SS/DS snapshot.
  It validates all eight AX=2533h vector/handler descriptions in an inert
  registry (3 → `0x9c908`; 6, 9–14 → `0x9c909`), saves the context
  fields in guest RAM, and stops at the `0xe20` return (`0xf43`). A fourth
  opt-in `--probe-x32-input` mode continues through recovered keyboard
  LED initialization (`0xcd414`/`0xcd3c0`), key-table setup (`0xcd430`),
  data selector capture (`0xfe610`), two more request allocations (`0x628`),
  and the first timer pool write (`0x734`). The request free count is then
  five, timer free count seven; the timer source 9 is staged in slot 7.
  It halts at `0x324` before the first actual timer service
  `INT 21h AX=2503h` and leaves the input initialized flag unset.
  A separate `--probe-x32-timer` mode supplies test-only `0x2468` and
  `0xabcd:12345678` returns for original `0x324`/`0x348`, validates the
  packed 14-byte timer slot and accepts exactly one inert registration
  corresponding to original `0x484` (AX=2506h). Recovered `0xcd4d4`
  then installs its second cleanup callback in the virtual table, sets
  the input flag and reaches the original return `0xcd550`. These values
  are deliberately synthetic, not measurements of the DOS extender.
  `--probe-x32-tactcom` separately continues *that same cold source-C state*
  through the original TACTCOM `0x8b47c` post-input dispatch, validating
  the zero-page-flag/no-device route before stopping at the uninitialized
  `0xe28e8` timer/sound service. Unlike the previously supported warm path,
  it does not preseed the timer guard, and it does not claim to exercise
  TACTCOM's preceding file/options phase or a complete game main startup.
  Another opt-in mode, `--probe-x32-tactcom-timer`, enters the real recovered
  `start_timer_service_e28e8` and `init_module_d6dc4` C bodies from that
  same cold state. It confirms the original one-time guard write
  (`0x15f751 = 1`) and stops at the first unimplemented external sound
  initialization call `0x101c54`, before the module-ready flag at
  `0x15bac5` is set. Later audio callbacks and memory-lock services remain
  fail-closed, not synthetic successes. A separate opt-in
  `--probe-x32-tactcom-ail` mode then enters the **cold no-debug branch**
  of recovered AIL trace initializer `0x101c54`. The original `AIL_DEBUG`
  environment variable is absent in this probe; the debugger/logfile route
  remains a strict unsupported branch at `0x101c87`. Original recovered
  `0x101a90`, `0x103d80`, `0x104580` and `0x104908` issue sixteen exactly
  checked inert code/data memory-lock requests, clear the AIL trace flags,
  and pass all nineteen default values through previously verified
  `exchange_slot_1041e4`. The next strict stop is `0x110fd4`, whose
  original instructions begin interrupt-sensitive initialization (`CLI`).
  No DOS interrupt or sound device is initialized by this probe.
  The further opt-in `--probe-x32-tactcom-ail-irq` runs the recovered
  source-C prefix of `initialize_ail_irq_tables_110fd4` under an inert,
  strictly checked *virtual* interrupt-disabled region. It saves the synthetic
  X-32 data selector `0x5678` at `0x17d7be`, registers the two original
  code/data locks through recovered `0x111575` (eighteen locks total), resets
  four AIL state dwords and zeroes four sixteen-dword tables at
  `0x17d664`, `0x17d6a4`, `0x17d6e4` and `0x17d724`. It stops at
  `0x11105b`, immediately **before** `INT 21h AX=2502h` queries the IRQ 8
  vector. No real `CLI`, interrupt-vector operation, PIT programming or
  IRQ handler installation happens. The remainder of this routine is
  accessible only through the later opt-in callback mode.
  A final independent `--probe-x32-tactcom-ail-vectors` mode continues that
  same original C flow through the IRQ 8 protected-mode (`AX=2502h`) and
  real-mode (`AX=2503h`) vector reads. Both return explicitly **synthetic**
  values; the recovered C saves the protected offset/selector and real-mode
  offset into `0x17d7a4`, `0x17d7a8`, `0x17d7aa` respectively, makes one inert
  IRQ 8 vector installation request (`AX=2506h`, original handler `0x110de6`),
  and writes state `2` at `0x17d6a0`. It stops at `0x1110ab` before the
  `trace_call_1021f4` timer callback registration (`slot offset 0x3c`, callback
  `0xd68d`). Note that `0x17d6a0` is also **slot 15** in the first
  16-dword table previously cleared at `0x17d664`, so that slot is now `2`
  while the other 63 table entries remain zero. Neither a real vector nor
  a timer callback is installed by this mode.
  The further independent `--probe-x32-tactcom-ail-callback` mode executes
  the already recovered 0x1021f4/0x1114c7/0x110f6f/0x110f3c/0x110f0c
  source-C timer chain: slot 15's value at `0x17d720` becomes `0xd68d`,
  the cached minimum at `0x17d7b2` becomes `0xd68d`, all active-time counters
  are zero, and the nested critical depth at `0x17d7ba` returns to zero.
  This value selects divisor zero (the standard 65536-count PIT reload).
  The three `OUT` requests (`0x43:0x36`, `0x40:0`, `0x40:0`) are checked and
  **recorded**, never executed on the host. After virtual interrupt-flag
  restoration at `0x1110b3..0x1110c4`, the original `0x110fd4` function
  returns naturally. The original module then marks `0x15bac5=1` and stops
  at its next unimplemented sound lock (`0xd6df0`). This does **not**
  register a working callback or audio driver. The earlier stop modes retain
  their stricter bounds; guest differential tests remain pending.
  An independent `--probe-x32-tactcom-ail-module` mode continues through
  the already recovered source-C `register_stack_ranges_d6df0` and the
  original module's `push_handler_9c3cf` cleanup registration, followed by
  two more code/data locks in `start_timer_service_e28e8`. All four lock
  requests are validated against the exact original exclusive-end ranges
  and remain **inert**. The cleanup handler `0xd6da0` occupies the first
  entry of the guest handler stack (`0x40d030`), count 1 at `0x40d0b0`.
  Execution stops in the no-debug AIL wrapper at `0x111365`, before the
  timer slot for `0xe28c4` is allocated; there is no working host timer.
  The further independent `--probe-x32-tactcom-ail-slot` mode runs recovered
  source-C `claim_table_slot_111365` after the virtual IRQ initializer has
  returned. The first free table entry at `0x17d664` changes from 0 to
  state 1, its paired callback at `0x17d624` becomes `0xe28c4`, and the
  original handle at `0x15f74c` becomes offset 0. The existing IRQ slot
  15 stays at state 2 with period `0xd68d`. This mode stops at `0x1114f7`
  before programming the new interval; it does not start a host timer.
  This is a test model, **not** live IRQ support or an
  emulation of `INT 21h`. The normal `--probe-main`
  intentionally still stops at `0x628` before mutating its uninitialized
  pool; these modes do not borrow each other's state. Actual DOS/X-32
  request/vector/timer services cannot be safely inferred from free-list
  initialization alone. A synthetic pre-initialized input-state test skips
  that setup as in the original, then executes the recovered TACTCOM
  `0x8b47c..0x8b52d` control flow: the `0xce08c` global read and the
  conditional page/input handlers. With no handler flags, it reaches the
  `0xe28e8` timer-service boundary; enabled handlers stop at their actual
  call targets rather than silently skipping their side effects. When the
  timer has already been initialized, its original guard skips the hardware
  setup. The following `0xe9ac0` dispatch stops at the original hardware
  probe for modes 1–3; mode 0 or an invalid mode does not probe. The
  recovered jump-table selection maps modes 1/2/3 to `0x30`/`0x46`/`0x47`,
  clears other modes, selects the corresponding tables via the existing
  `0x1d3c4` C body, and executes the existing `0x1c60c` input scaler using
  the explicitly disconnected-gameport model. This warm, unconnected-mode
  path reaches the original TACTCOM return at `0x8b52d`.
  A mapping-only file stops at the missing association heading `0x120e`;
  missing files stop at `0xea50c` after the original table initialization.
  Main's *next* call after TACTCOM remains a hard boundary. This warm-path
  result does not imply that first-time timer registration, connected
  gameport input or subsequent graphical startup is operational.
  Packed resources, malformed/exceptional numbers and unrestricted strings
  remain bounded. Neither reader is yet guest-differential verified.
These are **bounded probes**: they do not implement the general variadic
logger or connected-joystick input. The memory-shortage
branch stops at `0x8b417`. The remainder of the 402-byte TACTCOM entry and
actual first-screen services are **not yet linked**. The alternate `playvideo`
branch is exercised with synthetic guest argv. This is neither a native
reproduction of the real DOS memory report nor Watcom/guest replay verified.

Use `.#ef2000-hybrid` to combine OpenWatcom OMF replacements with the
original executable through the native64 harness until the source is complete.
The original-code runtime remains the validation oracle. See
`LICENSE-NOTE.md` before distributing any recovered source.

Status (2026-11-09): 4,277 Ghidra-inventoried functions plus 13 manually
confirmed additional entry points (4,290 tracked); 3,216 marked `compiles`,
251 `drafted`, 11 `spliced`, 7 `blocked`, 9 `compiles-bad`, and 20 `verified`.
The verified OpenWatcom OMF replacements are
`to_lower_ascii`, `to_upper_ascii`, `encode_runtime_segment_offset`,
`read_runtime_slot_flag`, `initialize_runtime_probe_latch`, `init_bounds_d0b68`,
`sync_flag_12ec8`, `reset_unit_counter`, `set_runtime_phase_five`,
`store_runtime_control_word`, `initialize_mode_limits`,
`copy_active_buffer_pointer`, `set_control_mode_a`, `set_control_mode_b`,
`set_control_mode_c`, `resolve_link_pointer`, `refresh_link_pointer`,
`exchange_slot_1041e4`, `strip_cr_inplace` and `find_last_char`.
Their isolated differentials and deterministic guest replays pass. The encoder uses two flag-neutral nops to
retain the original instruction count; its only isolated memory difference
is the 8-byte callee stack scratch below incoming ESP. Replay determinism
requires the synthetic VGA retrace clock. See
[batch 110](../docs/subsystems/decomp-batch-110.md),
[batch 111](../docs/subsystems/decomp-batch-111.md) and
[batch 112](../docs/subsystems/decomp-batch-112.md), plus
[batch 121](../docs/subsystems/decomp-batch-121.md) and
[batch 122](../docs/subsystems/decomp-batch-122.md),
[batch 123](../docs/subsystems/decomp-batch-123.md) and
[batch 124](../docs/subsystems/decomp-batch-124.md) for the latest verified
replacements; batch [125](../docs/subsystems/decomp-batch-125.md) verifies the
first control-mode gate, [batch 126](../docs/subsystems/decomp-batch-126.md)
verifies the paired mode B/C setters, and [batch 127](../docs/subsystems/decomp-batch-127.md)
verifies the signed link-pointer lookup and its unaligned candidate override;
[batch 128](../docs/subsystems/decomp-batch-128.md) verifies its record updater.
M0 mechanically compiles 418/1,020 available unrecovered Ghidra exports
(41.0%, 127,079/543,917 body bytes = 23.4%) with OpenWatcom v2; 589 exports
remain `manual` and 13 have compile errors. Hand-written recovered C covers
364,031 body bytes (40.0%); together with mechanical `ok` exports, potential
hybrid coverage is 491,110/908,947 tracked body bytes (54.0%). A focused
154-function image passes repeated-baseline main-menu and quick-fight-setup
replays. The broad image was relinked with 2,453 functions (2,043
hand-written, 410 mechanical), 12,265 entry bytes, a 587,348-byte arena and
599,613 total patch bytes, with 2,287 exclusions across 704/766 modules. Its
500M main-menu replay still faults at guest IP `0x190` on tick 561 with an
unmapped read at `0x358be389`; M1/M2 remain unpassed. See the latest replay
and compiler evidence in
[`decomp-log.md`](../docs/subsystems/decomp-log.md).
See also
[the quantitative status and remaining-work report](../docs/decompilation-status-2026-11-05.md),
as well as [renderer stream findings](../docs/subsystems/renderer-streams.md) (batch 082)
and [renderer/driver batch 083](../docs/subsystems/decomp-batch-083.md) for
boundary repairs, and [mission callback batch 084](../docs/subsystems/decomp-batch-084.md)
for its twenty named GUI entries, validation and pending guest work.
See also [campaign and weapons-panel batch 085](../docs/subsystems/decomp-batch-085.md),
which adds twenty more named callbacks with host-level tests only; and
[multiplayer/network callbacks batch 086](../docs/subsystems/decomp-batch-086.md),
which reconstructs twenty network configuration and status handlers, and
[environment parameter batch 087](../docs/subsystems/decomp-batch-087.md),
which covers fifteen damage/fade line parsers and five mode flag handlers;
and [environment and GUI batch 088](../docs/subsystems/decomp-batch-088.md),
which covers eight additional environment handlers and twelve GUI callbacks.
The [63-entry GUI dispatch and mission batch 089](../docs/subsystems/decomp-batch-089.md)
separately reports 48 five-byte jump aliases, two small quit wrappers, and
13 substantive callback/state bodies; jump aliases must not be mistaken
for completed native game logic.
The [READCOLS environment batch 090](../docs/subsystems/decomp-batch-090.md)
adds twenty nontrivial line parsers and global-state handlers as host drafts,
with explicit guest scanf and engine-call boundaries. They require guest
differential replay and OpenWatcom ABI verification before replacement.
The [palette, crater, and sky batch 091](../docs/subsystems/decomp-batch-091.md)
adds twelve more nontrivial READCOLS host models, including two record layouts,
linked texture allocation, signed bounds, and the two-stage SETSUN scanner;
guest ABI and engine operations are still unresolved.
The [READCOLS tail batch 092](../docs/subsystems/decomp-batch-092.md)
adds palette dispatch, cockpit palette selection, lightning-point allocation,
and the two smoke record rings; those four remain host-only drafts, with
specific guest call boundaries and a documented unresolved flare parser.
The [high-score C batch 093](../docs/subsystems/decomp-batch-093.md) adds five
host-tested C bodies for existing high-score routines, checked against the
previously guest-tested C++ model; they remain unspliced drafts.
The [GUI script parser batch 094](../docs/subsystems/decomp-batch-094.md)
adds seven source-grounded host C handlers for layout properties, string
commands, resource paths and palette extrema; tokenizer and graphics-driver
guest boundaries still require differential replay.
The [GUI script control batch 095](../docs/subsystems/decomp-batch-095.md)
adds a native guest-RAM lexer, the six-number FROM/SIZE/HOTSPOT reader and
the 14-command OPTIONS dispatcher. Its remaining helpers use host adapters.
The [OPTIONS label-table continuation](../docs/subsystems/decomp-batch-095-labels.md)
adds two more source-grounded host models for option assignment lookup,
replacement, deletion and allocation.
The [OPTIONS source reader batch 096](../docs/subsystems/decomp-batch-096.md)
connects the dispatcher to the original line and token routines and models
the indirect formatting wrapper, with pinned-byte audits for both entries.
The [OPTIONS display batch 097](../docs/subsystems/decomp-batch-097.md)
models the original cursor repeat timers, pointer selection loop and
fade/no-fade cleanup; rendering, hit testing and device inputs remain adapters.

Batch 113 adds `copy_record9_d0858` as a spliced, unverified OpenWatcom candidate: function differential passes, but the deterministic cockpit replay has a selected BSS hash mismatch. `latch_values_ce328` is blocked because its current C lowering exceeds the original extent. See [batch 113](../docs/subsystems/decomp-batch-113.md).
Batch 114 corrects the `group_flag_negative_cc1ac` ABI record: original disassembly masks EDX, so it is not preserved. Its 72-byte OpenWatcom C lowering cannot fit the 61-byte body; see [batch 114](../docs/subsystems/decomp-batch-114.md).
Batch 115 verifies `init_bounds_d0b68`, an exact 63-byte OpenWatcom replacement with 12 isolated cases, three captured caller states and a passing cockpit replay (155 hits). See [batch 115](../docs/subsystems/decomp-batch-115.md).
Batch 116 adds `find_node_by_byte_b` as a spliced OpenWatcom candidate. Its eight isolated cases pass, but available deterministic replay paths did not reach it; see [batch 116](../docs/subsystems/decomp-batch-116.md).

Batch 118 verifies `sync_flag_12ec8` from a live indirect callback and blocks its parent `reinitialize_runtime_slots` on callback ABI and placement evidence; see [batch 118](../docs/subsystems/decomp-batch-118.md).
Batch 119 records an OpenWatcom `max3_signed` candidate that passes 253 isolated cases and the repeated menu/setup/cockpit replay when paired with the current fixture-derived encoder patch. The candidate gets no entry hit, so it remains spliced. It also blocks the overlapping x87 entries `0xa4470`/`0xa4472`; see [batch 119](../docs/subsystems/decomp-batch-119.md).
