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
