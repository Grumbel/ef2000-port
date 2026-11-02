# Headless Ghidra analysis of the X-32 game image

EF2000's `_EF2000.EXE` is a FlashTek X-32 executable, not a conventional
PE/ELF/LE program. Importing the entire DOS executable as 32-bit code puts
addresses in the wrong places. Instead, the existing SHA-pinned extractor
provides the 32-bit initialized payload directly. Its **offset zero** is the
same offset used by the C++ Unicorn guest, startup traces and
`--profile-hot-blocks`.

This is a local reverse-engineering workflow; it is **not** a runtime or
packaging dependency. Original executable bytes, Ghidra projects, and generated
pseudo-C remain in the ignored `local/` directory by default.

## Initialize once

`nix develop` includes Ghidra and sets `GHIDRA_HOME` to the Nix store's
`lib/ghidra` installation. Run from the EF2000 repository root:

```sh
nix develop
python3 tools/ghidra_ef2000.py import \
  ../EF2000/c/EF2000/_EF2000.EXE
```

Outside the development shell, `nix run .#ghidra` opens the GUI and
`nix run .#ghidra-headless -- ...` runs the headless analyzer. With a separate
Ghidra installation, set `GHIDRA_HOME=/path/to/ghidra`, or supply
`--analyze-headless /path/to/ghidra/support/analyzeHeadless` to the Python tool.
The command verifies the executable SHA-256,
exports `payload.bin` and `manifest.json`, and imports the binary to the
`local/ghidra/projects/EF2000` project using Ghidra's `BinaryLoader`,
`x86:LE:32:default`, and load base **0**.

The pre-analysis script `tools/ghidra/EF2000Seed.java` creates an uninitialized
non-executable BSS region `[0x195fb0, 0x4de2f0)` and seeds known startup,
Watcom and copy-function addresses, plus selected data labels from
`docs/startup-symbols.md`. `0x8b274` is deliberately named a *candidate* main.

Once imported, open the project through Ghidra's GUI to explore cross-references,
rename functions and apply verified types. The importer refuses to overwrite an
existing project. Reuse the same project with `decompile`; preserve your manual
Ghidra annotations in the local project directory or export them separately.

## Export selected functions

```sh
python3 tools/ghidra_ef2000.py decompile 0xe9a24 0x8b274 0x9bd20
```

This processes the existing project **without rerunning automatic analysis**.
It generates `local/ghidra/decompiled/0x000e9a24.c` and corresponding files for
the other requested addresses. Each file includes incoming references and a
Ghidra pseudo-C body. If an address is inside a discovered function, that
containing function is exported; an unrecognized address is explicitly
disassembled and treated as a tentative function entry.

For performance investigations:

1. Run the C++ Unicorn backend with `--profile-hot-blocks --profile-copy`.
2. Collect `hot_block: ip=0x...` entries (the samples are **basic blocks**, not
   necessarily function entry points).
3. Decompile those guest addresses and identify the surrounding call graph.
4. Verify candidates against original code and guest traces before replacing
   anything with a C++ fast path. Keep optimizations opt-in until gameplay and
   timing checks pass.

The original memory-copy routine starts at `0xe9a24`. The existing
`--fast-copy` interception and its synthetic self-test make this a useful first
case for checking decompiler output against observed EAX/EDX, RET `0Ch`, and
stack behavior.

To correlate individual copy callers with the C++ backend's
`--profile-copy-callers` output, run:

```sh
python3 tools/ghidra_ef2000.py copy-callers
```

This writes `local/ghidra/copy-callers.tsv`, with each call instruction's
guest offset, its post-CALL **return IP**, and Ghidra's containing function.
Join `copy_caller: ret=0x...` with the `return_ip` column. The map includes
direct calls to `0xe9a24` and *candidate* indirect calls through the graphics
copy-function slot at `0x41f3d0`; the latter are not proven to call memcpy
without matching runtime samples. Ghidra may omit further indirect calls.

## Interpretation limits

- The initialized payload mixes code and data; static function recovery can
  misinterpret tables or jump targets. It is not a complete executable loader.
- The raw image omits runtime selector allocation, DOS memory and extender
  patches. Uninitialized BSS is represented but has no valid startup values.
- Ghidra's generic x86 calling convention is **not** the Watcom X-32 ABI.
  Decompiled parameters, return types and register preservation are hypotheses,
  particularly for register arguments, thunks and hand-written assembly.
- None of the generated `.c` files are drop-in C++ replacements. Do not commit
  copyrighted binary bytes or bulk decompiled game code; retain small
  evidence-backed annotations and behavioral models in the source repository.
- This integration's Python command tests can run without Ghidra or game data.
  An end-to-end import and decompilation still require both locally.

See [loader.md](loader.md), [startup-symbols.md](startup-symbols.md), and
[execution-probes.md](execution-probes.md) for the address model and evidence.

The first successful end-to-end import and decompilation is documented in
[ghidra-findings.md](ghidra-findings.md), with cross-checked guest offsets,
resource-loader callsites, and ABI caveats.
