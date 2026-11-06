<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 130: cdecl runtime state getter

## `get_state_106708` (`0x106708`, 13 bytes)

Original disassembly:

```text
8b 44 24 04       mov eax, [esp+4]
85 c0             test eax, eax
75 01             jne 0x106711
c3                ret
8b 40 04          mov eax, [eax+4]
c3                ret
```

The pointer is a caller-clean cdecl argument at `[ESP+4]`; the result is EAX.
Null returns zero; non-null returns the dword at `object+4`. The direct call
is from `trace_call_1028f4` at `0x102919`. That wrapper is called from three
reviewed paths: `0xd7594@0xd75ad`, `0xd75c8@0xd75e1`, and
`0xd75fc@0xd7615`. Its fifth argument is also caller-clean, correcting its
prior `stack_bytes` entry from 4 to 0.

The original signature had been incorrectly modeled as an EAX-register
argument. `EF_CDECL` now marks original stack-ABI definitions, and the
differential runner independently models incoming stack argument bytes and
callee cleanup. Its stack argument size is derived from confirmed
`[ESP+n]` signature operands; signatures without explicit stack operands
retain their prior behavior. The OMF splicer now matches a leading underscore
on cdecl code publics as well as the Watcom trailing-underscore decoration.
Synthetic tests cover the new name and stack-argument behavior. Inspecting
the wrapper also corrected its true stack cleanup (`RET`, caller cleanup),
scratch registers, and the third caller at `0xd75c8@0xd75e1`.

## Evidence and remaining reachability

- `decomp/src/runtime_helpers.c` compiles with OpenWatcom with zero warnings.
- The focused fixture emits exactly 13 bytes, byte-for-byte equal to the
  original body. OMF splicing accepts the leading-underscore public and
  creates an in-place patch at `0x106708`.
- Four isolated differential cases pass: null pointer, zero field, a
  nonzero value, and all-one value. Output and preserved registers match.
- The repeated-baseline menu/setup/cockpit suite with the 17 previous
  verified patches and this patch passes 3/3, with matching selected BSS,
  framebuffer and execution counters. The target entry is hit zero times in
  all three cases.
- Separate original runs at 500M startup instructions and 1.5B scripted
  cockpit instructions did not produce a target call capture. Keep this
  candidate `spliced`, not `verified`, until a replay reaches it and a live
  captured call passes differential.

The replay used the manifest and 17-patch list from
`docs/subsystems/decomp-batch-128.md`, adding
`local/decomp-batch-130-get-state/get-state.efpatch`. All binaries and
captures stay under ignored `local/`.

Asset-free checks run:

```sh
/nix/store/30qcpgvjzwnsky1q7wfnsq5pf9l32l04-python3-3.14.7-env/bin/python3 \
  -m unittest tests.test_func_differential tests.test_omf_inplace -v
python3 tools/decomp_status.py check
git diff --check
```

Both test modules pass (12 tests total), and the tracker reports 4,287 valid
functions. The original executable SHA-256 is
`f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd`.

The full `python3 -m unittest discover -s tests -v` run executed 394 tests,
with 81 skipped, one failure and three errors unrelated to this change: the
CLI path test inherited `/home/ingo` XDG overrides instead of its expected
temporary defaults, and three ELF tests rejected the host GCC's
`.note.gnu.property` section. The focused differential/OMF tests above pass.
