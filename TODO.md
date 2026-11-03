# EF2000 Linux port handoff

## Current state

Read `README.md`, then `docs/startup-runtime.md` and `docs/x32-services.md`.
The long-term decompilation/modernization roadmap is
`docs/decompilation-plan.md`; follow its ground rules and operating loop.
The current tip is reported by `git log -1`; this repository began without any
commits. Original game binaries/data stay outside Git and the Nix package.

The native x86-64 Linux harness executes original 32-bit instructions through
startup, runtime initializers, and game initialization. It reads the game archives
and configuration, drives timer handlers, and renders the original German
main menu in a 640×400 VBE mode and flight in 320×200 VGA. An SDL window
supplies mouse, keyboard, and joystick input. Keyboard view changes and held
flight controls, plus SDL virtual-joystick banking and button delivery, were
verified under Xvfb. Audio is disabled in a local config copy by default; opt-in `--sound sb16`
plays original digital sound through SDL and captures PCM. The user reports
working sound in native flight sessions.
Broader gameplay and mission completion remain unvalidated. The target executable and
its SHA-256 are recorded in `docs/executable.md`.

Implemented and verified:

- Two-stage X-32 extraction and both static segment-relocation passes.
- Native compatibility-mode execution with 64-bit Linux signal handlers.
- The first X-32 selector-limit request and DOS 5.0 version query.
- Non-DPMI stack reservation and heap growth, with reference state/flag tests.
- DOS time query with measured register preservation and logged host UTC samples.
- Real/protected vector tables, exception registration, dynamic LDT descriptors,
  Linux page residency, and conventional DOS allocation in a 64 MiB image model.
- DOS file open/device-info/read/write/seek/close, with a local copy-on-write
  drive overlay preserving original game assets/configuration.
- Keyboard make/break scan codes through IRQ1 and ports 60h/64h, controller
  acknowledgements, extended keys/modifiers, and focus-loss key release.
- SDL joystick selection, four mapped/invertible axes, four buttons, disconnect
  handling, and automatic keyboard fallback; deterministic game-port discharge.
- CLI/STI and PIT programming, host-timer IRQ0 delivery, and BIOS tick advancement.
- Wall-clock pacing for the original minimum 50 Hz physics step, and measured
  X-32 divide-exception delivery to the original renderer recovery handler.
- Bounded Unicorn execution of original 16-bit driver code; unknown ports/INTs stop.
- VGA DAC and banked VBE video, original framebuffer export, SDL window, and mouse.
- Native AC-fault recovery for the CPU probe; flag reflection remains approximate.
- Original PSP/environment copying and clearing of deliberately seeded BSS.
- Hardware expand-down protection. QEMU reaches the same startup boundary but
  did not enforce the tested lower bound; retain it for comparison only.
- A pinned Linux flake that precompiles both harness backends without game data;
  the installed launcher only assembles/links the supplied payload locally.

## Next work

1. Validate longer flight sessions, mission completion, and physical joystick
   hardware; keep automated UI tests on private Xvfb displays.
2. Revalidate the physical Xbox 360 Wireless Controller after fixing the
   sound-enabled joystick regression: inherited `JOY_FACTOR=0` caused original
   sound initialization to zero joystick calibration. Joystick selection now
   substitutes the executable default of 100 in the writable config, preserving
   nonzero custom scales. Original-code calibration and private-Xvfb SB16
   virtual-joystick flight now pass. Keep pause/resume coverage on private Xvfb.
3. Extend native checkpoints to SB16 audio configurations. Currently save/load
   requires native64 with `--sound off`; the save menu explicitly marks New Save
   unavailable otherwise. Verify save/load and fresh-process restore after changes.
4. Investigate FM music separately if needed. Digital SB16 discovery, shared PIC
   masks, 8237 DMA, real-mode IRQ completion, PCM capture, and SDL playback are
   implemented; see `docs/sound-research.md`.
5. Implement other exception-frame delivery and refine CLI/STI/AC flag reflection,
   timer coalescing/priority, memory residency reference counts, and DOS path handling.
   Keep unknown interfaces and model exhaustion as explicit stops.
