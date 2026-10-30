# Recovered X-32 startup services

## Evidence and reproduction

The original extender was measured by replacing the **application entry in a
disposable executable copy** with [x32_services.S](../probes/x32_services.S).
The original loader and extender bytes are unchanged. The hook records state,
calls the real services, writes `X32PROBE.BIN`, and exits before game startup.
It is an instrumented experiment, not an unmodified full-game trace.

```sh
python3 tools/reference_run.py --game-dir ../EF2000/c/EF2000 \
  --output local/x32-reference --entry-probe --headless
python3 tools/read_x32_probe.py local/x32-reference/drive/EF2000/X32PROBE.BIN
```

Requires DOSBox-X and GNU `as`/`objcopy`. The output directory must be new.
[x32-reference.json](x32-reference.json) records measured values, binary and hook
hashes, and the reference configuration. The original installation was unchanged.
The hook's current record format is 256 bytes, version 4, with a completion marker.

## Segments and AX=3505h

The initial application DS/ES is an expand-down writable 32-bit data segment:
LSL reports limit `0x7f`; LAR reports rights `0x40f700`. Initial SS is a separate
expand-down segment with limit `0xfff`. FS and GS cover the PSP and environment;
both reported `0xffffffff` limits in the reference.

The `3505h` dispatch table points to file `0x2836`. For the first startup request,
BX is DS and ECX is `0x260`. The handler updates the descriptor, returns its
effective limit through LSL in ECX, and clears CF. The captured result is ECX=
`0x260`, DS limit=`0x260`, ZF=1, CF=0. Raising an expand-down limit protects more
low addresses; it does not reduce the maximum usable address.

The harness implements this exact startup request using an LDT descriptor and
reloads the segment cache on return. General descriptor allocation, arbitrary
limits, and other X-32 selector services remain unsupported.

## Non-DPMI AX=3509h: stack reservation

The handler at file `0x298b` selects the non-DPMI path at `0x2bd8` when the mode
byte is not `0xc0`. The measured mode was zero. Do not infer VCPI versus raw
startup solely from that observation.

State offsets in the original 16-bit extender data selector:

| Offset | Meaning used by these paths |
| --- | --- |
| `0x52c` | Application linear base |
| `0x530` | Current heap end |
| `0x534` | Current stack reservation boundary |
| `0x538` | Free-space accounting |
| `0x840` | Heap ceiling |
| `0x844` | Minimum stack boundary |
| `0x5d4` | Extender mode byte |

The recovered success calculation is:

```text
candidate = (EBX - ECX) & 0xfffff000
consumed = old_stack_boundary - candidate
new_free = old_free - consumed
new_stack_boundary = candidate
EDX = candidate - new_free - 1
```

Subtraction underflow, crossing the minimum stack boundary, or insufficient
free space rejects the request. Successful non-DPMI execution preserves EAX,
EBX, ECX, and ESI. Final arithmetic flags come from DEC EDX followed by CLC.

In the reference, input EBX=`0xdfffeb96`, ECX=`0x4000`; the stack boundary moves
from `0xdfffe000` to `0xdfffa000`, free accounting from `0x77c51000` to
`0x77c4d000`, and EDX becomes `0x683acfff`. Translated C reproduces the captured
state, EDX, and full recorded flags from those inputs.

## Non-DPMI AX=3508h: heap growth

The core calculation is at file `0x28b5..0x2911`. It rounds ECX upward to a page,
rejects values at or above the heap ceiling, and consumes free space for positive
growth. Requests below the existing heap end do not shrink it. Success returns:

```text
ECX = current_heap_end
EAX = max(stack_boundary - free_space, minimum_stack_boundary)
```

Final arithmetic flags come from the comparison selecting that maximum, followed
by CLC. The reference request `0x4e0000` grows the heap from `0x4df000` by one page,
returns ECX=`0x4e0000`, EAX=`0xc0000000`, and flags=`0x3a86`. The differential
test reproduces all of these and the post-call accounting.

The original path can extend a swap file through the helper at `0x4821`.
The current harness reserves its memory upfront and uses a declared no-swap
allocation model. DPMI reallocation, swapping, and complete error ABI behavior
are not implemented; unsupported/exhausted model operations stop explicitly.

## DOS version query

The actual startup issues AX=`0x3005`, not AX=`0x3000`. A reference query with
sentinel register values returned EAX=5 (DOS 5.0), cleared only BL and CX,
preserved their upper bits, and preserved EDX/ESI/EDI and flags. The harness
supports this startup query only. The partial-register behavior matters: clearing
all of ECX would be a broader change than the measured service performs.

## DOS time query

The initializer calls AH=`2ch` through the register-block wrapper's INT 21h at
image `0x11f2a4`. A separate disposable-copy probe measures four queries:

```sh
python3 tools/reference_run.py --game-dir ../EF2000/c/EF2000 \
  --output local/dos-time-reference --time-probe --headless
python3 tools/read_dos_time_probe.py local/dos-time-reference/drive/EF2000/DOSTIME.BIN
```

