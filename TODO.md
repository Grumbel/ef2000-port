# EF2000 Linux port handoff

## Current state

Read `README.md`, then `docs/startup-runtime.md` and `docs/x32-services.md`.
The current tip is reported by `git log -1`; this repository began without any
commits. Original game binaries/data stay outside Git and the Nix package.

The native x86-64 Linux harness executes original 32-bit instructions through
startup, runtime initializers, and game initialization. It reads the game archives
and configuration, drives timer handlers, and renders the original German
main menu in a 640×400 VBE mode. An SDL window supplies mouse input. Audio is
disabled in a local config copy by default. Gameplay beyond the menu remains
unvalidated. The target executable and
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
- Explicit absent-joystick, keyboard acknowledgement, CLI/STI, and PIT programming
  models, host-timer IRQ0 delivery, and BIOS tick advancement.
- Bounded Unicorn execution of original 16-bit driver code; unknown ports/INTs stop.
- VGA DAC and banked VBE video, original framebuffer export, SDL window, and mouse.
- Native AC-fault recovery for the CPU probe; flag reflection remains approximate.
- Original PSP/environment copying and clearing of deliberately seeded BSS.
- Hardware expand-down protection. QEMU reaches the same startup boundary but
  did not enforce the tested lower bound; retain it for comparison only.
- A pinned Linux flake that precompiles both harness backends without game data;
  the installed launcher only assembles/links the supplied payload locally.

## Next work

1. Trace menu selections and progress toward mission loading and flight. Verify
   each new path against actual runtime output; menu rendering does not prove gameplay.
2. Implement sound hardware/audio and additional device APIs; validate physical joystick hardware.
3. Implement exception-frame delivery and refine CLI/STI/AC flag reflection,
   timer coalescing/priority, memory residency reference counts, and DOS path handling.
   Keep unknown interfaces and model exhaustion as explicit stops.
4. Document data/mission discoveries separately under `docs/`, with fingerprints,
   offsets, reproducers, and observed/inferred status.

## Reproduce

```sh
nix build -L .
nix flake check
nix run . -- ../EF2000/c/EF2000/_EF2000.EXE --output local/nix-startup
nix develop -c env EF2000_EXE=../EF2000/c/EF2000/_EF2000.EXE python3 -m unittest discover -s tests -v
```

The original-file test suite currently has 55 passing tests, including native GUI
rendering/export and native/QEMU diagnostic startup to the same driver boundary.
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


Mission-loading follow-up: Quick Combat reaches a moving cockpit/terrain under
Xvfb. DOS date/time and file commit, vendor mouse probes, periodic VGA snapshots,
and removal of the startup service-count cutoff are implemented. Continue flight
input and audio work; use Xvfb for all automated UI interaction.


Keyboard/joystick: SDL physical keys now drive original IRQ1 through port 60h;
joystick axes/buttons drive port 201h with deterministic discharge counters.
Make/break, modifiers, extended keys, focus release, device hotplug, selection,
axis mapping/inversion, keyboard fallback, and original-flight virtual-stick
banking are covered. Physical joystick testing remains desirable.
