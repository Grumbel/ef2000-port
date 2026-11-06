<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 119: signed maximum and x87 entry audit

## `max3_signed` (`0x3cd4`, 27 bytes)

The original computes the signed maximum of EAX, EDX and EBX into EAX. It
preserves EBX, ECX and EDX; the implementation pushes and restores ECX while
using it as a temporary. The C body now computes `m = max(a,b); max(m,c)`;
ties return the same value. The callers in `0x2e04` and `0x33b4` use EAX and
overwrite the flags before any conditional branch. The six reviewed call sites
are `0x304c`, `0x30d9`, `0x31a3`, `0x3289`, `0x350c` and `0x3615`.

Original instruction body observed from the SHA-pinned payload:

```text
3cd4 push ecx
3cd5 cmp eax,edx
3cd7 jle 3cdd
3cd9 mov ecx,eax
3cdb jmp 3cdf
3cdd mov ecx,edx
3cdf cmp ecx,ebx
3ce1 jle 3ceb
3ce3 cmp eax,edx
3ce5 jg 3ced
3ce7 mov eax,edx
3ce9 pop ecx
3cea ret
3ceb mov eax,ebx
3ced pop ecx
3cee ret
3cef nop
```

The production OpenWatcom module and focused fixture emit the same 19-byte
body, which fits the original 27-byte extent. Both compile with zero warnings.
The SHA-bound OMF patch spans 27 bytes, and **253/253** original-vs-patched
isolated cases pass, including all 125 combinations of signed extrema and 128
seeded full-register triples. The comparison covers EAX, preserved registers
and writes (none).

The repeated-baseline replay uses all seven previously verified patches plus
this candidate, two original cockpit baselines, and the fixed DOS/VGA guest
clocks. Main menu (500M instructions) and quick-fight setup (700M) pass. The
cockpit checkpoint (1.5B) does not: framebuffer, stop status, interrupt/tick
counters match, but selected 64 KiB BSS changes from original hash
`93aec906...` to patched hash `fc712136...`. The `0x3cd4` exact-entry hit count
is zero, so this replay has not exercised the candidate call boundary. Keep
the function `spliced`, not `verified`, pending an explanation for this memory
effect and a replay that reaches the real caller.

## Overlapping x87 entries `0xa4470` / `0xa4472`

The payload bytes at `0xa4470` are `b0 03` (`mov al,3`), with no `ret`. The
instruction at `0xa4472` starts the checked-square-root body; that body returns
to callers at `0xa44a6`. Thus a call to `0xa4470` sets AL to the math error
code and falls through into the x87 routine. The two-byte entry is not an
independent C function, and the two entries share one return path.

The current C model for `0xa4472` treats the x87 input/output as a `double`
register value and explicitly omits the original x87 stack imbalance when
AL=3. That does not preserve the original ABI or behavior. Both tracker entries
are `blocked` until an overlapping-entry splice and x87-aware differential can
compare ST0, AL and stack depth. Do not splice the current C model.

## Commands and evidence

- `tools/ef2000_wcc.py --compiler <OpenWatcom-2.0-wcc386> --output /tmp/low_helpers.obj decomp/src/low_helpers.c`
- Same wrapper for `decomp/tests/ow_max3_signed.c`; both builds report zero warnings/errors.
- `tools/omf_inplace.py /tmp/ow_max3_signed.obj --payload /tmp/ef2000-payload.bin --reviewed-flow 0x3cd4 -o local/decomp-batch-119-max3/max3.efpatch`
- `tools/func_differential.py 0x3cd4 --spec decomp/tests/0x00003cd4.json --executable local/reference-01/drive/EF2000/_EF2000.EXE --patch local/decomp-batch-119-max3/max3.efpatch` (253/253 pass)
- `tools/replay_suite.py` with the seven verified patch files and this candidate, manifest `local/decomp-batch-119-max3/full-manifest.json`, `--baseline-repeats 2` (2/3 checkpoints pass; cockpit BSS mismatch and candidate not reached).

All executable, patch, replay and game-data files remain under ignored `local/`
or `/tmp`.