6. Document data/mission discoveries separately under `docs/`, with fingerprints,
   offsets, reproducers, and observed/inferred status.
   The supplied `MISSION.CFG` is already a text file with 746 mission records;
   `NAVIG.NAV` has 1,118 navigation records. Defer a read-only typed
   mission/navigation parser and archive/campaign correlation until the
   current decompilation priorities are complete; do **not** begin a mission
   editor or write-back workflow yet. See
   `docs/existing-tools-and-releases.md`.
7. Validate **portable Unicorn flight controls and longer gameplay**. The
   flake exposes `nix run .#unicorn` as a separate experimental windowed
   launcher with standard game-data discovery and a writable state overlay;
   it is **not** the native default or a real-time-playability claim.
   `.#unicorn-probe` remains the bounded headless diagnostic interface. The
   opt-in `--fast-execution` batches guest instructions and watches IRQ return
   addresses; an 820M original-asset run reaches the cockpit in 38.3 seconds
   with 8,090 IRQ0 deliveries. Its intra-batch timing is approximate; compare
   gameplay/checksums against exact instruction-hook replays before enabling
   it by default.
   original 640×400 main menu, Quick Combat selection, Rookie difficulty,
   mission 01, and Accept all work through original GUI scripts. The previous
   asset-decompressor `INT 3` failure was fixed by moving DOS RAM, PSP and
   environment outside the protected image. A deterministic **1.02-billion-
   instruction** original-asset replay passes **604 checks**, switches from
   VBE `0x100` to VGA `0x13` at instruction `778005467`, and renders the
   **original 320×200 cockpit, terrain, HUD and changing flight instruments**
   through the final 1.02B checkpoint. Use `--framebuffer-sample` for bounded
   screenshots and `--progress-instructions` for live stderr status. A separate
   F4 make/break replay at ticks 8300/8305 produces two original keyboard IRQ1s
   and switches to the original external aircraft view by 860M instructions,
   with flight still active at 950M. PIT-timed **headless joystick scripts**
   now initialize the same game-port mailbox and virtual configuration as
   native joystick input; synthetic OUT/IN 201h tests verify deflection and
   buttons. An original-asset scripted X-axis replay reaches 950M with all
   604 LZ checks intact, but the flight frames do not establish a visible
   roll response **when only two axes are advertised**. A 900M follow-up
   captured **120,120 original IN 201h reads**,
   **30 OUT 201h writes**, and eight charge cycles sampling the full-X axis
   count `127`. A matched **four-axis** centered/deflected pair now verifies
   **original flight roll response**: identical 820M pre-input frames,
   divergent 850M frames, and a clear bank at 920M under sustained X
   deflection, with all 9,089 IRQ0 deliveries matched between runs. Next,
   validate **pitch/throttle effects and two-axis compatibility**, longer
   flight, and exit/pause handling, refine
   instruction-paced timers and VGA retrace pacing, and validate the Windows
   C++ Unicorn launcher on Windows or Wine. SDL3 window behavior is currently checked with an injected
   display; real GUI tests must run in a private Xvfb instance.

### Deferred decompilation and data-format work

- [ ] **Return to full native GUI interpretation:** extend the current
  `gui/*.gui` window/icon/directive extraction into a tested interpreter for
  layout, lifecycle, input hit-testing, state, screen transitions and named
  callbacks. Locate the real menu fonts, cursor/button art and palette use;
  reconcile the scripts with Ghidra call targets and live Unicorn traces.
  This is deliberately parked for now; see `docs/gui-assets.md`.
