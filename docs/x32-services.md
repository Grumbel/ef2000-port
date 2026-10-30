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
