# Mission, navigation and campaign text reconstruction (bundle #064)

This is a **read-only syntax and structural reconstruction** of the supplied
German DOS TACTCOM installation. It does not claim to recover the simulation
engine's full semantics or to create an editor. The important discovery is
that several major scenario sources are **already loose plain-text files**;
they do not need to be extracted from `DID.DAT` first. They contain record
keywords, quoted names, count declarations and nested groups.

The tool `tools/mission_records.py` preserves every unrecognized directive,
its original source line and raw text. No original game assets or extracted
scenarios are committed. Other patch levels/languages require fresh checks.

## Verified inventory

The following totals were measured on the supplied game files with the
new parser; declaration counts and route lengths agree **exactly**:

| File | Declared/observed sections | Related records | Structural checks |
| --- | --- | --- | --- |
| `MISSION.CFG` | 746 `MISSION`; 77 `BASE` | 807 `WP_ROUTE` containing 3,178 `WAYPOINT`; 264 `ATTACK_ROUTE` containing 1,050 `ATTACK_NODE`; 77 `TIMESLOTS` containing 444 `TIMESLOT` | All explicit route/section counts match. |
| `NAVIG.NAV` | 1,118 `BASE_NAVIGATION` | 4,831 `NAVIG_CORRIDOR` / `NAVIG_ROUTE` pairs; 23,657 `WAYPOINT` | All route lengths and corridor counts match. One waypoint has a repeated index (see below). |
| `CAMPAIGN.CG` | 543 `SSNAME` target areas; 6,479 `MOB` mobiles | 6,479 `PIL` pilot lines | Both declared section counts match. |
| `CONFLICT.CG` | 542 `SSNAME` target areas; 6,458 `MOB` mobiles | 6,458 `PIL` lines | Both declared section counts match. |

`CAMPBRF.BRF` is a separate compact `#`-separated briefing record, not a
block file; its fields are listed in [existing tools research](existing-tools-and-releases.md).
It remains a future small-format parsing task.

### Mission file grammar, as far as established

1. `NUM_MISSIONS N` is followed by `N` `MISSION` sections. The supplied
   declaration includes an additional `INCLUDE_PILOTS` flag. The sixth
   argument of each `MISSION` header equals its **zero-based ordinal** for
   all 746 records. This is a measured position, not yet a claim about every
   purpose of the preceding/following arguments.
2. A `MISSION` section contains one or more `GROUP` / `FLIGHT` descriptions.
   Across the installation there are 542 `GROUP` and 265 `FLIGHT` records,
   corresponding exactly to 807 `WP_ROUTE` and 807 `TARGET` records. Every
   mission satisfies the per-section equality
   `GROUP + FLIGHT = WP_ROUTE = TARGET`.
3. Each `FLIGHT` has **20 `PYLON` records** with a slot and weapon name;
   there are 5,300 pylons total. This is a strikingly regular layout and
   likely maps directly to original game loading structures, but weapon
   compatibility and unused slot semantics remain to be recovered.
4. `WP_ROUTE N ...` is followed immediately by `N` `WAYPOINT` rows with
   indices starting at zero in this file; `ATTACK_ROUTE N` is followed by
   `N` `ATTACK_NODE` rows. Route counts match exactly across the whole file.
5. The subsequent `NUM_BASES 77` region contains 77 `BASE` sections. The
   fourth argument of each `BASE` header equals its ordinal; each base has
   exactly one `TIMESLOTS N` section, immediately followed by `N` `TIMESLOT`
   lines. The numeric fields' units and cross-links are not yet established.

The source also includes `3DP`, `PIL`, `ATTACK_PATTERN`, `TARGET_AREA` and
occasional numeric-only records. **Nothing is dropped** by the token parser,
even when no higher-level meaning is assigned.

### Navigation file grammar

- `NUM_BASE_NAVIGATIONS 1118` agrees with the count of `BASE_NAVIGATION`
  headers. Each header's **fourth argument** is its observed corridor count.
- Every base navigation has one `3D_POSITION`, and each `NAVIG_CORRIDOR`
  has one `NAVIG_ROUTE` of declared length followed by that many `WAYPOINT`
  lines. All 4,831 route lengths match.
- The syntax distinguishes `NAVIG_CORRIDOR` (navigation path grouping) from
  `NAVIG_ROUTE` (waypoint count); collapsing them would lose structure.
- One genuine data irregularity occurs at `NAVIG.NAV` lines **34868–34870**:
  a two-waypoint route repeats `WAYPOINT 0` for its second point instead of
  using index 1. It is reported as a **nonfatal index anomaly** and retained
  exactly. Every *declared count* remains consistent.

## Original x86 code candidates from the Ghidra corpus

The repository's existing decompilation export gives direct corroboration
for several **writers/serializers**. These are *guest code addresses* in the
SHA-pinned German X-32 image, **not** the byte offsets in the text files:

| Guest function | Observed Ghidra evidence | Conservative interpretation |
| --- | --- | --- |
| `0x000b1bb0` | Calls a formatting routine, transforms its buffered result, then calls `0x000a7a1c` with a handle and byte buffer | Formatted-output wrapper; calling convention/return value still unverified. |
| `0x000b2560` | Formats `FLIGHT`, iterates **20 `PYLON` entries**, emits `WP_ROUTE` and `TARGET` records | Flight/route text serializer, with likely weapon-index array and route pointers. |
| `0x000b1f80` | Formats the `MOB PL`, `MOB SH` and `MOB GB` record variants; subsequently emits per-mobile pilot/position data | Mobile serializer with a tentative 0/1/2 class switch; need typed ABI verification. |
| `0x000b1c34` | Emits `NUM_MOBILES`, walks a linked list via a `+4` next pointer and calls `0x000b1f80` | Mobile-list serializer; seems to assign the observed ordinal before writing. |
| `0x0007d2f8` | Creates `custom.cfg` and emits a two-mission layout, 20-pylon flight blocks and base/timeslot templates | Custom scenario writer, **not** necessarily the general original `MISSION.CFG` loader. |

Evidence paths in the existing ignored export include
`local/ghidra-entries-recovery/custom-storage-export/functions/000b/0x000b2560.c`
and its neighbors. Ghidra currently uses a **generic x86 ABI**, so these are
candidate signatures and structure hints, not yet validated source-level
types. The relationship between 20 `PYLON` rows and the original writer's
20-iteration loop is particularly useful. The actual **read/parse** routines
still need to be isolated; finding a string reference that writes a field
does not establish the function that reads it.

Provisional storage anchors for the next typed decompilation pass: the
`0x000b2560` output loop indexes a weapon selector from the flight object's
byte range **`+0x55` through `+0x68`** (20 entries), and tests a route
count through the object around **`+0x1c`**. The `0x000b1f80` mobile writer
switches on a byte at **`+0x0b`** (observed branches 0, 1 and 2).
`0x000b1c34` traverses mobile objects using a next pointer at **`+4`**
and writes a short ordinal at **`+0x13`**. These offsets are literal Ghidra
observations; they have not yet been cross-validated against a live memory
layout, and Ghidra's synthetic types must not be treated as C++ ABI types.

### Campaign file grammar

`NUM_TARGET_AREAS` governs `SSNAME` rows with quoted place names. `NUM_MOBILES`
governs `MOB` rows. In both supplied campaign snapshots, each `MOB` is followed
by exactly one `PIL` row, and the sixth argument of `MOB` agrees with the
zero-based ordinal throughout. Additional top-level `WORLD_STATUS`,
`DIFFICULTY`, `PERFORMANCE`, `NUM_ALLEGIANCES` and numeric-only records are
preserved but not interpreted.

The **structural** relationships above were cross-checked across all records.
They are reported as observational `block_checks`, rather than asserted as
compatibility rules for every edition. `--strict` currently rejects mismatched
*explicit declarations and route lengths*, not inferred relationships or
the single original waypoint-index irregularity.

## Reproduce or export a specific original record

Use files supplied separately by the caller:

```sh
# Read-only inventory with declared counts, route lengths and anomalies:
python3 tools/mission_records.py /path/to/EF2000/MISSION.CFG --strict
python3 tools/mission_records.py /path/to/EF2000/NAVIG.NAV --strict
python3 tools/mission_records.py /path/to/EF2000/CAMPAIGN.CG --json

# Recover one exact mission with its ordered, line-numbered child directives:
python3 tools/mission_records.py /path/to/EF2000/MISSION.CFG --block MISSION:516

# Entire file, grouped or flat, including unknown tokens and original lines:
python3 tools/mission_records.py /path/to/EF2000/MISSION.CFG --blocks-json
python3 tools/mission_records.py /path/to/EF2000/MISSION.CFG --records-json

# Synthetic grammar/validation fixtures; safe without commercial assets:
python3 -m unittest tests.test_mission_records -v
```

All operations only read their input. Structured JSON is printed on stdout,
not silently installed or saved elsewhere. Raw records retain source text;
tokenized arguments are best-effort (quoted strings are decoded by `shlex`,
and localized DOS character encoding is not fully classified). No mission
files are rewritten, and no editor/modding UI is planned yet.

## Follow-up reverse engineering

1. Find game code that reads the header declarations (`NUM_MISSIONS`,
   `NAVIG_ROUTE`, `NUM_MOBILES`) using the existing Ghidra string references
   and file-open call traces. Identify its tokenizer, record dispatch and
   in-memory allocation/copy patterns, then name the routines.
2. Recover the **coordinates, route units, waypoint action enum, mission
   status flags, target references and aircraft/pylon enum** by comparing
   original reader code, live mission selection and controlled files on a
   writable overlay. Never write experimental bytes into original assets.
3. Correlate `MOB` IDs, flight/campaign relationships and mission IDs against
   observed original runtime structures, not just plausible integer matches.
   Compare original TACTCOM's mission editor output with a known before/after
   snapshot if available.
4. Support additional editions only after checking exact file fingerprints
   and resolving differences in grammar/encoding. A mission editor or
   round-trip writer is intentionally out of scope for this milestone.
