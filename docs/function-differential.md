<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Isolated function differential tester (Phase 1, §3.10)

`tools/func_differential.py` runs an **original** EF2000 function and an
**in-place EFPATCH1 replacement** from identical, fresh, flat 32-bit Unicorn
guests. The fixture is drawn from the SHA-pinned original executable. It
never writes game data to the repository, and it cannot run an arena patch.

~~~sh
nix run -L .#func-differential -- 0x0000432c \
  --executable ../EF2000/c/EF2000/_EF2000.EXE \
  --patch local/patches/hiscore.efpatch \
  --spec decomp/tests/0x000432c.json
~~~

This is an **example of the eventual command**, not a presently passing
game-function test: the recovered source, compiled OMF object, validated
patch, and representative high-score inputs are still to be provided.

Each test spec lives in `decomp/tests/0x<seven-hex-digits>.json` (derived
work, not GPL). Example schema (numbers accept integers or hexadecimal
strings; `hex` data is little-endian guest RAM):

~~~json
{
  "address": "0x0000432c",
  "function_size": 15,
  "max_instructions": 20000,
  "cases": [
    {
      "name": "qualifies",
      "registers": { "eax": "0x10203040" },
      "stack_args": [],
      "memory": [{"address": "0x1970a0", "hex": "e8030000"}]
    }
  ]
}
~~~

The numeric `function_size` above matches the tracked 15-byte extent of
`0x432c`. The runner insists that it matches `functions.tsv` and that the
target is marked with a confirmed ABI and at least `compiles` state.
Input memory writes must initialize every guest pointer/data region read by
that function. It is unsafe to assume the tiny example represents a complete
input state for 0x432c. `stack_args` contains 32-bit words in the order found
at `[ESP+4]` onward; its length must equal the confirmed `stack_bytes` / 4.

The tool loads the canonical ABI contract from
`decomp/symbols/signatures.tsv` (not Ghidra's unconfirmed ABI), uses a
return sentinel at the top of the guest stack, and verifies both calls end
by returning to it. It compares:

- All ABI output registers and reviewed preserved registers (including AL,
  AX and other subregisters), plus exact callee stack cleanup. A spec can
  request an `eflags_mask` where flags are part of the verified contract.
- Final values at the **union of all runtime-written RAM addresses**, even if
  one implementation skipped a write; different final bytes are a failure.
- Memory above the incoming ESP, including caller arguments, is not omitted.
  Only dead callee stack-scratch bytes below incoming ESP are excluded by the
  bounded `stack_scratch_bytes` region (default 64 KiB), following plan §2.2.
  Stack accesses outside this reserved scratch region remain checked.
- Unique dynamic basic-block entry addresses observed **inside the specified
  original function extent**. Counts are diagnostic, not a block-coverage
  percentage: no static block denominator has been audited yet.

The tester sets deterministic, nonzero input GPRs and lets each case override
full registers by name. It defaults to 64 MiB mapped guest RAM, stack top
at `0x3fff000`, and at most 20,000 instructions per call. These can be
overridden with `guest_size`, `stack_top`, `return_sentinel`,
`stack_scratch_bytes`, `max_instructions`. Allocation outside the map,
unmatched function return and instruction-budget exhaustion fail closed.

## Limits before the first real function is verified

- **No x87 state capture** yet. Signatures using ST0 are rejected rather
  than pretending that EAX/EDX represent an 80-bit result.
- **No DOS, X-32, IRQ, selector or I/O simulation** in isolated calls. A
  call into one of those services faults instead of being silently stubbed.
- **No trace-captured live call states** or callee-return stubs yet. Recorded
  memory and registers are manually curated in JSON. Capture and replay is
  the next required enhancement before broad game-function testing.
- Real OpenWatcom OMF and a safely packed replacement are not available in
  this checkout. The asset-free synthetic test executes two equivalent
  machine-code implementations when Python Unicorn is installed; the other
  tests verify ABI parsing, spec bounds and memory comparison boundaries.
- A passing isolated comparison is **not** a `verified` tracker state.
  Per §1(5), a real replacement also needs OpenWatcom compilation, an
  in-game splice and the golden replay suite (§3.9).

## X-32 allocator watermarks (Phase 1, §3.8)

Both runtimes now report observed allocator bounds on exit:

- C++ Unicorn: `x32_memory: heap_peak=0x... stack_low=0x...` on stdout.
- Native64/QEMU: `MEMORY HEAP_PEAK=... STACK_LOW=...` in `calls.log`,
  parsed as `memory_watermark` in `report.json`.

The values are **process high-water marks** from X-32 `3508h` (heap grow)
and `3509h` (stack reserve), which start at offsets `0x4df000` and
`0x3ffd000` respectively in the synthetic 64 MiB guest model. The two
backends also report the final allocator state and call counts. They do
not track arbitrary direct guest stores or host device mappings; a quiet
allocator is not proof of a safe executable patch arena.

For a useful measurement, play through multiple screens and a sustained
flight session on the original executable; the initial 30-million-instruction
startup test alone establishes only a startup lower bound. Do not enable
arena writes until a cross-backend range/mapping audit and a guard-region
test establish a defensible reservation. In particular, the Unicorn table
map currently begins just above 64 MiB, and the native64 checkpoint copies
exactly 64 MiB of guest RAM: simply enlarging one constant is unsafe.
