# EF2000 (MS-DOS) — Keyboard Shortcuts

Reference for **Digital Image Design's EF2000 (1995)**, its patched DOS **2.0–2.05** releases, and the DOS **TACTCOM** expansion. It does *not* cover the later *Eurofighter Typhoon* game, Super EF2000 for Windows, or DOSBox's own emulator shortcuts.

This list combines the original 288-page DOS manual and six keyboard diagrams with a contemporary May 1996 keyboard summary, block-2.x change notes, a 1997 TACTCOM joystick mapping, and contemporary reviews. The original manual is the baseline. Version-specific additions are indicated. **†** means the binding is documented in a secondary period source but remains unverified against an exact game executable. Commands are grouped by function, not physical key position.

**Notation:** `KP` = numeric keypad. `+` on the main keyboard often means the physical `=`/`+` key. `<` and `>` in old documentation refer to the comma/period key positions, which vary by national layout. In some modes keys have different actions.

## 1. Flight controls and engines

| Shortcut                                        | Action                                           |
|-------------------------------------------------|--------------------------------------------------|
| `Up` / `Down`                                   | Pitch control in keyboard flight mode            |
| `Left` / `Right`                                | Roll control in keyboard flight mode             |
| `,` / `.` (`<` / `>` in original documentation) | Rudder left/right; nose-wheel steering on ground |
| `Shift+,` / `Shift+.`                           | Left/right toe brake                             |
| `-` or `KP -`                                   | Reduce throttle                                  |
| `+` / `=` or `KP +`                             | Increase throttle                                |
| `KP *`                                          | Afterburners on                                  |
| `KP /`                                          | Afterburners off                                 |
| `[`                                             | Toggle left engine                               |
| `]`                                             | Toggle right engine                              |
| `G`                                             | Landing gear up/down                             |
| `W`                                             | Wheel brakes                                     |
| `B`                                             | Airbrake toggle                                  |
| `Shift+B`                                       | Deploy drag/braking chute                        |
| `Alt+B`                                         | Release/jettison drag chute                      |
| `L`                                             | Automatic leveller/flight recovery               |
| `Shift+Esc`                                     | Eject                                            |

## 2. Autopilot, navigation, landing, refuelling

| Shortcut                       | Action                                                  |
|--------------------------------|---------------------------------------------------------|
| `A`                            | Autopilot on/off                                        |
| `Alt+A` once                   | Autopilot mode 1: waypoint following                    |
| `Alt+A` twice                  | Autopilot mode 2: heading, altitude and speed           |
| `Alt+A` three times            | Autopilot mode 3: target tracking                       |
| `Alt+A` four times             | Autopilot mode 4: autothrottle                          |
| `C`, in autopilot mode 3       | Cycle tracked target                                    |
| `+` / `-`, in autopilot mode 4 | Adjust commanded speed (also available through the MFD) |
| `Shift+W`                      | Next waypoint                                           |
| `Alt+W`                        | Previous waypoint                                       |
| `I`                            | Instrument landing system (ILS) mode                    |
| `'`                            | Deploy/stow refuelling probe                            |
| `Shift+'`                      | Refuelling HUD mode                                     |

## 3. Weapons and target selection

| Shortcut                         | Action                                                                      |
|----------------------------------|-----------------------------------------------------------------------------|
| `Enter` / `KP Enter`             | Cycle air-to-air weapons; AA HUD mode                                       |
| `Backspace`                      | Cycle air-to-ground weapons; AG HUD mode                                    |
| `Space`                          | Fire selected weapon/release bomb                                           |
| `C`                              | Cycle available targets (also context-dependent in autopilot/padlock modes) |
| `T`                              | TIALD laser-bombing sight                                                   |
| `Right Shift+Up/Down/Left/Right` | Slew TIALD IR/TV aiming image                                               |
| `Shift+J`                        | Jettison external fuel tanks                                                |
| `Alt+J`                          | Jettison air-to-ground stores                                               |
| `Shift+Enter`                    | Select cannons directly (added by DOS patches)                              |
| `X`                              | Enemy lock/nearest-threat action; exact behaviour varies by patch           |
| `Z`                              | Nearest-threat selection †                                                  |
| `F`                              | Eyeball radar/view current track †                                          |

`Shift+Enter` and direct radar controls were introduced after the original release. Block 2.01 notes explicitly call `X` the nearest-threat lock, but other period controller mappings describe `X` as cycling locks and `Z` as nearest threat.

## 4. Radar, displays, countermeasures and avionics

| Shortcut               | Action                                          |
|------------------------|-------------------------------------------------|
| `R`                    | Select/toggle radar                             |
| `D`                    | Select/toggle DASS defensive aids               |
| `E`                    | Toggle electronic countermeasures (ECM/jamming) |
| `J`                    | JTIDS tactical information display              |
| `M`                    | Moving map                                      |
| `V`                    | Night vision toggle                             |
| `U`                    | Cycle HUD brightness/contrast                   |
| `Alt+H`                | HUD declutter                                   |
| `Shift+A`              | Radar/DASS overlay in HUD-only view             |
| `Shift+M`              | Miniature JTIDS repeater on HUD (block 2.0+)    |
| `Insert`               | Release chaff                                   |
| `Delete`               | Release flares                                  |
| `Home` / `End`         | Radar range up/down (block 2.x)                 |
| `Page Up`              | Change radar azimuth (block 2.x)                |
| `Page Down`            | Change radar mode (block 2.x)                   |
| `Alt+Home` / `Alt+End` | DASS range adjustment †                         |

