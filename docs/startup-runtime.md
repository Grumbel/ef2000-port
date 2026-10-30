# Original-code startup harness

## Run and validate

From `ef2000/`, with Python 3, GCC, and GNU binutils:

```sh
python3 tools/run_startup.py ../EF2000/c/EF2000/_EF2000.EXE \
  --output local/startup-native
EF2000_EXE=../EF2000/c/EF2000/_EF2000.EXE python3 -m unittest discover -s tests -v
```

The default `native64` backend requires x86-64 Linux with `modify_ldt` support.
It needs no 32-bit libc. Output directories must be new. Generated ELF files
embed original game bytes and belong in ignored local/build storage.

This is not playable yet. The current explicit stop is DOS AH=`0x2c` (time query)
at application offset `0x11f2a4`, reached during runtime initialization.

## Native execution architecture

The ELF64 harness maps the initialized application at linear `0x10000000` and
creates 32-bit code/data descriptors with that base. A far return switches into
x86 compatibility mode and starts the unchanged original instructions at offset
`0x86`. Guest offsets remain unchanged; original code and data are not rewritten.

Unsupported INT 21h causes a hardware general-protection exception. A 64-bit
Linux signal handler recognizes it, dispatches supported interfaces, updates the
saved registers, and resumes the original code. Host calls use the 64-bit Linux
ABI. The harness uses an alternate signal stack and no libc inside handlers.
It removes the interception-induced RF bit from software-interrupt return state.

The 64-bit signal frame does not reliably expose guest FS/GS selector values;
native call logs mark them `ffffffff` (unavailable). The original startup's own
saved PSP and environment selectors are checked in guest memory instead.

## Verified startup checkpoint

The original code now:

- Updates the expand-down data descriptor through X-32 `3505h`.
- Queries DOS version with AX=`3005h`.
- Copies `gui` and the program path from the supplied PSP/environment.
- Clears `[0x195fb0, 0x4de2f0)`, deliberately seeded with `0xa5` by the harness.
- Reserves stack space through translated X-32 `3509h`.
- Enters runtime initializers and grows the heap through translated `3508h`.

The allocator translations in `runtime/x32.c` are tested against captured
original-extender state, return values, and arithmetic flags. See
[x32-services.md](x32-services.md) for the disassembly and measurements.

The current memory model provides 8 MiB at guest offsets starting at zero, an
initial stack at `0x7ff000`, a page-rounded heap end derived from `_end`, and two
guard-accounting pages between heap and stack growth. Its accounting is explicit;
addresses from the DOSBox run are not substituted as service return constants.
Swapping, complete failure paths, privileged/device operations, and remaining
DOS services are not supported yet. Unknown interfaces stop with a register log.

## Segment-fault diagnostic and QEMU comparison

```sh
python3 tools/run_startup.py ../EF2000/c/EF2000/_EF2000.EXE \
  --output local/native-limits --check-segment-limit
python3 tools/run_startup.py ../EF2000/c/EF2000/_EF2000.EXE \
  --output local/startup-qemu --backend qemu
```

The diagnostic reads offset `0x40` through DS after installing an expand-down
limit of `0x7f`. Native execution correctly raises #GP (trap 13); that expected
fault is a successful diagnostic, not successful game startup.

QEMU i386 10.2.4 reaches the same current DOS-call boundary and passes the startup
memory checks, but the diagnostic read succeeds rather than faulting. Therefore
QEMU remains a comparison/debugging backend, not evidence of segment-fault
equivalence. Its synthetic ELF32 profile uses a zero linear base.

A conventional native ELF32 probe was terminated by SIGSYS in this managed
environment. The ELF64/compatibility-mode design works here and does not require
32-bit Linux syscalls.
