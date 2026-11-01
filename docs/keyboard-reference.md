<!-- SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# EF2000 keyboard reference and current port behavior

## Source, scope and confidence

The original-era EF2000 keyboard-chart scan at
<https://smetisteher.cz/soubory/ef2000_1.pdf> documents normal, Shift,
Alt, Ctrl/MFD and extended-keypad layouts. This is a **manual-derived command
reference**, not a disassembly-proven map of all input branches in the supplied
German Tactcom `_EF2000.EXE`. Version-specific differences are possible. The
chart is copyright its respective rights holder and is linked rather than
copied. The English labels below paraphrase the printed chart.

Blank or unmarked keys on the chart are omitted: this is every **labeled
function**, not a claim that all other key combinations are ignored by the
executable. Keys that the host steals from the game are listed separately.

## Unmodified keyboard

| Key(s) | Game action |
|---|---|
| ` (grave/backtick) | Setup menu |
| 1 through 9 | Wingman menu choice 1 through 9 |
| 0 | Previous wingman menu |
| - / = or + | Decrease / increase throttle |
| Backspace | Cycle air-to-ground weapons |
| Q | Wingman commands |
| W | Wheel brake |
| E | Electronic countermeasures (ECM) |
| R | Select radar |
| T | Select TIALD |
| Y / N | Yes / No |
| U | HUD brightness |
| I | Select ILS |
| P | Pause game |
| [ / ] | Left / right engine |
| A | Autopilot |
| D | Select DASS |
| G | Landing gear up/down |
| J | Select JTIDS |
| L | Auto-level |
| ' (apostrophe) | Refuelling nozzle |
| Enter | Cycle air-to-air weapons |
| C | Cycle targets |
| V | Night vision |
| B | Air brake |
| M | Maps |
| , / . | Rudder or nosewheel left / right |
| Space | Fire selected weapon |

## View function keys

| Key | Game action |
|---|---|
| F1 | Cycle HUD views |
| F2 | Wide-angle cockpit |
| F3 | Check six / rear view |
| F4 | Player external views |
| F5 | Wingman external views |
| F6 | Fly-by views |
| F7 | Target views |
| F8 | Weapons views |
| F9 | Browse aircraft views |
| F10 | Padlock views |
| F11, F12 | No normal-mode game assignment shown in chart |

## Shift combinations (changed assignments)

| Key(s) | Game action |
|---|---|
| Shift+Esc | Eject |
| Shift+F1 | VR/MFD cockpit |
| Shift+F4 | Satellite view |
| Shift+F6 | Browse-target fly-by |
| Shift+Q | Quit to DOS |
| Shift+W | Next waypoint |
| Shift+T | Time warp |
| Shift+A | HUD overlay |
| Shift+S | Skip to next event |
| Shift+J | Jettison tanks |
| Shift+L | Refuelling HUD mode |
| Shift+C | Cycle aircraft being browsed |
| Shift+B | Deploy brake chute |
| Shift+, / Shift+. | Left / right toe brake |

## Alt combinations (changed assignments)

| Key | Game action |
|---|---|
| Alt+W | Previous waypoint |
| Alt+R | Select VGA/SVGA display mode |
| Alt+A | Autopilot modes |
| Alt+D | Select graphics detail |
| Alt+H | Declutter HUD |
| Alt+J | Drop all stores |
| Alt+B | Retract brake chute |

The chart leaves the remaining letter/function key functions unchanged when
Shift or Alt is held, unless specifically highlighted otherwise.

## Extended keyboard and numeric keypad

The keypad list assumes Num Lock is in the game's expected control state.
The chart labels physical positions, not necessarily the character produced
by a modern keyboard layout.

| Key | Game action |
|---|---|
| Insert | Release chaff |
| Delete | Release flares |
| Keypad / | Afterburner off |
| Keypad * | Afterburner on |
| Keypad - / + | Throttle down / up |
| Keypad 7 / 9 | Scroll view left / right |
| Keypad 8 | HUD view |
| Keypad 4 | Warning panel |
| Keypad 5 | IRST |
| Keypad 6 | Horizon and compass |
| Keypad 1 / 2 / 3 | MFD 1 / MFD 2 / MFD 3 |
| Keypad 0 | Briefing summary |
| Keypad . | Game map |
| Keypad Enter | Air-to-air weapons |
| Arrow keys | Cursor movement |
| Shift+Keypad 7 / 1 | Zoom in / out |
| Shift+arrow keys | Cockpit scroll (up/left/down/right) |

The original chart does not annotate actions for Home, End, Page Up/Down,
Print Screen, Scroll Lock, Pause/Break, or Num Lock in the extended view.
Do not infer that they are necessarily unused in every version.

## Ctrl and MFD keyboard

The chart marks the following as Ctrl-modified MFD controls:
`Ctrl+Q`, `Ctrl+E`, `Ctrl+I`, `Ctrl+O`, `Ctrl+P`, `Ctrl+A`,
`Ctrl+D`, `Ctrl+F`, `Ctrl+G`, `Ctrl+H`, `Ctrl+J`, `Ctrl+K`, `Ctrl+L`,
`Ctrl+C`, `Ctrl+B`, `Ctrl+N`, and `Ctrl+M`.

The scan identifies these as MFD shortcuts but **does not label individual
actions**. A full action-by-action description here would be invented without
the main manual, a working game UI trace or disassembly of the target's input
dispatch. These remain a specific reverse-engineering task.

## What reaches the current Linux port

- `tools/input_devices.py` converts SDL scancodes to PC/AT set-1 make/break
  bytes, including `E0` extended sequences and the special Print Screen and
  Pause encodings. It also builds the BIOS-style key buffer and shift flags.
- `runtime/startup.c` provides keyboard ports 0x60/0x64 and IRQ1 delivery;
  `tools/real_mode.py` handles BIOS INT 16h functions 00h/01h/02h/10h/11h.
  This is **transport**: none of the in-game flight commands above are
  individually reimplemented as host shortcuts.
- **F10 is intercepted by the host** to open/close the debug overlay: the
  original game's F10 padlock-view action cannot receive it.
- **F11 is intercepted by the host** for fullscreen. The chart shows no F11
  flight action, but mods or other versions might use it.
- While the overlay is open, it consumes all keyboard input; arrows move the
  selection, Enter/Space activate, Escape backs out, and F10 closes it.
- Escape outside the overlay is passed to the original game; holding it for
  three seconds also releases host mouse capture. The chart's eject sequence
  is Shift+Escape, not an ordinary press of Escape.
- Other key actions, including Alt+R VGA/SVGA and Alt+D detail, should
  normally be transmitted as scan codes. **They have not been individually
  validated inside the running original game.**

Potential follow-up: make F10 padlock views accessible via an alternate
host-configurable binding (or reserve F10 for the guest with another overlay
hotkey). Verify the game's control set via a private Xvfb/gameplay trace;
no input should be injected on the user's desktop session.