### Alternate extended-keyboard commands (DOS block 2.01+)

| Shortcut | Action                |
|----------|-----------------------|
| `Alt+1`  | Chaff                 |
| `Alt+2`  | Flares                |
| `Alt+3`  | Radar scan range up   |
| `Alt+4`  | Radar scan range down |
| `Alt+5`  | Radar azimuth change  |
| `Alt+6`  | Radar mode change     |
| `Alt+7`  | DASS range up         |
| `Alt+8`  | DASS range down       |

These eight mappings are explicitly enumerated in the block-2.01 additions.

## 5. Cockpit and external views

| Shortcut                   | Action                                                                     |
|----------------------------|----------------------------------------------------------------------------|
| `F1`                       | Cycle cockpit with HUD / HUD-only                                          |
| `Shift+F1`                 | Full VR cockpit with MFDs                                                  |
| `F2`                       | Wide-angle virtual cockpit (in later v2 builds, primarily in padlock mode) |
| `F3`                       | Check six / look behind                                                    |
| `F4`                       | Cycle player's external views                                              |
| `Shift+F4`                 | Satellite/God's-eye view                                                   |
| `F5`                       | Cycle wingman views                                                        |
| `F6`                       | Player fly-by (including nearest missile in some cycles)                   |
| `Shift+F6`                 | Target/browse-plane fly-by                                                 |
| `F7`                       | Cycle target views                                                         |
| `F8`                       | Cycle weapon/weapon-eye views                                              |
| `F9`                       | Cycle browse-plane views                                                   |
| `F10`                      | Virtual cockpit padlock (repeated presses can switch targets/missiles)     |
| `F11`                      | Wingman padlock/wingman's threat (v2/TACTCOM)                              |
| `F12`                      | Missile padlock (v2/TACTCOM)                                               |
| `KP 7`                     | Look left in 60° increments                                                |
| `KP 8`                     | Restore forward cockpit/HUD view                                           |
| `KP 9`                     | Look right in 60° increments                                               |
| `Shift+Up/Down/Left/Right` | Pan cockpit/external camera                                                |
| `Shift+KP 7`               | Zoom external view in                                                      |
| `Shift+KP 1`               | Zoom external view out                                                     |
| `Shift+C`                  | Cycle browse aircraft or wingman, depending on current view                |
| `Shift+X`                  | Previous browse plane †                                                    |
| `Shift+F2`                 | Track view †                                                               |
| `Shift+F7`                 | Browse view †                                                              |
| `Shift+F`                  | View filter †                                                              |
| `Shift+V`                  | Smart view †                                                               |

**Version distinction:** The original printed 1995 diagram leaves `F11` and `F12` blank. Their padlock actions appear in later 2.x/TACTCOM references. Num Lock should generally be off for keypad zoom. `Shift+Arrow` pans the view generally, while **Right Shift+Arrow** slews the TIALD sight in that mode.

## 6. MFDs, panels and context-sensitive softkeys

| Shortcut | Action                         |
|----------|--------------------------------|
| `KP 1`   | MFD 1: left display            |
| `KP 2`   | MFD 2: centre display          |
| `KP 3`   | MFD 3: right display           |
| `KP 4`   | Warning panel                  |
| `KP 5`   | IRST screen                    |
| `KP 6`   | Artificial horizon and compass |
| `KP 0`   | Briefing summary               |
| `KP .`   | Full game map (`Esc` exits)    |

The original *Keyboard MFD Controls with Ctrl* diagram highlights **all 17** of the following context-sensitive MFD shortcuts:

`Ctrl+Q`, `Ctrl+E`, `Ctrl+I`, `Ctrl+O`, `Ctrl+P`,
`Ctrl+A`, `Ctrl+D`, `Ctrl+F`, `Ctrl+G`, `Ctrl+H`, `Ctrl+J`, `Ctrl+K`, `Ctrl+L`,
`Ctrl+C`, `Ctrl+B`, `Ctrl+N`, `Ctrl+M`.

These operate functions on the selected MFD. The letter is **not** a fixed global aircraft-system command: the effect depends on which MFD and submenu are active. The exact softkey-to-screen-position correspondence remains to be verified against the executable.

## 7. Wingmen, radio and multiplayer

| Shortcut              | Action                                            |
|-----------------------|---------------------------------------------------|
| `Tab`                 | Open/close wingman-command menu                   |
| Main keyboard `1`–`9` | Wingman menu choices / execute selected command   |
| Main keyboard `0`     | Previous wingman-command menu                     |
| `Y`                   | Yes/confirm                                       |
| `N`                   | No/deny                                           |
| `Shift+L`             | Recall last radio message (patched versions)      |
| `S`                   | Send multiplayer text message †                   |
| `Shift+N`             | Network-player names †                            |
| `Shift+S`             | Network-player labels in some multiplayer modes † |

