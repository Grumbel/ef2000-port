<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 083: renderer queue, stream opcodes and drivers

This batch adds 20 C entry-point models in seven small modules. It is based
on disassembly of the SHA-pinned original X-32 payload
(`3fd26b189efe289feb9e6ac701d592a5d48d7c1e152527aa00c01611e87e0fdb`).
These are **host-tested drafts**, not Watcom-compiled, spliced, or guest-
verified replacements. The new code uses guest-memory addresses explicitly
and exposes unresolved indirect calls, DOS interrupts and I/O ports as host
callbacks. Do not promote any of them to `compiles` without OpenWatcom OMF
evidence or to `verified` without a differential guest run.

| Subsystem | Entry addresses | Recovered behavior / evidence |
| --- | --- | --- |
| Depth queue | `c3fd4`, `c409c`, `c4167`, `c41b0` | Quicksort of paired signed-word key/index arrays, finishing insertion pass, sort and draw dispatch, 0x800-entry overflow |
| Word stream | `c5369`, `c5384`, `c53b4`, `c53cf` | Four 9–27-byte entries (one shared branch): 0xbf sentinel write, endian conversion, and absolute/biased 48-byte-stride index conversion |
| Bitstream | `fa16e`, `fa251` | Huffman 0xff escape with two extra bits and a literal byte; dual variable-width output packet decoder |
| Scene and viewport | `c4db4`, `c8ccc`, `e7a34` | BE scene header and record parser bridge; explicit five/six-word packed viewport argument order and carry-dependent flush |
| Audio and video | `e0bcc`, `117a0f` | Device reset via port writes, guarded by bit 7; scanline converter selection and addresses |
| Threaded opcode handlers | `c547f`, `c549e`, `c54b7`, `c54d6` | Four independent handler **entries** with RET paths: two state toggles and two resets, dispatching to separate unrecovered interpreters |
| DOS bridge | `11f287` | INT 21h through six-register block, results and carry status written back, then status callback |

## Corrected original boundaries

The Ghidra 12.1.4 inventory previously counted 4,277 functions, with the
two additional renderer stream entry points from batch 082. This batch adds
**eight omitted entry boundaries**:

- The four word-stream code entries `c5369`, `c5384`, `c53b4`, `c53cf`.
  The first is a shared return branch reached from `c5372` rather than
  a separately proven `CALL` target. In
  particular, `c5372` formerly had size 27, incorrectly swallowing the
  complete `c5384` subroutine. Its actual extent is 18 bytes.
- Four threaded-dispatch opcode handlers `c547f`, `c549e`, `c54b7`,
  `c54d6`, between previously inventoried `c5441` and `c54ef`. These are
  callable interpreter *entry points*, not ordinary independent functions;
  their model leaves the full c5551/c5681 interpreters in the guest.

The `c41b0` original inventory size was **131** bytes, stopping at
`c4233`. But that is the start of an alternate path into the same routine.
At `c41c4`, the program rewrites the JGE rel8 displacement from `0x1b`
(branch to `c41e0`) to `0x6e` (branch to `c4233`). The latter is a valid
overflow handler that ends at `c4259`. The true covered extent of `c41b0`
is **170** bytes. The C model stores the displaced byte in guest RAM as
ordinary state; preserving code patching in the executable is an ABI issue.

With these corrections there are **4,287 tracked entries** (including 10
manually found boundaries across batches 082–083) and 27 `drafted` models.

## Host validation

Four standalone C tests (GCC, `-std=c11 -O2 -Wall -Wextra -Werror` with
`EF_DECOMP_HOST_TEST=1`) cover all 20 entries:

- `decomp_renderer_sort_queue_test.c`: paired-key alignment, negative and
  duplicate depths, normal queue sorting and callback order, both branches
  after overflow to 0x800, sentinel preservation and byte-order helpers.
- `decomp_bitstream_extended_test.c`: direct and escaped Huffman leaf,
  variable widths and two output streams, byte and bit accumulator cursors.
- `decomp_graphics_driver_entries_test.c`: endian scene header and record
  dispatch, packed viewport layout, hardware port reset order, indexed and
  interleaved video converter arguments.
- `decomp_opcode_dos_bridge_test.c`: branch-dependent opcode dispatch
  sequences, six DOS register results and carry propagation.

The `flake.nix` host-check block includes these tests. Runtime limitations:

1. Quicksort and insertion sort rely on the original valid indexed buffer,
   a lower-bound sentinel, and 0x800 capacity; malformed inputs are not
   made safe. The host queue does not emulate EFLAGS/DF or exact register
   preservation.
2. The scene parser `c4e3b`, threaded opcodes `c5551`/`c5681`, indirect
   viewport checks, frame handler, audio I/O and DOS interrupt remain guest
   dependencies. Tests only check wrapper effects and callback arguments.
3. Bitstream refill uses the existing `fa332` dependency. Tests exercise
   buffer-resident streams; multi-refill behavior requires the original
   executable as an oracle.
4. OpenWatcom/Ghidra/Unicorn are absent in this execution environment.
   Revalidate boundaries in a rebuilt Ghidra database, compile individual
   functions as OMF, and run guest state differentials before any live splice.
