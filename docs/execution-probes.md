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
