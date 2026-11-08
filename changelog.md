# v1.1.0

* New modes: **5 - Extreme** (20 ticks) and **6 - Ultimate** (45 ticks). The hotkey and pause button now cycle through all six modes.
* New constants `kExtremeTicks` / `kUltimateTicks` in `src/BufferConfig.hpp`; `mod.json` labels and tests updated.
* Extreme and Ultimate are very forgiving (about 83 ms and 188 ms at 240 ticks/s) and can fire a jump you no longer meant. Not recommended for verified runs.
* Settings saved under v1.0.0 keep working (labels for modes 1-4 are unchanged).
* Fixes from the first CI builds: per-platform `gd` object in `mod.json`, 3-argument `processCommands` (half ticks are not counted), typed `listenForSettingChanges`.

# v1.0.0

* Initial release.
* Four buffer modes: Off, Low (4 ticks), Medium (6), High (10). Constants in `src/BufferConfig.hpp`.
* Per-player buffer for single- and two-player (including mirrored dual mode).
* Exactly one delayed press per buffered press; expired presses are never fired.
* Original hold length is replayed when the press was already released (robot jump height preserved).
* Reset on death, restart, pause, level complete and exit.
* Settings page selector, pause-menu button with live readout, rebindable hotkey with toast, optional HUD indicator, optional debug log.

## Known limitations

* **Not yet verified in-game.** The buffering logic is unit-tested; the hooks were only type-checked against API stand-ins. Use `TESTING.md`.
* Assumes non-half-tick `GJBaseGameLayer::processCommands` calls happen once per physics step (half ticks are ignored). Enable *Debug Logging* and check that it reports about 240 ticks per second.
* Ship, UFO, wave and swing copter always act on a press, so there is nothing to buffer. Orb presses made in those modes are not buffered.
* Pads trigger automatically in GD (no press), so they need no buffering.
* After an orb has been used, a second press while still overlapping it is treated as "can act" (same as vanilla), not buffered.
* One buffered press per player: a newer press that also cannot act replaces the older one.
* Platformer left/right inputs are not buffered; the level editor playtest is not covered.
* Interaction with other input-timing mods (click-between-frames style) is untested; they hook the same functions.
* The pause-menu button may not be focusable with a controller; the hotkey accepts controller buttons.
* Windows is the platform the hooks were written for; other platforms are unverified.
* Geode's mod index may require a `cheat` tag for gameplay-advantage mods; check the current index rules before submitting.