Wingman menu numbers are contextual, not fixed commands. Period references disagree about `Shift+N` versus `Shift+S` for multiplayer labels; verify against the intended binary. In normal campaign flight `Shift+S` instead skips ahead to the next event.

## 8. Game, time, graphics and capture

| Shortcut                            | Action                                                                       |
|-------------------------------------|------------------------------------------------------------------------------|
| Backquote/tilde key (`` ` `` / `~`) | In-flight setup menu                                                         |
| `Esc`                               | Close setup or exit current modal map/view                                   |
| `Shift+Q`                           | Quit to DOS                                                                  |
| `P`                                 | Pause (some cockpit functions remain usable)                                 |
| `Shift+T`                           | Accelerated simulation time                                                  |
| `Shift+S`                           | Skip to next significant event; context-dependent around landings/refuelling |
| `Alt+D`                             | Graphics detail selection                                                    |
| `Alt+R`                             | VGA/SVGA resolution toggle                                                   |
| `Shift+D`                           | Cycle campaign difficulty (block 2.01+)                                      |
| `Alt+F7`                            | Decrease gamma                                                               |
| `Alt+F8`                            | Increase gamma                                                               |
| `Shift+F12`                         | Capture screenshot as `.LBM` (later versions)                                |
| `Alt+V`                             | Toggle onboard camera/video mode †                                           |

`Alt+R` is the VGA/SVGA key, confirmed by the original printed diagram and a contemporary summary; a search-index OCR description incorrectly calls it `Alt+E`.

## 9. Version and completeness audit

| Source/build              | Result                                                                                                             |
|---------------------------|--------------------------------------------------------------------------------------------------------------------|
| Original 1995 manual      | Cross-checked against manual key summary, view summary, and six normal/Shift/Alt/Ctrl/extended keyboard diagrams   |
| May 1996 keyboard summary | Supplies extra radar, view, network, and target commands missing from printed diagrams; some entries are ambiguous |
| DOS block 2.0             | Adds `Shift+M` JTIDS repeater and direct radar range/mode controls                                                 |
| DOS block 2.01            | Explicit `Alt+1`–`Alt+8`, `Shift+D`, nearest-threat `X`, and DASS-range controls                                   |
| DOS block 2.02–2.04       | Checked change notes; no separate new key list identified                                                          |
| TACTCOM / 2.x             | Confirms `F11`/`F12` padlock, screenshots, and further view and communications bindings                            |

### Remaining executable-level checks

1. Verify target-selection semantics for `Z`, `X`, and `Shift+X` by version.
1. Verify lesser-documented camera commands `Shift+F`, `Shift+V`, `Shift+F2`, `Shift+F7`, and `Alt+V`.
1. Resolve multiplayer `S`, `Shift+N`, and `Shift+S` in each DOS version.
1. Resolve `Alt+Home`/`Alt+End` DASS controls relative to the confirmed block-2.01 `Alt+7`/`Alt+8` alternatives.
1. Map all 17 `Ctrl+letter` MFD combinations to actual softkey positions and check conditional behaviour.
1. Inspect the exact `_EF2000.EXE` key dispatch table or run interactive tests to rule out undocumented handlers.

**Completeness claim:** This is a consolidated reference to the *documented* keyboard commands of EF2000 DOS and its updates, including period secondary discoveries. It is not yet proven exhaustively complete at executable level. The `†` entries should be considered a verification checklist rather than hard requirements for a native port.

## Sources

1. [Original EF2000 keyboard diagrams (PDF)](https://smetisteher.cz/soubory/ef2000_1.pdf).
1. [EF2000 DOS manual, 288 pages](https://www.scribd.com/document/665935833/Ef-2000-Manual-Dos-En).
1. [Gary A. Lucero, *EF2000 Keyboard Summary*, May 1996 (EFKEYS.ZIP, Word 6)](https://apajalista.net/tiedostot/?tiedoston_id=25072&tiedoston_nimi=EFKEYS.ZIP&tiedoston_osasto=Pelit+%2F+DOS-pelit+%2F+Faqit%2C+muut+tekstit).
1. [EF2000 Intelligence Report Part III: patch/block revisions](https://www.combatsim.com/archive/htm/htm_arc2/ef2k3.htm).
1. [Contemporary EF2000/TACTCOM Thrustmaster mappings, updated January 1997](https://www.combatsim.com/archive/htm/htm_arc2/eftm.htm).
1. [TACTCOM contemporary review](https://www.combatsim.com/printer.php?action=review&id=397&page=1).
1. [GameFAQ from February 1996](https://gamefaqs.gamespot.com/pc/197213-ef2000/faqs/1969).
1. [EF2000 Reloaded discussion: gamma, screenshot, keypad zoom](https://simhq.net/forum/ubbthreads.php/ubb/printthread/Board/215/main/377763/type/thread).
