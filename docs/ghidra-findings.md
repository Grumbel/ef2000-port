# First Ghidra decompilation findings

The SHA-pinned original `_EF2000.EXE` (1,684,561 bytes, SHA-256
`f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd`)
was imported and analyzed successfully with **Ghidra 11.1.2** and a Java 17
JDK. The import used the repository's X-32 payload extractor, the **0-based**
`x86:LE:32:default` image, and the BSS/entry-point annotations from
`EF2000Seed.java`. All addresses in this note are *guest image offsets*.

The resulting Ghidra project and exported pseudo-C are local research data:
`local/ghidra/projects/EF2000` and `local/ghidra/decompiled/`, respectively.
Neither original executable bytes nor bulk decompiler output belongs in Git.

## Cross-checked functions

| Entry | Observation | Evidence and confidence |
| --- | --- | --- |
| `0x86` | FlashTek/Watcom X-32 startup; clears BSS and calls runtime initialization | Ghidra flow matches the previously disassembled startup; strong |
| `0x9bd70` | Watcom initializer dispatcher | Traverses six-byte initializer records and calls function pointers; strong |
| `0x9bd20` | Watcom C entry, calls `0x8b274` | Direct call and runtime setup in Ghidra output; strong |
| `0x8b274` | Candidate game `main`; handles `playvideo` specially | Both Ghidra decompilation and the original instruction sequence; strong for branch, provisional function identity |
| `0x9d524` | ASCII case-insensitive string comparison | Both inputs are folded from `A`–`Z` to lowercase; returns first differing byte subtraction; strong |
| `0x8b2d8` | Separate video-mode initialization | Only called on the `playvideo` branch of `0x8b274` in the recovered entry path; probable |
| `0x8b39c` | TACTCOM initialization | Contains the `TACTCOM found ... K bytes of memory` diagnostic and a substantial setup call sequence; strong for initialization |
| `0xea270` | Resource/clump loader | Processes file/entry descriptors; its error path mentions `serclump: no file`; strong for loading, structure layout provisional |
| `0xaaee8` | Main control path | Sets up command/parser and mouse-related services, then enters repeated processing; strong for flow, purpose of individual calls provisional |
| `0xe9a24` | Guest memory-copy routine | Ghidra output agrees with disassembly and prior Unicorn hotspot sampling; strong |

### The `playvideo` branch

At `0x8b274`, EAX holds an argument count and EDX points to an argument-vector
array. The function checks for exactly two arguments, compares the second one
with the zero-terminated string **`playvideo`** at `0x18a7e8` via `0x9d524`,
and calls `0x8b2d8` if they match. It then returns zero without entering the
normal game initialization. Other invocations follow this call sequence:

1. `0xe29a0`: early runtime setup/allocation;
2. `0x8b39c`: TACTCOM/game initialization;
3. `0xea470`: floating-point exception setup;
4. `0xea270` twice, with descriptor pointers `0x1603f4` and `0x16040c`;
5. `0x9c3cf`: registers a callback;
6. `0xaaee8`: the main control path.

The two descriptor objects begin with pointers to the strings
**`clump_3.txt`** (`0x1914ac`) and **`clump_ss.txt`** (`0x1914c0`). Those
addresses and strings were checked directly against the extracted image,
not inferred solely from Ghidra's C-like output. The descriptors and their
resource lifetimes are good future targets for dynamic tracing and profiling.

### Memory-copy ABI matters

`0xe9a24` implements a forward copy in 32-byte unrolled blocks (`0xe9a3a`),
then 8-byte and byte tails. Direct disassembly verifies:

- Near-call stack arguments: `[EBP+8]` = byte count, `[EBP+12]` = source,
  `[EBP+16]` = destination.
- EAX and EDX advance with the destination and source, respectively; the
  routine restores the other saved general-purpose registers and exits via
  `RET 0Ch`.
