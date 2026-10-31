# EF2000 Linux port

Behavior-preserving port of the original DOS game to Linux. Preserve the original
game instructions and data; replace DOS and hardware interfaces incrementally.
Emulation and binary translation are intermediate tools. Full decompilation into
maintainable source is the long-term goal.

## Status

The two-stage FlashTek X-32 loader, segment relocation table, initialized image,
BSS boundaries, and protected-mode entry have been recovered by static analysis.
The extractor reproduces the static relocation passes. A native x86-64 Linux
harness executes unchanged original 32-bit code through startup and into the
original **main menu and flight**, displayed in an SDL window with mouse,
keyboard, and joystick input. It handles
DOS services, memory allocation, timers, VGA/VBE banked graphics, and BIOS calls.
Unicorn executes real-mode driver requests. Audio is disabled in a local config
copy by default. Broader gameplay and mission completion remain unverified.
The first target is the installed `_EF2000.EXE` in the sibling
`../EF2000/c/EF2000/` directory. Other releases need separate fingerprints.

## Development

On x86-64 Linux, build and run with Nix:

```sh
nix build -L .                 # Precompile the native and QEMU harness objects
nix flake check                # Unit tests, app/help/version checks, REUSE
nix run . -- ../EF2000/c/EF2000/_EF2000.EXE --output local/nix-startup
nix develop                    # Python, GCC/binutils, QEMU, DOSBox-X, GDB
```

`packages.default` and `packages.ef2000` are the same native package;
`packages.ef2000-startup` is an alias. `apps.default` and `apps.ef2000` run the
`ef2000` launcher; `ef2000-startup` remains available as a command alias.
Builds require no original game files. The launcher accepts your external
executable, creates a new local output directory, and links it with the
precompiled harness. The executable's parent directory supplies the original
game assets, opened read-only. Configuration and other writes go under
`OUTPUT/drive/`. Close the window to stop; its final framebuffer is retained as
`OUTPUT/framebuffer.png`. Use a fresh output path for each run.

For a bounded run without a window:

```sh
nix run . -- ../EF2000/c/EF2000/_EF2000.EXE \
  --output local/menu-capture --headless --seconds 25
```

Python 3 is sufficient for the analysis tools. Direct startup development also
requires GCC/binutils, Unicorn, and SDL2 (provided by `nix develop`); QEMU and
DOSBox-X are used by comparison/reference tools:

```sh
python3 tools/inspect_exe.py ../EF2000/c/EF2000/_EF2000.EXE
python3 tools/extract_image.py ../EF2000/c/EF2000/_EF2000.EXE --output local/image-01
python3 -m unittest discover -s tests -v
# Include checks against the fingerprinted original:
EF2000_EXE=../EF2000/c/EF2000/_EF2000.EXE python3 -m unittest discover -s tests -v
# Execute original startup (x86-64 Linux, GCC/binutils; no 32-bit libc required):
python3 tools/run_startup.py ../EF2000/c/EF2000/_EF2000.EXE --output local/startup-01
```

Keep original executables and game data outside this repository. Put generated
memory dumps, traces, and extracted images in ignored `local/` or `build/`.
The native executable, package version, and help flags are checked without game
assets; original-file integration tests run explicitly outside the Nix build.

## License and continuity

New port code and documentation are GPL-3.0-or-later; see
[LICENSES/GPL-3.0-or-later.txt](LICENSES/GPL-3.0-or-later.txt) and `REUSE.toml`.
Original game executables and data remain external and retain their own rights.
Read [TODO.md](TODO.md) for the current stopping point, validation limits, next
steps, and the incremental adoption of the gamepkgstd Linux conventions.

## Documentation

- [Port architecture and milestones](docs/port-plan.md)
- [Executable and startup findings](docs/executable.md)
- [Recovered loader and image layout](docs/loader.md)
- [Startup symbols and interface worklist](docs/startup-symbols.md)
- [Execution probes and reference experiments](docs/execution-probes.md)
- [Native startup harness and validation](docs/startup-runtime.md)
- [Recovered extender services and differential evidence](docs/x32-services.md)
- [Sound driver research and diagnostic probe](docs/sound-research.md)
- [Research and evidence conventions](docs/research.md)

The mouse starts free and is captured only by a left-click inside the game
image. That first click captures the pointer without activating a game button.
Hold Escape for three seconds to release the desktop pointer; a short Escape
press still reaches the game normally. Losing focus also releases capture, and
regaining focus never captures it again. The window title shows the available
capture/release action. Relative input remains necessary for reliable menu
cursor positioning; uncaptured absolute input is not implemented yet.


Flight controls default to the first connected SDL joystick, or keyboard when no
joystick is connected. Use `--controls keyboard` or `--controls joystick` to force
a mode. Keyboard input includes held keys, releases, modifiers, function keys,
and the numeric keypad; losing focus releases held keys.

The joystick exposes up to four axes and four buttons through the DOS game port.
Use `--joystick N` to select an SDL device index or `--joystick off` to disable it.
`--joystick-axes=0,1,3,-2` remaps the four axes and inverts SDL axis 2. Defaults
are axes 0,1,2,3 and buttons 0–3. Use the original game's joystick calibration
for the selected stick; only the output directory's config is changed.
Joystick hotplug is supported; automatic flight-control selection happens at
startup. Keyboard and joystick flight input were verified under Xvfb, using an
SDL virtual joystick. When joystick controls are selected, an inherited
`JOY_FACTOR=0` is replaced with the executable default of 100 in the writable
config: digital-sound startup otherwise reduces joystick calibration to zero.
Add `--verbose` to compare `input: joystick` focus/axes/buttons
with `trace: GAMEPORT` samples received by the game; moving the stick should change
both. Focus loss centers axes and releases buttons.

Press **F10** to pause the native game and open the debug overlay. Use the
arrow keys and Enter, or click an entry. Escape goes back; F10 resumes. The
menu provides Resume, borderless fullscreen, separate Save State / Load State
submenus, and Quit. **F11** also toggles fullscreen. The save submenu offers
**New Save** and existing slots to overwrite; both lists scroll with the keyboard.
The overlay shows the current loaded/saved slot.

Save states use the native64 backend with the default `--sound off` configuration.
They include CPU/x87 context, guest and DOS memory, emulated video/input state,
open-file positions, and the writable drive overlay. Original assets stay
read-only. Loading replaces this run's drive overlay with the saved copy.
With audio enabled, **New Save (Unavailable)** explains that saving requires
`--sound off`. States are tied to the executable, runtime, and device configuration; incompatible
states are rejected. QEMU and experimental audio checkpoints are not supported.
Names include the date, observed menu/loading/flight phase, resolution, and
campaign/mission/navigation filenames when observed. Exact player location and
mission type are not yet recovered, so names do not guess those fields.

By default, states go into `$XDG_DATA_HOME/ef2000/states` (or
`~/.local/share/ef2000/states`), shared across runs even with different output
directories. Older saves in `OUTPUT/states` can be opened using `--state-dir OUTPUT/states`. Use `--state-dir DIR` for another library, or load a state directly with:

```sh
nix run . -- ../EF2000/c/EF2000/_EF2000.EXE --output local/restored-run \
  --state-dir local/states --load-state local/states/SAVE.efstate
```

`--fullscreen` starts in desktop fullscreen. Normal stdout reports paths,
configuration, renderer/window settings, observed game/video phase changes,
file creation, and state operations. `--verbose` adds detailed guest file,
console, PIT, and I/O activity. The complete native transcript remains in
`OUTPUT/calls.log` regardless of verbosity.