- [ ] **EF2000-style movable-window overlay:** inventory/rip the original
  menu fonts, frames, icons, backgrounds, buttons, cursor and palette from
  locally supplied DID/assets (do not commit copyrighted art). Build a
  reusable skin/texture atlas for the host-side SDL3 windows. The initial
  asset-free window manager now supports **drag, resize, focus/Z-order,
  close/reopen and scroll**, with F12 controls and diagnostics panels;
  optional caller-cropped BMP frame skins can be applied as nine-slice
  textures with `--overlay-frame-bmp`. The `gui-assets` extractor now has an
  offline `--gallery` view, and full-screen English/German original option
  images provide concrete gray/silver/olive frame references. The procedural
  host overlay follows those observed colors/title tabs; `newgui03` is only
  a textured background. **Most original GUI text is baked into its bitmap
  art**; preserve that distinction when interpreting the `.gui` scripts.
  The `gui-assets font` extractor now recovers **90 ASCII glyphs including
  digits and lowercase** from the original `DID1ST` font specimen
  `hash_33773001` (the four characters `{|}~` and accented-glyph mappings
  remain unresolved). **Reverse-engineer the procedural `box`/`bkd`
  renderer and the underlying glyph storage/metrics**, then map remaining
  glyphs and verify against original screenshots. Finer widgets and gamepad
  window navigation remain open tasks. The same specimen also yields 14
  verified light/dark directional and window-like symbols via
  `gui-assets symbols`; optionally use its dark cross in the SDL3 close
  button with `--overlay-symbols-bmp`. **Trace these symbols back to their
  original GUI use before treating them as genuine menu button art.** The
  other 48 visible accented glyphs now have a reproducible numbered PNG/HTML
  gallery and coordinate JSON from `gui-assets font`; **match the German
  accented glyphs to actual Unicode/guest character codes** before use.
  Keep the native UI separate from guest framebuffer emulation so menus can
  coexist with the cockpit and eventually replace original hardcoded GUI
  callbacks. Compare extracted textures and layout with `docs/gui-assets.md`;
  preserve the original aesthetic without binding new layout to DOS pixels.
- [ ] **Validate physical gamepad flight:** confirm third-axis throttle and
  fourth-axis rudder in the original aircraft (including calibration,
  reversal, flight response and auto-centering); tune the new latched
  throttle and rudder dead zone with a real controller. The SDL-independent
  mapping tests prove host behavior only. Evaluate keyboard fallbacks if the
  guest flight code ignores the auxiliary gameport axes.
- [ ] **Mission/navigation read-only reconstruction:** parse loose text
  `MISSION.CFG`, `NAVIG.NAV`, `CAMPAIGN.CG` and briefing records with explicit
  typed fields, cross-references and count checks. Compare generated missions
  with the built-in TACTCOM editor and historical Strategic Commander output.
  Extract/export for analysis before considering a full editor or modding UI.
  The **syntactic inventory, block export, route/count validation and 20-pylon
  flight relationship** are now recovered in `tools/mission_records.py` and
  `docs/mission-records.md`. Remaining: field units, enum meanings, cross-file
  references and briefing parsing. Original **reader** entry points,
  their child calls, the lexical helpers and waypoint-count/last-index
  validation are now identified in `docs/mission-readers.md`; next recover
  typed structs, enum *usage* and callsite-to-filename mapping. A read-only
  pinned-payload probe now extracts the reader's 313 table labels and eight
  mobile-class aliases in `tools/mission_enum_probe.py`. A first native C++
  guest layout model now captures mission/group/base, navigation, target
  members, pilot records, and nested attack-pattern/route/node structures,
  along with waypoint height normalization and navigation coordinate clamping
  (`runtime/ef2000_mission_layout.*`, `docs/mission-layout.md`). Still to
  reconstruct: pointer lifetimes, the meaning and usage of remaining pilot/
  attack fields, and measured runtime comparisons before replacing guest code.
- [ ] **Version-aware reference/decompilation:** fingerprint the candidate
  `rdeprera/EF2000` v2.0 DOS executable separately; verify its actual
  language, patch level and code/data differences from the German reference.
  Do not apply the current executable's fixed offsets or hash to it.