- Ghidra's generic x86 signature describes a `void` function and does **not**
  model those EAX/EDX postconditions. Its signed-loop conditions are also
  awkwardly reconstructed. Check assembly and live traces before using its
  pseudo-C as an optimization specification.

Ghidra found **23 direct call references** to `0xe9a24` in this static image;
the list is incomplete for indirect calls and is *not* a frequency profile.
The copy-caller map additionally finds two callsites using the writable
graphics dispatch slot at `0x41f3d0`: `0xc3dc0` and `0xc3f38`. A live guest
profile verifies that both dispatched to the memcpy routine in the observed
startup run; their respective return IPs are `0xc3dc6` and `0xc3f3e`.
The earlier Unicorn sampling put 23.3% of sampled blocks at `0xe9a3a` during
one startup run. This supports studying the callers' byte counts and buffers,
but does not establish a sustained FPS improvement from `--fast-copy`.

### Real-game per-caller measurements

The C++ headless backend was built against Unicorn 2.1.3 and run for 12 wall
seconds with the supplied original game installation, SB16 DMA enabled,
`--profile-copy-callers`, and `--profile-hot-blocks`. Without either copy
acceleration option, it counted **154,811** memcpy calls moving **816,738,410**
bytes. The three leading callsites accounted for **99.35%** of those bytes:

| Guest return IP | Calls | Bytes | Interpretation |
| --- | ---: | ---: | --- |
| `0xc3dc6` | 6,072 | 388,614,144 | Indirect chunked transfer into the banked VGA memory window |
| `0xc3f3e` | 3,304 | 211,456,000 | Indirect transfer of a 64,000-byte frame into VGA memory |
| `0xc3886` | 3,299 | 211,328,000 | Direct copy of the main graphics buffer |

The first two are invisible to a search for direct calls to `0xe9a24`.
They call through `[0x41f3d0]`, as verified by disassembly. The previously
implemented `--fast-copy` handles the third site, but its conservative data
range excludes destination addresses in DOS/VGA RAM.

This led to a separate, **opt-in** `--fast-copy-vga` extension. It permits a
host copy only when the entire destination lies inside the mapped VGA
`[DOS_BASE+0xa0000, DOS_BASE+0xb0000)` bank and all other existing fast-copy
guards (source range, minimum size, flat DS, non-overlap, return address)
hold. It implies `--fast-copy`; the existing flag alone keeps its former
eligibility rules. There is no memory-mapped I/O write callback for that bank:
the VBE implementation captures its contents via `uc_mem_read`.

In a second 12-second run with `--fast-copy-vga`, **460,911** VGA-window
copies totaling **29,498,305,536** bytes took the new path, while **344,325**
ordinary large copies also took the previous path. The workload subsequently
spent many more iterations drawing; raw interrupt and byte totals are thus
**not comparable as FPS measurements**. Both the original `--fast-copy` and
new VGA mode produced byte-identical **640×400** menu-frame captures in
separate eight-second SB16 runs (PPM SHA-256
`fef5340cabcbe7356bb52225fa4508e9cd12d0f49f480d6fd646e6b78d4f9d73`).
That verifies one rendered menu state, not game-wide correctness or frame pacing.

## Reproduce and extend

From `nix develop`, with a local copy of the matching proprietary executable:

```sh
python3 tools/ghidra_ef2000.py import /path/to/_EF2000.EXE
python3 tools/ghidra_ef2000.py decompile \
  0xe9a24 0x8b274 0x9d524 0x8b39c 0xea270 0xaaee8
```

Next useful investigations are (1) checking the VGA fast path in actual
windowed gameplay and measuring FPS/frame pacing, (2) tracing which clump
files and records feed the cockpit/loading path, and (3) matching `0xaaee8`'s
repeating calls with menu and in-flight traces. Treat every unverified Ghidra
function signature as a hypothesis until checked against the Watcom register ABI.

See [ghidra.md](ghidra.md), [startup-symbols.md](startup-symbols.md), and
[execution-probes.md](execution-probes.md) for workflow and earlier evidence.
