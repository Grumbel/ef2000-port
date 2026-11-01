# Execution probes

The current execution path is the [native startup harness](startup-runtime.md).
The small first-entry probe below remains useful for reproducing the initial
backend experiment; it does not describe the latest startup progress.

## Original entry under QEMU

Reproduce from `ef2000/` with Python 3 and `qemu-i386`:

```sh
python3 tools/probe_entry.py ../EF2000/c/EF2000/_EF2000.EXE \
  --output local/qemu-entry-01
```

The output directory must be new. The tool extracts the fingerprinted payload,
wraps it in a diagnostic ELF32 image, and runs QEMU with one instruction per
translation block and register logging. No assembler or C compiler is required.
The application offsets map to guest addresses starting at zero. QEMU's
`-B 0x40000000` chooses a host guest-base offset; this is not the original DOS
allocation base. A small wrapper at guest `0x08000000` initializes a synthetic
stack and jumps to the unchanged application entry at `0x86`.

Observed with `qemu-i386 version 10.2.4`:

| Application offset | Original instruction | Result |
| --- | --- | --- |
| `0x86` | `mov bx, ds` | BX becomes the synthetic data selector `0x2b` |
| `0x89` | `mov ecx, 0x260` | ECX=`0x260` |
| `0x8f` | `mov ax, 0x3505` | AX=`0x3505` |
| `0x93` | `int 0x21` | Stops: no DOS/extender interrupt handler |

The stop is target SIGSEGV, not a successful DOS call. The probe reports success
only if the trace contains the expected instruction boundary, register values,
and signal. It disables core-file generation. `report.json`, `trace.log`, and
`console.log` preserve the evidence locally.

This validates execution of **three original instructions** in QEMU's Linux-user
mode. It does not validate original selector values, extender services, x87
behavior, timing, or game startup. FS/GS, the PSP, the environment, and extender
patches are intentionally not prepared yet. QEMU Linux-user is an available
experimental execution backend; its suitability for the complete game remains
unproven.

`probe.elf` embeds the original payload. Keep it in ignored local/build storage;
do not commit or distribute it as source.

## Disposable DOSBox-X reference experiments

```sh
python3 tools/reference_run.py --game-dir ../EF2000/c/EF2000 \
  --output local/reference-01 --seconds 10 --headless
```

This copies the installation before mounting it, records hashes, requests INT 21h
and file-I/O logging, and runs the original `gui` startup with its environment
variables. Default configuration: 32 MiB, normal CPU core, fixed 60,000 cycles,
surface video, MIDI disabled. Headless mode uses dummy audio/video. These settings
are experiment inputs, not established optimal or faithful game settings.

The script records the emulator process status and compares both the original
and copied installation after the run. A host timeout bounds runs if the
emulator's time limit does not terminate them.

The 10-second experiment using DOSBox-X `2026.05.02`, commit `5817c64`, exited
with code zero, without a host timeout or changes to either installation. Video
logs changed from text output to a 640x400 surface and recorded
`MOUSE:Unhandled videomode 68 on reset`. This is not proof of a displayed menu.
No requested INT 21h/file-I/O trace appeared in this installed build. Use an
instrumented build or another tracing method before treating this as a reference
for loader register state or syscall coverage.

## Unicorn original-menu mouse dispatch

The protected-mode Unicorn probe now reaches the original 640x400 German main
menu with the supplied `_EF2000.EXE` and its unmodified DID archives. Use
`--trace-mouse --timer-instructions 100000` alongside timed `--mouse-event`
options and `--report` to record grouped guest call sites rather than every
individual mouse poll. The addresses below refer to the extracted protected
image, not the on-disk DOS stub.

- `0xCDFE8` reads INT 33h function 03h (position and buttons), including the
  game's historical Y/2 scaling; `0xCE02C` reads relative motion (0Bh), and
  `0xCE064` reads the button mask alone.
- The main menu uses the separate `0xAC854` input path. It updates a software
  cursor from mickeys and passes button state to `0xACBE4` for hit-testing.
  `0xACFB8` dispatches the resulting control event.
- An asset-backed move to mouse `(180,85)` after menu initialization produced
  software cursor `(179,84)` and hit control id 7. A left click registered
  actions 25 (press), 9 (held), and 33 (release) for that same control. This
  coordinate is in the *window title bar*, not the menu icon's clickable body.
- An earlier press during menu initialization instead entered a wait-for-release
  loop at `0xAC569`. A click probe must move and press *after* that handoff;
  an unchanged screenshot by itself does not identify the failing input stage.
