<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 087: environment and damage parameters

Twenty distinct, named X-32 entry points have been reconstructed as
**host-tested drafts**: fifteen 74-byte line parsers and five closely
associated environment-mode flag operations. Source is in
`decomp/src/environment_damage_params.c` and
`decomp/src/environment_mode_flags.c`.

Original entry ranges and destination addresses were recovered by
disassembling the 1,662,896-byte payload, SHA-256
`3fd26b189efe289feb9e6ac701d592a5d48d7c1e152527aa00c01611e87e0fdb`,
and independently checked with the opt-in original-payload audit
`tests/test_decomp_environment_payload.py`. No original executable bytes,
assets, or Ghidra-generated pseudo-C are committed.

## Fifteen damage and fade parameters

All these original functions have the **same 74-byte instruction sequence**
apart from the destination address passed to the original `sscanf`-like
routine at `0xdf26f`. Each reads the current input cursor at `0x425af8`,
searches for an LF or NUL, advances the cursor, modifies two input bytes,
then calls the parser using format pointer `0x18fffc` and its own destination.
The original parser return value in EAX is forwarded without modification.

| Original entry | Named function | Output in guest RAM |
| --- | --- | --- |
| `db95c` | `craft_hit_water` | `425bdc` |
| `db9a8` | `craft_hit_ground` | `425be0` |
| `db9f4` | `cannon_hit_concrete` | `425bda` |
| `dba40` | `missile_hit` | `425bde` |
| `dba8c` | `cannon_hit_water` | `425bea` |
| `dbad8` | `cannon_hit_ground` | `425bee` |
| `dbb24` | `missile_hit_concrete` | `425bd4` |
| `dbb70` | `bomb_hit` | `425be6` |
| `dbbbc` | `missile_hit_water` | `425bec` |
| `dbc08` | `missile_hit_ground` | `425bd6` |
| `dbc54` | `bomb_hit_concrete` | `425bd8` |
| `dbca0` | `craft_hit_building` | `425bd2` |
| `dbcec` | `bomb_hit_water` | `425be2` |
| `dbd38` | `bomb_hit_ground` | `425be8` |
| `dbd84` | `depth_fade` | `425bce` |

These names describe **configuration-reader entry points**, not simulated
collision or damage calculation; the actual parser/calculation semantics
must be verified separately. We have not assumed the format string's C
conversion type or number of bytes written into each output global.

### Original input mutation

The input cursor sequence matters more than the number of wrappers:

1. Start at the dword pointer stored in `425af8`.
2. Find the first LF (`0a`) or NUL (`00`); write NUL at that position.
3. Advance the cursor by one; **also write NUL at the old position minus
   one**, which strips CR for CRLF lines.
4. Store the advanced cursor at `425af8`; pass the original beginning of
   the line to `df26f` with format `18fffc` and the individual output.

The original second write is not conditional. On LF-only or final
NUL-terminated lines it removes the last actual character. On an empty
line it writes one byte *before* the current cursor. This was preserved as
an explicit `ORIGINAL BUG` comment and tested; replacing it with normal
line-ending trimming would break original-state equivalence.

## Five environment flag entries

| Original entry | Name | Exact global mutation |
| --- | --- | --- |
| `db8a8` | `unfix_horizon` | `byte[425c86] = 1` |
| `db8b0` | `load_horizon` | `byte[425c86] = 0` |
| `dbe28` | `corridor` | `byte[425bc4] |= 02` |
| `dbe30` | `planet2` | `byte[425bc4] |= 04`, `byte[159130] |= 04`; publish the new first mask in AH |
| `dbe54` | `space` | `byte[425bc4] |= 42` |

The `planet2` model takes and returns EAX explicitly because the original
modifies AH but preserves AL and the upper half of EAX. All other flag
entries model only their RAM side effects; their full guest calling
conventions still require independent verification.

## Evidence, checks, and remaining work

- Host test: `tests/decomp_environment_params_test.c`, built with strict
  GCC C11 warnings. All twenty functions are exercised. The 15 parser
  wrappers are independently checked for original destination mapping,
  pointer updates, correct guest-call arguments and EAX propagation; tests
  cover CRLF, LF-only, NUL-only, and empty-line input.
- Opt-in original-payload test: set `EF2000_PAYLOAD` to the raw X-32 payload
  and run `python3 -m unittest discover -s tests -p 'test_decomp_environment_payload.py'`.
  It checks the pinned hash, all fifteen output pointers and the actual
  original disassembly's line terminator operations, plus the five flag
  instruction sequences. It skips without the executable or `objdump`.
- Host test is integrated into the flake check. The general Python suite
  also discovers the optional source audit (skipping when no original data
  is supplied).

**No function is guest-verified or splice-ready.** The original parser
`df26f` remains an injected host hook, its stack calling convention needs
confirmation, and OpenWatcom OMF compilation, guest differential runs and
golden replays have not been executed here. No statuses were changed to
`compiles` or `verified`; twenty named entries became `drafted`.
