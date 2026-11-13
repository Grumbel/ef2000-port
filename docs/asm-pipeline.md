# Mechanical ASM baseline

The recovery sequence is executable → address/byte annotated ASM → conservative
C → readable C. Original instruction matching is the preferred static evidence;
a fingerprint match or successful decode is not proof of equivalence. Preserve
separate behavioral comparison evidence when compiler output differs.

Export all tracker entries from the validated Ghidra payload:

```sh
python3 tools/asm_inventory.py --output local/asm-inventory
```

Use a new output directory for each export. To inspect a cluster:

```sh
python3 tools/asm_inventory.py --address 0x10d80b --address 0x10d94f \
  --output local/asm-heap-growth
```

Each entry has a readable `.asm` listing and JSON instructions with original
addresses, bytes, mnemonic, operands, and direct control-transfer targets where
objdump supplies a numeric operand. The manifest records the payload digest,
selected tracker-row digest, decoder version and tracker recovery metadata.
Every decoded byte and instruction address is checked against the payload;
missing or inconsistent decoding fails the export. Original bytes stay under
the ignored `local/` directory. The CLI rejects other output locations.

These are **unverified linear windows**, using tracker `size`, not exact Ghidra
function bodies. They may include data or adjacent functions and omit detached
blocks. Overlaps are reported among selected entries; undecodable instructions
are listed in each record. No recovery state is promoted by this tool. Do not
use the sum of window sizes as recovered-code coverage. An interrupted export
has no final manifest and must not be treated as complete.

For translation, audit the body boundaries and ABI against callers, compile
with `tools/ef2000_wcc.py`, then inspect `tools/asm_diff.py` and
`tools/asm_match.py`. The latter compares instruction features, not exact
instruction equivalence. Existing isolated differential fixtures provide a
second check without running the game.

Next automation steps are exact body-range import, code/data and indirect-edge
classification, relocation-aware compiled instruction comparison, and dependency
reports for menu/cockpit blockers. Keep unresolved edges and compiler/runtime
ASM exceptions explicit rather than guessing C or treating them as complete.

Schema 2 also records `control_flow.edges`, `control_flow.review_edges`, and
per-entry `review_edge_count`. Calls and conditional branches retain a
fallthrough edge; unconditional jumps, returns, `hlt` and `ud2` do not.
Numeric targets resolve only to decoded instruction starts in the same window.
Indirect transfers, targets outside those starts (including instruction
interiors), window-exiting fallthrough, interrupt/system transitions and bad
instructions remain explicit review items. External calls are review items,
not automatically errors. This mnemonic-based report neither proves reachability
nor resolves jump tables; its input still consists of unverified linear windows.

Ghidra inventory/export now also writes `body-ranges.json`, containing half-open
function ranges and Ghidra instruction locations/lengths, with a SHA-256 of the
actual initialized program memory. Export to a separate directory to preserve
an existing pseudo-C index:

```sh
python3 tools/ghidra_ef2000.py inventory --output local/ghidra/body-inventory
python3 tools/asm_inventory.py --address 0x10d80b \
  --body-ranges local/ghidra/body-inventory/body-ranges.json \
  --output local/asm-body-heap-growth
```

Schema 3 preserves these ranges without decoding the gaps. It checks payload
provenance, span ordering and bounds, entry containment, and instruction byte
continuity separately in every span. A missing body is an error, never a fallback
to a tracker-sized window. The manifest binds the body export by its digest.
`ghidra_instruction_layout_matches` compares every decoded instruction start and
length against Ghidra, exposing embedded data or disagreeing decoders; absent
layout metadata produces `null`, not agreement. These are exact exports of
Ghidra's current ownership hypotheses, not proof that those hypotheses are
complete or correct.

2026-11-13 full check (Ghidra 12.1.4): 4,277 bodies, including 40 with multiple
ranges; no body overlaps; instruction layouts agree for 4,275. Entries 0xfa3d4
and 0x11f6d8 disagree and need review. Thirteen additional tracker entries have
no Ghidra body and are already blocked. The selected full-body export and the
explicit missing-entry tracker are under ignored `local/`; none are promoted.
