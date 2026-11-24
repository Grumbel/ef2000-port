# EF2000 releases, modding tools and reference installations

This is a research map for the port, **not** an endorsement of any third-party
game download. It distinguishes direct observations of our supplied DOS
TACTCOM installation from historical/community reports and work still to be
verified. The port needs legally supplied original assets; none of the linked
projects' proprietary executable or data files are incorporated into Git.

## Release and graphics variants

Release names are unfortunately not interchangeable. The following distinctions
follow the contemporary [CombatSim v2.0 review](https://www.combatsim.com/archive/htm/htm_arc2/ef22-rev.htm)
and the [EF2000 release/patching primer](https://www.tales-from-darkenedroom.com/post/ef2000-updated-part-1-a-primer):

| Release or upgrade | Platform / renderer | Relevance to the port |
| --- | --- | --- |
| EF2000 original | DOS, software-rendered | Earliest baseline; patches and data may differ from TACTCOM. |
| TACTCOM expansion | DOS, software-rendered | Adds missions, multiplayer and an air-to-air scenario editor; basis for GFX+. |
| EF2000: Evolution | DOS | Combined original + TACTCOM release. |
| **GFX+** 3Dfx / Rendition patches | DOS with dedicated Voodoo or Vérité graphics paths | Upgrade for **TACTCOM/Evolution**, not the unexpanded original or Super EF2000; separate language variants were distributed. |
| **Super EF2000** / Special Edition | Windows 95, DirectX | Separate Windows redevelopment, **not** the DOS GFX+ executable. |
| **EF2000 V2.0** (1997 compilation) | DOS software, DOS 3Dfx, DOS Rendition **and** Windows 95 variants | The installer offers distinct targets; “v2.0” alone does not identify the renderer or binary. |

The [EF2000 installation/patching guide](https://www.tales-from-darkenedroom.com/post/ef2000-updated-part-2-installation)
documents applying the `tactpat.exe` GFX+ patch to TACTCOM and selecting the
3Dfx option. Its workflow is useful for identifying the actual accelerated
executable and required files; it is not a license to redistribute them.

### Why 3Dfx is interesting, but not a drop-in renderer

[EF2000 Reloaded's original SimHQ announcement](https://simhq.net/forum/ubbthreads.php/topics/3830314/all/EF2000_Reloaded)
describes a modified DOSBox with low-level Voodoo emulation (based on kekko's
DOSBox work), plus a launcher and VFX1/TrackIR translation. The detailed
[Reloaded/DOSBox guide](https://www.tales-from-darkenedroom.com/post/ef2000-updated-part-3-ef2000-reloaded-dosbox)
explains that GFX+ **statically links its Glide implementation** into the DOS
executable. As a result, there is no separate, dynamically replaceable
`GLIDE2X.OVL` call boundary for an ordinary Glide-to-OpenGL wrapper to catch.
It instead communicates with 3Dfx hardware at a lower level. The
[DOSBox-X Voodoo guide](https://dosbox-x.com/wiki/Guide%3ASetting-up-3dfx-Voodoo-in-DOSBox%E2%80%90X)
also distinguishes low-level Voodoo emulation from Glide passthrough.

**Implication:** GFX+ is a valuable *reference implementation* of a hardware
accelerated EF2000 render path, not an immediate SDL3 GPU backend. We must
first identify the accelerated DOS executable and its calling conventions,
trace the hardware-facing memory/port activity and texture/geometry inputs,
and determine whether a stable high-level primitive boundary exists. A native
SDL3 GPU/OpenGL/Vulkan renderer may then be possible, while preserving the
software path as a correctness reference. Full Voodoo hardware emulation is
another option, but has different complexity/performance trade-offs. Neither
route has been implemented or benchmarked in this project.
The [GFX+ patch investigation](gfxplus-rendering.md) now records the
archived Windows updater's fingerprint and Glide/3Dfx string addresses,
without claiming to have extracted or run a patched EF2000 executable.

The [Reloaded discussion linked by the user](https://simhq.net/forum/ubbthreads.php/ubb/printthread/Board/215/main/377763/type/thread)
and [SimHQ announcement](https://simhq.net/forum/ubbthreads.php/topics/3830314/all/EF2000_Reloaded)
are useful starting points for build/compatibility details. The
[EF2000 Reloaded guide](https://www.tales-from-darkenedroom.com/post/ef2000-updated-part-3-ef2000-reloaded-dosbox)
also describes running the accelerated DOS edition with appropriate Voodoo
emulation in DOSBox-X or DOSBox Staging; its modern OpenGL output does **not**
mean the original GFX+ executable is emitting OpenGL calls.

## Existing tools, community work and intended use

| Project / tool | What it is known to do | Source / caveat |
| --- | --- | --- |
| [tfx3view](https://github.com/Grumbel/tfx3view) and its `tfxdat` | DID asset archive extraction / TFX-family 3D-model viewing | Useful archive-format precedent. Its viewer is **not** an EF2000 menu or mission editor; see [GUI asset research](gui-assets.md). |
| [EFUTIL (The Bayou)](https://simhq.net/forum/ubbthreads.php/topics/3835020/re-ef2000-reloaded) | Legacy VB3 **campaign aircraft-type swapper**, e.g. changing aircraft assigned to campaign slots | **Not** a general mission-file extractor, despite the name. Old download locations may no longer work. |
| [ReViVe / REVIVE32](https://www.combatsim.com/memb123/archive/htm/htm_arc2/evolute.htm) | Adjusts/revives campaign aircraft and bases, backs up the campaign | Historical Windows/VB-era utility; portability and current availability unverified. |
| [Super ReViVe](https://simhq.net/forum/ubbthreads.php/topics/3830314/all/EF2000_Reloaded) | More extensive campaign editing; included with Reloaded | Brett Luck's utility; historical binary/tooling, not audited by this port. |
| [Strategic Command(er) for EF2000](https://simhq.net/forum/ubbthreads.php/topics/3458818/3) | Community campaign/scenario planner/editor with units, waypoints, alliances and targets | Discussion describes particular DOS v2.0 compatibility constraints; do not assume arbitrary campaign transfers work. |
| [TACTCOM built-in A2A editor](https://www.tales-from-darkenedroom.com/post/ef2000-updated-part-1-a-primer) | Air-to-air training/scenario setup | Part of the expansion, not a standalone extraction tool. |
| [EF2000 Reloaded](https://simhq.net/forum/ubbthreads.php/topics/3830314/all/EF2000_Reloaded) | Frontend + specialized DOSBox, optional Voodoo/TrackIR emulation, campaign management and custom campaign creator | Does **not** include the game; it also bundles Super ReViVe and basic campaign editing. |
| [rdeprera/EF2000](https://github.com/rdeprera/EF2000) | Third-party DOS game mirror with DOSBox / DOSBox-X configurations and a v2.0 manual | Contains commercial game files; useful for **researching variant fingerprints**, not a source to vendor into the port. |
| [EF2000 RETRO for Falcon BMS](https://combatace.com/forums/topic/99483-ef2000-typhoon-retro-mod-for-falcon-bms-437/) | Modern Eurofighter-themed add-on for **Falcon BMS** | Separate simulation/mod, **not** a modified DID EF2000 engine or compatible asset format. |

## Mission and campaign data: already human-readable

This is an observation from the supplied **German DOS TACTCOM** installation,
not an inference from a third-party utility. Several important files are
**loose ASCII/text records**, not packed members that first need a DAT extractor:

| File | Direct observation | What we can already recover |
| --- | --- | --- |
| `MISSION.CFG` | 455,280 bytes; `NUM_MISSIONS 746` followed by exactly **746** `MISSION` blocks | `GROUP`, `3DP`, `WP_ROUTE`, `WAYPOINT`, `TARGET` lines; 542 group lines and 3,178 waypoint lines. |
| `NAVIG.NAV` | 1,317,326 bytes; `NUM_BASE_NAVIGATIONS 1118` and exactly **1,118** `BASE_NAVIGATION` records | Navigation routes and waypoint records. |
| `CAMPAIGN.CG` | 1,006,160 bytes; begins with `DIFFICULTY`, `PERFORMANCE`, `WORLD_STATUS`, `NUM_TARGET_AREAS 543` | A saved/generated campaign's world status and named targets/airbases. |
| `CONFLICT.CG` | 1,002,607 bytes; similar campaign text, `NUM_TARGET_AREAS 542` | Another campaign/world template or snapshot; not byte-equivalent to `CAMPAIGN.CG`. |
| `CAMPBRF.BRF` | 109-byte `#`-delimited text record in this installation | Briefing fields (`MISSION`, `TIME`, `TYPE`, `FLIGHT`, `TARGET` etc.). |

These are examples from **one installation**; campaign state changes during
play, and file sizes/counts are not release invariants. `MISSION.CFG` starts
with a comment identifying a *Conflict generator Mission file*, and individual
missions include labeled groups, world-coordinate triples, route lengths and
waypoints. `NAVIG.NAV` similarly identifies a *Conflict generator navigation
file*. The field names and counts are observed; the coordinate systems,
relationships, units, mutability and complete grammar have **not** yet been
reconstructed. These plain-text records are distinct from the player
preference file `EF2000.CFG`.

You can inspect your **own** install without modifying it:

```sh
rg '^NUM_MISSIONS|^MISSION |^GROUP |^WAYPOINT ' /path/to/EF2000/MISSION.CFG
rg '^NUM_BASE_NAVIGATIONS|^BASE_NAVIGATION ' /path/to/EF2000/NAVIG.NAV
rg '^NUM_TARGET_AREAS|^SSNAME ' /path/to/EF2000/CAMPAIGN.CG
```

Community [Strategic Commander discussion](https://simhq.net/forum/ubbthreads.php/topics/3458818/3)
also identifies `CAMPAIGN.CG`, `MISSION.CFG` and `CAMPBRF.BRF` as a transferable
campaign trio, with significant version/compatibility caveats. This explains
why campaign-oriented tools exist. **A read-only structured parser/exporter
would be useful** for the decompilation effort, independently of writing a
modding editor. The `DID*.DAT` resources still have unnamed hashed entries;
our existing [asset extractor](gui-assets.md) handles known archive paths,
not this mission grammar. Parsing, editing and round-tripping should be
separate, evidence-driven steps.

## Cross-version reference: `rdeprera/EF2000`

The [repository](https://github.com/rdeprera/EF2000) describes itself as
“EF2000 2.0 TACTCOM - DOS”; it offers `main`, `dosbox`, and `dosbox-x` branches.
We compared **`main`** against the supplied German installation using SHA-256
and the port's `DIDArchive` reader. These are separate copies; no external
game files were added to Git.

| File | Supplied German TACTCOM | `rdeprera/EF2000` `main` | Result |
| --- | --- | --- | --- |
| `_EF2000.EXE` | 1,684,561 bytes; SHA-256 `f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd` | 1,682,129 bytes; SHA-256 `ff245c9f812d4113c4a2a8fe60fb9a0e45687bc6ae73c19e4c23c32bccd2ec49` | **Different executable**, requires separate layout/function analysis and explicit opt-in fingerprint support. |
| `DID1ST.DAT` | 14,301,836 bytes; 580 indexed entries | 14,301,821 bytes; 580 indexed entries | All 580 hashes present in both; **575 decoded members byte-identical**. |
| `DID.DAT` | 51,528,982 bytes; 8,416 indexed entries | 53,124,034 bytes; 8,469 indexed entries | 7,931 hashes in common; **7,884 common decoded members identical**, 538 mirror-only and 485 German-only. |
| `gui/mainmenu.gui` | Found in both DAT archives | Found in both DAT archives | The equivalent script member is **byte-identical** in each archive across installations. |

The archive comparisons are based on the hash index and decoded member bytes,
**not recovered original filenames** for every entry. The copy called
“German” is known from the supplied executable's live menu; the mirror's
**English language is plausible but not yet proven** by this inspection.
Identical menu scripts and `VER` bytes do not prove the two executables or
localization tables are interchangeable. Differences may include patch level,
game logic, assets, language, or installation state; isolate them before
attributing changes to localization. The mirror's README suggests generic
3Dfx wrapper configurations, but that alone does not establish it contains
the actual GFX+ executable or supports Glide passthrough.

## Cross-version reference: EF2000 V2.0 for Windows 95

A Windows-only V2.0 installer disc is available locally at
`../EF2000_V2.0-Twilight/EF2000/`. It has no DOS or GFX+ executable. The game
executable is `PROGRAM/superw.dat` inside `DATA.Z` (InstallShield 3, unpacked
with nixpkgs `unshieldv3`): 1,952,768 bytes, linked 1997-05-13, SHA-256
`35c949cbe86485f38464254a08e2af6f8c9e0e1534c3adcdc1ed92ae4f1c6436`. It is an
unencrypted Watcom PE without symbols but with 63,008 base relocations.
`superw.exe` is only the launcher.

- **Code:** of the 4,309 tracked German DOS functions, 1,931 (28 % of bytes)
  have the same instructions with addresses masked, and 3,123 (75 %) are at
  least 60 % similar. Random pairs score at most 0.45, and shared global
  addresses agree in 98.8 % of references. The 60-80 % band and DOS addresses
  from `0x100000` up need manual review.
- **Pointers:** the relocations confirm 8,510 distinct DOS addresses as
  pointers. Absence from that set does not prove a constant.
- **Self-modifying code:** the examined cases are rewritten as data access.
  `0xc4755` reads three globals instead of patched immediates, and `0xc46bc`
  and `0xdc34e` write globals instead of code. The code section is read-only.
- **Data:** 7,165 of the 7,571 distinct decoded `DID.DAT` members are
  byte-identical to the English DOS ones.

Use it as a reading aid only. It is a later build, so the German DOS
executable stays authoritative and the Windows build is not a test oracle.
Supporting this edition is deferred until the DOS decompilation is finished.
The function and address maps and the scripts that generate them are in the
ignored `local/win-v2/`; its `README.txt` describes their use.

## Cross-version reference: EF2000 V2.0 for DOS

`tools/didpatch.py` rebuilds the DOS V2.0 executable from the English
TACTCOM `_EF2000.EXE` and the archived GFX+ updater; see
[gfxplus-rendering.md](gfxplus-rendering.md). It is 1,675,537 bytes, still
FlashTek X-32 (payload at file offset `0x54a1`, as in the German and English
builds) and identifies itself as `EF2000 V2.0 for DOS $Revision: 3.32 $ by
Russ on May 02 1997`. It is a recompile, not a binary patch, so addresses
shift throughout.

The Windows matcher, run unchanged against the V2.0 payload, gives:

- **Code:** of the 4,309 tracked German functions, 2,343 (32 % of bytes)
  have the same instructions with addresses masked, 3,071 (63 %) are at
  least 80 % similar and 3,439 (80 %) at least 60 % similar. This is close
  to the Windows result (28 % exact). V2.0 is not a minor patch of TACTCOM.
- **Consistency:** call targets of aligned calls agree in 99.7 % of
  repeated references and global addresses in 99.0 %. 1,744 functions
  (526 KB) are confirmed by at least two shared globals.
- **Limits:** the build has no relocations. Function starts come only
  from direct call targets, and the global map is a value-range test, not
  proof of a pointer. Random pairs reach 0.84 similarity (Windows: 0.45), so
  check every match below 0.9 by hand.

Use the two V2.0 builds together. The Windows build answers
pointer-versus-constant and self-modifying-code questions through its
relocations. The DOS build has the same extender, memory layout and
hardware code as the German reference. Both maps, the extracted payload and
the scripts are in the ignored `local/dos-v2/`, which has a `README.txt`.

### Version plan

| Build | Format | Role now |
| --- | --- | --- |
| German TACTCOM DOS | X-32 | Authoritative; the decompilation target |
| English TACTCOM DOS (`rdeprera`) | X-32 | Input to the V2.0 update; layout in [executable.md](executable.md) |
| V2.0 DOS | X-32 | Reading aid (`local/dos-v2`) |
| V2.0 Windows 95 | PE | Reading aid (`local/win-v2`) |
| GFX+ 3Dfx / Rendition | LE under CauseWay | Deferred to the hardware-renderer design |

The DOS decompilation stays first (rev 5). The V2.0 maps only help with
reading. Neither V2.0 build is a test oracle. GFX+ work waits until the
renderer needs a hardware boundary. Its next steps are in
[gfxplus-rendering.md](gfxplus-rendering.md): an LE/CauseWay loader or a
separate Ghidra project, then a low-level Voodoo trace in DOSBox-X.

## Follow-up experiments (not yet completed)

1. Preserve each independently obtained executable and archive with its own
   SHA-256, release provenance, language, patch level and renderer target.
   Import each binary into a **separate Ghidra project**; correlate stable
   functions/data rather than transplanting German executable addresses.
2. Find the actual DOS **3Dfx GFX+** executable and record its patch provenance;
   compare the loader, render dispatch, resources, surface dimensions and
   hardware-register traces with the software version. Test in low-level
   Voodoo emulation as a visual oracle before designing native accelerated
   output.
3. Recover a read-only typed representation of `MISSION.CFG` and `NAVIG.NAV`
   using their record counts and cross-file references as validation. Compare
   a fresh campaign, Quick Combat mission, and TACTCOM editor output; do not
   assume they share all record types or implement write-back yet.
4. Search for localized text/fonts and renderer-independent geometry across
   archive differences; cross-check with the existing [GUI asset map](gui-assets.md)
   and original guest's dynamic resource loads.
