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
  actions 25 (press), 9 (held), and 33 (release) for that same control. The
  displayed menu nevertheless remained unchanged by the instruction budget.
  A second press/release on the same control generated the same action sequence
  again without transitioning to another screen.
- An earlier press during menu initialization instead entered a wait-for-release
  loop at `0xAC569`. A click probe must move and press *after* that handoff;
  an unchanged screenshot by itself does not identify the failing input stage.
- The nested widget callback does execute: `0xF1A3F` calls `0xE1D14` with a
  pressed flag of `0x10`. `--trace-mouse` records the subcontrol event mask,
  callback address, and the associated parent-control activation attempts in
  `mouse_gui.subcontrols` and `mouse_gui.activations`.
- A press/release on *Schnelles Gefecht* (control 7, mouse `(180,85)`) and on
  *Simulator* (control 8, mouse `(230,120)`) independently reached `0xE1D14`.
  Both parent controls had status `0x07`, flags `0x6c`, and enabled byte zero.
  The global activation checks at `0x41EDA8` and `0x41EDAA` were nonzero
  (`7` and `8`). The callback tests parent status `& 0x06` and accepts only
  `0` or `2`; status `7` yields `6`, so neither attempt reached the actual
  activation function `0xA4B08`. The game remained in the original menu.
- The original GUI script's `JumpToFront` handler at `0xF73A0` sets status bits
  `0x06`, while the `Style` handler at `0xF73C0` clears those bits and sets
  `0x02`. Whether an expected state change is missing in the emulated startup
  or the click must target a different phase of the original menu is not yet
  established. The observed status is a *guest* state, not an emulator-imposed
  block; forcibly clearing it is not a validated fix.

These observations establish that the mouse driver, software cursor, control
hit-test, GUI event dispatcher, and nested callback are connected. They **do
not** yet validate execution of a menu action or flight transition. The
top-level widget callback at `0xF1C58` is not used for these controls; follow
the `JumpToFront`/`Style` control-state transitions before attributing the
inactive menu to absent assets or changing the guest's status bytes.
