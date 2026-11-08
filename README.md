# Predictive Input Buffer (Geode mod for Geometry Dash 2.2081)

> **Practice and accessibility assist.** It changes *when* a press you really made
> is delivered. **Turn it off (Mode 1) for any run you plan to submit for
> verification**, and check the rules of any leaderboard or list you play on.

A press that arrives while the player cannot act (a cube still in mid-air, an orb
not yet touched) is remembered. If the player can act within the buffer window,
exactly **one** press is delivered on the first tick it can. Otherwise the press
expires and is thrown away. Level data, physics values, hitboxes and the game's
own update timing are never touched.

## Status (read this first)

| Item | State |
|---|---|
| Buffering logic (`src/BufferCore.hpp`) | **Unit-tested** off-game: ~1.8M checks incl. a randomized test against an independent reference model (`sh tests/run_tests.sh`) |
| Hook code (`src/main.cpp`) | Syntax/type-checked against *stand-ins* for the Geode API. **Not compiled against the real SDK and not run inside Geometry Dash.** |
| `.geode` package | **Not built.** It needs the Geode SDK + CLI + game bindings; build it with the steps below or the included GitHub Actions workflow. |
| In-game acceptance tests | **Not run.** Use `TESTING.md` (a step-by-step matrix with a debug log to confirm window sizes). |

Treat v1.0.0 as "ready to compile and test", not "verified".

## Build

Requirements: Geode CLI, Geode SDK **v5.x** (v5.11.2 was current when this was written; v5 targets GD 2.2081), CMake 3.21+, a C++20 compiler (MSVC 2022 / clang-cl on Windows).

```sh
geode sdk install            # or: geode sdk update   (make sure it is a v5.x SDK)
geode sdk install-binaries   # game bindings for your platform
geode build                  # configures, compiles and packages
```

The package is written to `build/yourname.input-buffer.geode`.
If your SDK version differs from `"geode": "5.11.2"` in `mod.json`, set that field to the SDK version you build with.

**No local setup?** Push this folder to GitHub and run the *Build Geode Mod* workflow
(`.github/workflows/build.yml`); the `.geode` is in the "Build Output" artifact. The workflow builds Windows only,
the platform the hooks were written for.

Before publishing, change `id`, `developer` and `repository` in `mod.json` from the `yourname` placeholders.

## Install

Copy the `.geode` file into `<Geometry Dash folder>/geode/mods/` and restart the game. Requires Geode v5 and GD 2.2081.
Geode's mod list should show **Predictive Input Buffer** with no missing dependencies; the mod has none.

## The four modes

| # | Mode | Window | At 240 ticks/s |
|---|---|---|---|
| 1 | Off (default) | none, input is vanilla | n/a |
| 2 | Low | up to **4** ticks | ~17 ms |
| 3 | Medium | up to **6** ticks | ~25 ms |
| 4 | High | up to **10** ticks | ~42 ms |

A **tick** is one physics step of the game (one non-half-tick call of `GJBaseGameLayer::processCommands`).
The window is counted in ticks, never in wall-clock time or rendered frames, so it is the same at any FPS.
The tick rate is **not hardcoded**: the mod counts the ticks the game actually runs and uses the game's own step delta only
to print the millisecond estimates. *If you meant 4/6/10 frames at 60 FPS rather than ticks, multiply the constants by 4 (see tuning).*

A press stays eligible for `window` ticks after it arrived: a press at tick T can fire on ticks T+1 ... T+window.

## Using it

* **Settings page** (Geode mod list > Predictive Input Buffer > settings): *Buffer Mode* is a selector whose
  labels include the buffer size, e.g. `3 - Medium (6 ticks)`. The choice is saved by Geode and persists between sessions.
* **Pause menu**: a button at the bottom shows the live readout, e.g. `Input Buffer: 3 - Medium (6 ticks, ~25 ms)`; click it to cycle modes.
* **Hotkey** (default **Shift+B**, rebindable, supports controller buttons): cycles Off > Low > Medium > High during a level and shows a brief toast (can be turned off).
* **HUD indicator** (off by default): a small `BUFFERED` label while a press is waiting.
* **Debug logging** (off by default): logs each buffered press and fire with its tick numbers, plus a ticks-per-second measurement every 1000 ticks.

Accessibility: standard Geode/GD widgets and fonts, high-contrast HUD text, a remappable hotkey (keyboard, mouse or controller),
and a toast that is not the only way to see the state (the pause button and the settings page show it too).
The pause-menu button is a plain menu item; if your controller setup cannot focus it, use the hotkey instead.

## Tuning the buffer values

Everything lives in **`src/BufferConfig.hpp`**:

```cpp
inline constexpr int kLowTicks    = 4;
inline constexpr int kMediumTicks = 6;
inline constexpr int kHighTicks   = 10;
```

After editing, update the matching labels in `mod.json` (`buffer-mode` > `one-of`, plus the description text), then run
`python3 tools/check_mod_json.py`. It fails if the labels and constants disagree, so the UI can't lie about the window size.
The default mode is `kDefaultMode` (and `"default"` in `mod.json`).

## How it works

* `GJBaseGameLayer::handleButton` is **always forwarded unchanged**. Around it, the mod notes which player's held state changed,
  so P1 / P2 / mirrored-dual routing is decided by the game, never re-implemented. Each player has its own slot.
* `GJBaseGameLayer::processCommands` runs once per tick. Before the game processes the tick, each slot is asked: expired? able to act now?
  If a live press can act, one `PlayerObject::pushButton(Jump)` is delivered and the slot is emptied.
* "Can act" (`src/PlayerProbe.hpp`): touching an orb, or standing on a surface for cube / ball / robot / spider. Ship, UFO, wave and swing copter
  always act on a press, so nothing is ever buffered there. Pads fire automatically on contact and need no press.
* If your finger was already up when the press fires, the original hold length is replayed (minimum 1 tick), so a robot jump keeps the height you asked for and no phantom hold is left.
* A safety net hooks `PlayerObject::incrementJumps`: if the player acts by any other route while a press is armed, the press is dropped (no double fire).
* Reset on death, restart, pause, level complete and exit; a replayed hold is released first. Off adds no behaviour: the hooks forward straight to the originals.
* Touch/hold triggers in levels see only your real presses, exactly like vanilla; the delayed press goes straight to the player object.

## Known limitations

See `changelog.md` for the short list. Highlights: nothing has been run in-game yet; the two hook points
(`handleButton(bool,int,bool)`, `processCommands(float,bool,bool)`) assume one non-half-tick `processCommands` call per physics step, which the debug log lets you confirm;
orbs are not buffered while in ship/UFO/wave/swing; platformer left/right is not buffered; the level editor playtest is not covered.

## Layout

```
src/BufferConfig.hpp   modes + tick constants (single source of truth)
src/BufferCore.hpp     pure buffering state machine (unit-tested)
src/PlayerProbe.hpp    "can this player act?" (reads game state only)
src/main.cpp           hooks, settings, hotkey, pause button, HUD
tests/                 off-game unit tests (sh tests/run_tests.sh)
tools/check_mod_json.py  labels <-> constants consistency check
TESTING.md             in-game acceptance matrix
```