[dos_time.S](../probes/dos_time.S) supplies sentinel registers, records input and
output flags, and inserts busy waits between queries. The 148-byte version-1
record contains four full register samples and a completion marker.
[dos-time-reference.json](dos-time-reference.json) retains the measurements,
original/patched/hook hashes, and configuration. The source installation remained
unchanged. This hook measures the service directly, not the full initializer.

Observed: EAX becomes `0x2c00` even with nonzero high bits and AL on input;
CH/CL return hours/minutes and DH/DL seconds/hundredths. The upper halves of
ECX/EDX, EBX/ESI/EDI, and flags (including a deliberately set CF) are preserved.
The four samples advance across the waits.

The harness supports this measured call site using host UTC `CLOCK_REALTIME`,
not DOSBox local time or its tick cadence. Raw host samples are retained in the
transcript/report. Native64 uses Linux clock_gettime; QEMU uses clock_gettime64
with the same 64-bit timespec layout. Both backends pass the startup integration
check and continue through vector setup and game initialization. See
[startup-runtime.md](startup-runtime.md) for the current boundary.

## Vectors, descriptors, conventional memory, and files

Additional disposable-copy measurements are in
[runtime-reference.json](runtime-reference.json). Reproduce each independently:

```sh
python3 tools/reference_run.py --game-dir ../EF2000/c/EF2000 \
  --output local/vector-reference --vector-probe --headless
python3 tools/read_runtime_probe.py local/vector-reference/drive/EF2000/XVECTORS.BIN
python3 tools/reference_run.py --game-dir ../EF2000/c/EF2000 \
  --output local/memory-reference --memory-probe --headless
python3 tools/read_runtime_probe.py local/memory-reference/drive/EF2000/DOSMEM.BIN
python3 tools/reference_run.py --game-dir ../EF2000/c/EF2000 \
  --output local/file-reference --file-probe --headless
python3 tools/read_runtime_probe.py local/file-reference/drive/EF2000/DOSFILES.BIN
```

Observed `2503h` returns the packed real-mode vector in EBX; `2502h` returns the
protected offset in EBX and selector in ES. `2505h` sets the real vector from
EBX; `2504h` sets the protected vector from DS:EDX. CL selects the vector while
upper ECX bits survive. INT 80h replacement/query/restoration was measured.
Arithmetic flags other than CF vary with extender bookkeeping; new harness
services model success/failure CF rather than claiming exact arithmetic flags.

Static dispatch recovery adds `2506h` (file `0x2069`, install a protected handler
and real-mode thunk), `2532h` (`0x25c9`, query exception handler), and `2533h`
(`0x260b`, set exception handler). Exceptions below 8 share the protected vector
table. The harness stores these registrations; exception delivery remains an
explicit fault. Default handlers and thunk addresses are declared synthetic
values, not addresses copied from DOSBox.

`252bh` at file `0x2403` implements memory residency. BX=`0501h` locks a
selector-relative ECX/EDX range; `0601h` unlocks it. The probe measures both.
Upper EAX is a PTE/accounting temporary in the non-DPMI path, not preserved input.
The harness maps the observed ranges to Linux mlock/munlock, returning failure
when the host refuses. It does not reproduce extender PTE reference counts.

Descriptor services are recovered from file `0x274d` (3501h, allocate one),
`0x27af` (3502h, free), `0x27f2` (3503h, set base), `0x211e` (3504h, query base),
and `0x2836` (3505h, set limit). The harness maintains 16 user LDT slots and
updates actual Linux descriptors. Limits above 1 MiB use page granularity.
`350bh`, file `0x2af5`, returns the smaller of remaining heap ceiling and free
allocation accounting in the declared no-swap model.

DOS AH=48h takes paragraphs from BX, ignoring upper EBX. Measured success returns
the real segment in EAX and an application-relative linear pointer in EBX;
failure returns EAX=8, EBX=available paragraphs, CF=1. The harness allocates from
`0x10000..0x8ffff`, reserving the final conventional segment for the bridge stack;
release/resizing is not implemented yet. A host alias at `0x01000000` represents
DOS physical memory, including the BDA and VGA window, through the loader patches.

The file probe measures read-only open, device info, read, seek, close, missing
files, and console device info. EAX is zero-extended from DOS AX except that
X-32 buffered reads can return a full 32-bit count. Filename-copy open clears
EDX; a completed read clears ECX/EDX. IOCTL 4400h returns EAX=2 for a C: disk
file and replaces only DX; the console returns `80d3h`. Seek uses CX:DX low
halves and returns DX:AX low halves, preserving upper EDX. Successful close of
the measured single-reference file returns EAX=`3e01h`. Linux handles back these
operations. Writable opens first copy originals to the local drive overlay;
create/write operations and preservation of source files have native tests.
Synthetic native guests
test actual byte transfer, EOF, seek, handle reuse/errors, conventional memory,
descriptor loads/limits, and vector round trips without original game assets.
