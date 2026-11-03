<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# OpenWatcom OMF splice planner (Phase 1, §3.7)

The first stage of `tools/omf_splice.py` is an **offline inspector and patch
planner only**. It does not modify the EF2000 image and **no emulator knows
how to load `.efpatch` yet**. The first real splice must wait for §3.8 arena
reservation, an audited control-flow boundary, and an independent function
differential/replay. Do not run a planned patch in the game by manually
writing its bytes into the executable.

## Inspect a compiler object

~~~sh
ef2000-wcc decomp/tests/fixture.c
python3 tools/omf_splice.py decomp/build/fixture.obj \
  -o local/omf/fixture-inspection.json
~~~

Inspection reports segment sizes, initialized ranges, code/data publics,
external names, 32-bit relocation locations and their thread-expanded target
methods. It is useful even before a function has a confirmed ABI.

## Produce an offline patch plan (not ready to run)

The following example is **illustrative**: replace both the object path and
the function address with a *real* recovered C function that has `state`
`compiles` or later and `sig_status=confirmed` in `functions.tsv`, and whose
cross-function edges and body holes have been independently audited. The
explicit `--reviewed-flow` assertion only records that audit; the tool cannot
prove a control-flow boundary on its own.

~~~sh
python3 tools/omf_splice.py decomp/build/recovered.obj \
  --image local/ghidra/payload.bin \
  --functions decomp/symbols/functions.tsv \
  --globals decomp/symbols/globals.tsv \
  --reviewed-flow 0xADDRESS \
  --arena-base 0xARENA_BASE \
  -o local/patches/recovered.efpatch
~~~

`--arena-base` must be a **real independently validated** unused guest range
above the original image, *not* a guessed value. In particular, space beyond
the image can still belong to the original heap, stack, DPMI mappings or
graphics allocations. The runtime high-water measurement and mapping are
still outstanding in §3.8.

The emitted JSON carries its source image's SHA-256 digest and a list of
hex-encoded memory writes. Code that fits the original function is relocated
to the original entry and its remainder padded with `INT3`; larger code is
placed in an arena and the original entry becomes a `JMP rel32`. Data
segments also live in the arena. Neither layout is yet guaranteed usable by
the runtime.

Current fail-closed restrictions:

- Exactly one USE32 CODE segment and one public function at offset zero per
  object, matching a unique named tracker entry.
- A confirmed ABI, compiled function state and an explicit reviewed-flow
  assertion; no `library`, dead or anonymous entry replacement.
- Only completely initialized code and ordinary 32-bit `OFFSET32` relocations
  (`FIXUPP` Location 9). Segment-relative and self-relative displacements
  are resolved, including target threads; external addresses must uniquely
  resolve through the two canonical symbol tables.
- Frame methods F4/F5 only in generated patches. Other group/frame methods,
  short/selector/far relocations, LIDATA, COMDAT, BAKPAT, unsupported record
  forms and overlapping data/fixups stop the entire module. No guessing.
- One source object per plan, no multi-object linker, no actual arena
  reservation, no guest loading, no save-state patch-set binding, and no
  differential/replay verification. Those are subsequent steps.

The implementation is exercised with *synthetic OMF records* in
`tests/test_omf_splice.py`: external near-call relocations, internal absolute
data references, inline and redirected placement, fixup threads, malformed
references, and CLI inspect/build. It has **not** yet passed an end-to-end
test using a real OpenWatcom-emitted `.obj`; unsupported structures can be
discovered once that fixture is available in the test environment.

OMF record interpretation follows the
[TIS OMF specification](https://openwatcom.org/ftp/devel/docs/omf.pdf),
especially the FIXUPP Locat/Fix Data fields and their persistent threads.