- The title-bar widget runs `0xF1A3F` → `0xE1D14` with a pressed flag of
  `0x10`. The parent window's status `7` fails the focus/activation gate at
  `0xA4B08`. *This is not the game menu action.* The original GUI resource
  declares these windows with `nojump`; the separate `JumpToFront`/`Style`
  handlers do not establish a missing state transition in the emulator.
- The asset's `window arcade` starts at `(150,80)` with size `100×40`; its
  `arcadesel` icon covers the window body below the title. A click at
  `(165,110)` targets that icon, and release produces action `33`, nested
  widget flag `0x20`. The low-level widget's callback at `+0x71` is zero,
  but the game uses separate script masks for `select entering`, `unselect
  leaving`, and `call release` (handlers `0xF6868`, `0xF74B4`, `0xF757C`).
  The menu's event loop stops on release, and the original game begins
  opening/loading `newgui03.lbm` for the next screen. Direct filename opens
  return DOS error 2, then a successful 178,688-byte read from the archive
  follows. A later click cannot be used to test a second menu action because
  the game has already left that event loop.
- The scripted `S` key also takes the screen-loading path; `Escape` opens the
  original "Quit EF2000?" confirmation in the same menu. This establishes
  that keyboard IRQ delivery, GUI command dispatch, and the game-data script
  are functioning. A framebuffer exported while the next screen is loading
  can still show the previous menu; verify the subsequent video handoff before
  reporting navigation failure.
- Extending the *Schnelles Gefecht* click replay to **230 million protected
  instructions** completes the handoff: the guest renders the original 640×400
  Quick Combat screen with difficulty choices, 12 mission buttons, overview,
  high scores, and `Akzeptieren`. At 158–163 million instructions the screenshot
  still showed the previous menu because the game was reading the next screen
  from its archive. This is an instruction-budget artifact, not failed input.

For a repeatable headless Quick Combat screenshot (replace the path with the
original installation location):

```sh
python3 tools/unicorn_backend.py /path/to/_EF2000.EXE \
  --game-dir /path/to --timer-instructions 100000 \
  --instruction-limit 230000000 \
  --mouse-event 1400:165:110:0 \
  --mouse-event 1450:165:110:1 \
  --mouse-event 1470:165:110:0 \
  --framebuffer /tmp/ef2000-quick-combat.png \
  --report /tmp/ef2000-quick-combat.json
```

These observations establish that the mouse driver, software cursor, control
hit-test, GUI event dispatcher, scripted menu activation, and subsequent screen
load are connected. They **do not** yet validate mission start or flight under
Unicorn. Do not clear the guest's window-status bits to make clicks work: the
actual menu icon is already dispatching its action.

## Unicorn Quick Combat mission start

The original GUI script in `DID1ST.DAT` defines `window difficulty` at
`(100,40)`, `window miss_sel` at `(100,135)`, and `window summary` at `(100,240)`.
Their clickable controls lie in the window bodies, below the title bars:

| Action | Mouse target | Original script action |
| --- | --- | --- |
| Rookie difficulty | `(205,74)` | `gpwChangeDifficulty` |
| Mission 01 | `(130,176)` | `gpwSelectLevel` |
| Accept box (right of label) | `(287,368)` | `gpwArcadeAccepted` |

After opening Quick Combat with the first-menu icon click at ticks
`1400/1450/1470`, the new GUI polls events by approximately tick `1678` with
`--timer-instructions 100000`. A follow-up replay selected Rookie at ticks
`1790/1810/1830`, mission 01 at `1850/1870/1890`, and accepted at
`1910/1930/1950` (move/press/release for each). The screen changed to the
original *Simulation wird geladen...* loading status. The guest then read
additional mission and effects assets, reaching the original payload's
decompressor before stopping at instruction `325724093`.

The stop is **not** an unimplemented flight renderer: original code at
`0xFA153` compares a checksum with word `[0x16279E]`; on mismatch it branches
to `INT 3` at `0xFA167`. The current portable backend reports
`unsupported_interface: unimplemented INT 03h at IP=000fa166`, with the last
video mode still VBE `0x100` (640×400). The routine at `0xFA2E4` computes an
8-bit two-accumulator checksum over the decompressed output starting at
`[0x162794]`, with size `[0x162790]`. The preceding archive reads do not by
themselves identify whether corrupted input, decompressor execution, or another
guest state caused the mismatch. Preserve the guest breakpoint and compare
expected/actual checksums and buffer bytes on the next probe; treating `INT 3`
as a no-op would conceal a failed asset integrity check.
