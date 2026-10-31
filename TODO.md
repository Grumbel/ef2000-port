# EF2000 Linux port handoff

## Current state

Read `README.md`, then `docs/startup-runtime.md` and `docs/x32-services.md`.
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
2. Investigate the reported physical Xbox 360 Wireless Controller regression:
   SDL registers the device and auto selects joystick controls, but flight does
   not react with `--sound sb16`. Preserve virtual joystick coverage and validate
   the complete SDL-to-game path, including pause/resume.
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

Keep commits on one history. For handoff from this initially empty repository,
create a full-history bundle with `git bundle create <path> HEAD`, without
prerequisites. Use the first root commit as the work-line identifier in its name;
later bundles must continue to include the entire history from this empty base.


Input support is implemented and verified with an SDL virtual joystick.
A physical USB stick was unavailable during development; hardware validation
remains follow-up work. Pause release, device selection, and automatic controls
have dedicated regression coverage. All commits include exact model attribution.


Sound discovery has an independent DOSBox-X probe and captured reference values.
`--sound sb16` runs the unchanged SB16.DIG against the implemented DSP/PIC/DMA
model and plays captured digital PCM through SDL. FM music remains disabled in
this mode. Failed real-mode calls retain mutated memory and CPU state for
investigation. Sound is working according to the user's current flight report.

The F10 pause overlay provides fullscreen, resume, quit, and scrolling save/load
submenus. Native checkpoints include CPU/device state and the writable drive
overlay, with runtime identity validation and `--load-state` / `--state-dir`.
Save states currently require `--backend native64 --sound off`; unsupported
configurations show New Save (Unavailable) with the required flags. Save/load
and fresh-process restore have an original-asset regression test.
