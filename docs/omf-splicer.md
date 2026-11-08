<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# OpenWatcom OMF splice planner (Phase 1, §3.7)

`tools/omf_splice.py` inspects OpenWatcom OMF and creates a reviewable JSON
patch plan. `tools/efpatch.py` can pack **in-place-only** plans into the
binary `EFPATCH1` format. The native64 harness and C++ Unicorn runner accept
this restricted format via repeatable `--patch FILE` arguments, applying it
to a disposable copy of the extracted guest image. Original executable and
game assets are never modified. No replacement is considered verified without
the differential and full-game replay stages.

## Inspect a compiler object

~~~sh
ef2000-wcc decomp/tests/fixture.c
python3 tools/omf_splice.py decomp/build/fixture.obj \
  -o local/omf/fixture-inspection.json
~~~

Inspection reports segment sizes, initialized ranges, code/data publics,
external names, 32-bit relocation locations and their thread-expanded target
methods. It is useful even before a function has a confirmed ABI.

## Build an in-place-only patch in one step

Once the OpenWatcom object has actually compiled and been inspected, the
function tracker has reached `state=compiles`, and its flow boundary has been
independently reviewed, the guarded wrapper can construct the complete
SHA-bound `EFPATCH1` file directly:

~~~sh
ef2000-wcc decomp/src/hiscore.c
nix run -L .#omf-inplace -- decomp/build/hiscore.obj \
  --executable ../EF2000/c/EF2000/_EF2000.EXE \
  --reviewed-flow 0x432c \
  -o local/patches/0x000432c.efpatch
~~~

The `0x432c` source is presently **drafted**, not compiled or verified, so
this is a **next-stage command**, not yet a proven successful patch. The
`--reviewed-flow` argument asserts an external review; it does not perform
Ghidra's flow/body-hole audit itself. Do not advance the tracker based only
on the native host-model test.

`tools/omf_inplace.py` requires the supported SHA-pinned original payload,
a single canonical public function with a confirmed ABI and compiled tracker
state, and fully resolved relocations. It refuses any code growth beyond
the original function, private initialized data, patch arena or redirect.
It rejects unknown or ambiguous globals, writes **only** the patch file,
and refuses to overwrite an existing output. With an accepted object, test
it using `nix run -L .#func-differential -- 0x432c --executable ...
--patch local/patches/0x000432c.efpatch` before attempting a game replay.

Synthetic tests exercise the complete OMF → relocated in-place code →
EFPATCH1 conversion, including its external data fixup and rejection rules;
an optional Unicorn test executes both sides when Python Unicorn is present.
The original executable and a real OpenWatcom object remain the final gate.

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
  -o local/patches/recovered.efpatch.json
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
segments also live in the arena. **Only the first case, with zero arena
writes, can currently be packed and run.**

~~~sh
python3 tools/efpatch.py local/patches/recovered.efpatch.json \
  --image local/ghidra/payload.bin \
  -o local/patches/recovered.efpatch

# C++ Unicorn backend:
nix run .#ef2000-unicorn -- /path/to/_EF2000.EXE \
  --patch local/patches/recovered.efpatch --windowed

# Native64 backend:
python3 tools/run_startup.py /path/to/_EF2000.EXE \
  --patch local/patches/recovered.efpatch --output local/startup-patched
~~~

`EFPATCH1` is a compact little-endian binary: 8-byte `EF2000P1` magic,
32-byte SHA-256 of the **original initialized payload**, 32-bit write count,
then repeated 32-bit address, length, and data. Both runners verify digest,
bounds, count, total size and nonoverlap (including across multiple patches)
before applying any write. The native save-state identity includes the
ordered patch file digests, preventing a patched run from accepting an
unpatched or differently patched save state. Unpatched identity remains
unchanged for compatibility. The native harness logs the patch identifiers
in its run report; C++ Unicorn prints the applied patch count on startup.

The original memory size remains 64 MiB. All patch writes must fit within
the original *initialized payload*; arena/redirected plans fail during
packing, and malformed binary writes are rejected by both runners. This
scope deliberately avoids guessing a safe guest arena or allocator high-water
mark and does not yet support injected constants or enlarged functions.

Current fail-closed restrictions:

- Exactly one USE32 CODE segment and one public function at offset zero per
  object, matching a unique named tracker entry.
- A confirmed ABI, compiled function state and an explicit reviewed-flow
  assertion; no `library`, dead or anonymous entry replacement.
- Only completely initialized code and ordinary 32-bit `OFFSET32` relocations
  (`FIXUPP` Location 9). Segment-relative and self-relative displacements
  are resolved, including target threads; external addresses must uniquely
  resolve through the two canonical symbol tables.
- Frame methods F4/F5 are supported. Also supported are self-relative,
  external OFFSET32 calls framed against the OMF `FLAT` group in USE32 code;
  their call displacement is resolved from the reviewed guest address and the
  canonical target address. Other group/frame methods, short/selector/far
  relocations, LIDATA, COMDAT, BAKPAT, unsupported record forms and
  overlapping data/fixups stop the entire module. No guessing.
- One source object per plan; no multi-object linker, no actual arena
  reservation, and no differential/replay verification. Those are subsequent
  steps. Only explicitly reviewed *in-place* patches reach the guest.

The implementation is exercised with *synthetic OMF records* in
`tests/test_omf_splice.py`: external near-call relocations including a FLAT
group frame, internal absolute data references, inline and redirected
placement, fixup threads, malformed references, and CLI inspect/build. An
OpenWatcom-emitted runtime latch object also passed `omf_inplace.py`; its
resolved 29-byte replacement matched the original routine byte-for-byte.
Four additional asset-free tests validate binary packing, digest binding,
corruption detection and cross-patch overlaps; an independent C++ test
exercises the same wire layout.

OMF record interpretation follows the
[TIS OMF specification](https://openwatcom.org/ftp/devel/docs/omf.pdf),
especially the FIXUPP Locat/Fix Data fields and their persistent threads.