- [ ] **GFX+ as a 3D acceleration reference:** acquire a properly licensed
  TACTCOM/Evolution 3Dfx GFX+ executable, compare it in a separate Ghidra
  database and trace its static Glide / low-level Voodoo interface under
  hardware emulation. Determine a stable polygon/texture/API boundary before
  promising an SDL3 GPU implementation. See
  `docs/gfxplus-rendering.md`. The archived `Tactpat.exe` updater has been
  fingerprinted and scanned for embedded Glide 2.1/Voodoo strings, but the
  **actual installed 3Dfx game executable is still missing**. A further public
  search found patch archives and installers but no verified, directly
  analyzable installed GFX+ output; resume when a properly sourced 3Dfx
  `EF2000.EXE` with its companion data files is available.

## Reproduce

```sh
nix build -L .
nix flake check
nix run . -- ../EF2000/c/EF2000/_EF2000.EXE --output local/nix-startup
nix develop -c env EF2000_EXE=../EF2000/c/EF2000/_EF2000.EXE python3 -m unittest discover -s tests -v
```

The asset-backed suite verifies native GUI rendering/export, native/QEMU
diagnostic startup, and the exact C5A7Ch renderer overflow through its unchanged
recovery handler. Run the opt-in SDL suite separately on a private Xvfb display
with `EF2000_TEST_DISPLAY=:193`; it covers device selection, mapped/inverted
axes, buttons, hotplug, focus release, and control-mode selection before launch.
Nix package checks omit tests requiring the external installation. Synthetic guests
test native signal/syscall/LDT behavior, timer frames, alignment recovery, writable
overlays, and BIOS bridging without game assets. Emulator tests cover actual
16-bit execution, changed-code cache invalidation, VBE bank round trips, and mouse
motion/buttons. The original main-menu framebuffer was visually verified.

## Scope and conventions

Apply `~/projects/gamepkgstd/trunk/gamepkgstd` incrementally. Current adoption:
small focused commits, Ingo Ruhnke as author, Codex co-author attribution,
GPL-3.0-or-later/REUSE, one `VERSION` file, native flake aliases, checks, and this
handoff. Desktop integration and broader runtime conventions follow when there
is a playable Linux application. R36S, Android, and other platform ports are
outside current scope. The source license does not license original game data.

Keep commits on one history. For the current numbered bundle handoff, the
agreed prerequisite is `63eaca4faf8ba9a268e931170a3c47591777372d`.
Create each cumulative bundle from that base to the new tip, including **all**
agent commits from previous bundles; verify it with `git bundle verify`.
Each numbered bundle replaces the previous download, never creates a parallel
workline, and is the only delivery artifact (no patch files).


Input support is implemented and verified with an SDL virtual joystick.
A physical USB stick was unavailable during development; hardware validation
remains follow-up work. Pause release, device selection, and automatic controls
have dedicated regression coverage. All commits include exact model attribution.


Sound discovery has an independent DOSBox-X probe and captured reference values.

### Native C++ Unicorn migration (incremental)

- [x] Add original-executable loader, ring-3/LDT bootstrap and X-32 startup
  services using the Unicorn **C API**, with a synthetic CPU self-test.
- [x] Port DOS reads, memory allocation, keyboard ACK, PIT/IRQ0 injection and
  early video/mouse BIOS services; verify the 30M-instruction checkpoint,
  interrupt and IRQ count against the Python reference.
- [x] Add the SDL3 C++ frontend, VGA/VBE banked framebuffer capture and
  startup progress text; render original splash and menu graphics.
- [x] Continue beyond 30M through the original Quick Combat mission loading
  and flight cockpit using a fully C++/Unicorn, scripted 920M-instruction replay.
- [x] Forward SDL3 mouse, keyboard and joystick input to the guest BIOS/
  keyboard controller/gameport, with headless scripted input options.
- [x] Add a separately rooted C++ DOS copy-on-write overlay for file create,
  open, read, write, seek, flush and close; reject writes without an overlay.
- [x] Return a BIOS disk failure for `INT 13h` without exposing host sectors;
  test the status/CF result in the asset-free C++ suite. Follow up with a
  long-running original-asset replay and the emitted AH/DL diagnostic. A
  virtual DOS file drive does **not** imply a BIOS raw-sector device.
- [x] Add an independent C++ Unicorn real-mode CPU with bounded driver calls,
  shared DOS memory/register exchange and opt-in SB16 discovery ports; validate
  synthetic guest IN/OUT and interrupt/timeout failures.
- [x] Port the 8237 DMA, 8259 PIC and SB16 DSP completion models to native C++;
  test a complete synthetic guest-programmed PCM transfer through IRQ7, DSP
  acknowledgement and real-mode ISR return.
- [x] Poll DSP DMA from protected-mode PIT ticks, run the original 16-bit IRQ
  handler independently of an outstanding X-32 call, and stream normalized
  PCM into SDL3 in windowed mode. Synthetic deferred-IRQ and format tests pass.
- [x] Validate the full original `SB16.DIG` in native C++: DSP 4.5,
  IRQ7/DMA1/5 original-driver self-test, real IVT updates, and sound-enabled
  1.5B-instruction replay to the flight cockpit. Add optional lossless
  complete-frame guest PCM capture via `--wav-capture DIR`.
- [x] Handle SDL3 gamepads with standardized left/right sticks (no accidental
  trigger-axis mapping), joystick re-enumeration, explicit control selection,
  input diagnostics and throttled event pumping. Add a headless original-game
  gameport read/write checkpoint and synthetic x86 port I/O self-test.
- [x] Port read-only MSCDEX INT 2Fh and single-BINARY CUE/Red Book audio to C++;
  enable `--cd-cue`, virtual CD-mode `EF2000.CFG`/`MDI.INI`, mix CD/SB16 streams
  via SDL3, and test synthetic disc requests, timing, and PCM capture format.
- [x] Correct X-32 low-memory selectors and refresh cached live segment bases
  when Miles maps the BIOS Data Area for CRT-retrace polling. Verify with
  `--self-test-bda` and a CD-enabled original-game startup replay.
- [x] Add a wall-clock/PIT scheduler for unlimited interactive C++ Unicorn
  sessions. Avoid Unicorn's implicit per-instruction counting hook, retain
  the exact instruction-mode diagnostics, and test block-slice/tick operation
  with a synthetic CPU loop (`--self-test-clock`).
- [x] Add Win64/Win32 MinGW-w64 C++ Unicorn/SDL3 ZIP packaging using the
  `sdl-jstest` Nix build pattern, host CryptoAPI for SHA-256 and `%LOCALAPPDATA%`
  for writable guest files. Keep game and CD assets out of the archives.
- [ ] Run the bundled Windows synthetic tests under Wine and verify real
  SDL3 windows, audio, CD music and gamepad input on Windows hardware.
- [ ] Verify live SDL3 rendering, window resizing, gamepad and flight controls
  on actual hardware, profile remaining performance bottlenecks in flight
  under the real-time scheduler, and test
  sustained interactive mission execution.
- [ ] Verify audible SDL3 SB16 and original-disc CD music using a full original
  installation and physical audio device; improve
  streaming latency if required and port remaining OS services before
  switching `.#unicorn` over to C++.
`--sound sb16` runs the unchanged SB16.DIG against the implemented DSP/PIC/DMA
model and plays captured digital PCM through SDL. FM music remains disabled in
this mode. Failed real-mode calls retain mutated memory and CPU state for
investigation. Sound is working according to the user's current flight report.

The F10 pause overlay provides fullscreen, resume, quit, and scrolling save/load
submenus. Native checkpoints include CPU/device state and the writable drive
overlay, with runtime identity validation and `--load-state` / `--state-dir`.
Save states currently require `--backend native64 --sound off`; unsupported
configurations show New Save (Unavailable) with the required flags. Save/load
and fresh-process restore have an original-asset regression test. The default
library now persists in `$XDG_DATA_HOME/ef2000/states` (fallback
`~/.local/share/ef2000/states`), independently of disposable output directories.
Older output-local libraries remain available with `--state-dir OUTPUT/states`.
